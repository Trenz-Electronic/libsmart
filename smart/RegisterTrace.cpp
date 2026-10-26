/// \file  RegisterTrace.cpp
/// \brief	Recorder for MappedFile register accesses (debug builds only).
///
/// \copyright	SPDX: BSD-3-Clause Trenz Electronic GmbH

#ifdef SMART_REGISTER_TRACE

#include <chrono>		// std::chrono
#include <cstdio>		// std::snprintf, std::rename, std::remove
#include <fstream>		// std::ofstream
#include <ostream>		// std::ostream
#include <thread>		// std::this_thread::sleep_for

#include <unistd.h>		// getpid

#include "RegisterTrace.h"	// ourselves.

namespace smart {

namespace {

std::uint64_t regKey(const std::uint32_t mapId, const std::uint32_t index)
{
	return (static_cast<std::uint64_t>(mapId) << 32) | index;
}

char kindChar(const RegisterAccess::Kind kind)
{
	switch (kind) {
	case RegisterAccess::READ:		return 'R';
	case RegisterAccess::WRITE:		return 'W';
	case RegisterAccess::WRITE_MASKED:	return 'M';
	}
	return '?';
}

// The recorder MappedFile currently reports to, if any.
RegisterTraceRecorder* g_installed = nullptr;

} // anonymous namespace

// --------------------------------------------------------------------------------------------------------------------
RegisterTraceRecorder::RegisterTraceRecorder(const std::size_t capacity)
:
	_capacity(capacity > 0 ? capacity : 1),
	_frontEntryNo(0),
	_nextSeq(0),
	_dropped(0),
	_version(0),
	_sinkPeriodMs(500),
	_sinkRunning(false),
	_sinkPid(0),
	_sinkWrittenVersion(0),
	_sinkStop(false)
{
}

// --------------------------------------------------------------------------------------------------------------------
RegisterTraceRecorder::~RegisterTraceRecorder()
{
	uninstall();
	stopFileSink();
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::record(const RegisterAccess& access)
{
	_restartSinkIfForked();

	std::lock_guard<std::mutex> lock(_mutex);

	// Resolve the map id.
	const std::string name = access.filename + "@" + std::to_string(access.offset);
	std::uint32_t mapId;
	const auto it = _mapIds.find(name);
	if (it == _mapIds.end()) {
		mapId = static_cast<std::uint32_t>(_mapNames.size());
		_mapNames.push_back(name);
		_mapIds.emplace(name, mapId);
	} else {
		mapId = it->second;
	}

	const std::uint64_t key = regKey(mapId, access.index);
	const std::uint64_t seq = _nextSeq++;
	++_version;

	if (access.kind == RegisterAccess::READ) {
		// Fold a repeated read of the same value into the previous read entry, if it is still in the ring.
		const auto last = _lastRead.find(key);
		if (last != _lastRead.end() && last->second >= _frontEntryNo) {
			Entry& e = _entries[static_cast<std::size_t>(last->second - _frontEntryNo)];
			if (e.value == access.value && e.fromShadow == access.fromShadow) {
				++e.count;
				e.lastUs = access.timestampUs;
				return;
			}
		}
	} else {
		// A write ends the run of reads of this register.
		_lastRead.erase(key);
	}

	Entry e;
	e.seq = seq;
	e.firstUs = access.timestampUs;
	e.lastUs = access.timestampUs;
	e.count = 1;
	e.kind = access.kind;
	e.mapId = mapId;
	e.index = access.index;
	e.value = access.value;
	e.mask = access.mask;
	e.fromShadow = access.fromShadow;

	if (_entries.size() >= _capacity) {
		_entries.pop_front();
		++_frontEntryNo;
		++_dropped;
	}
	_entries.push_back(e);
	if (access.kind == RegisterAccess::READ) {
		_lastRead[key] = _frontEntryNo + _entries.size() - 1;
	}
}

// --------------------------------------------------------------------------------------------------------------------
std::vector<RegisterTraceRecorder::Entry> RegisterTraceRecorder::snapshot() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return std::vector<Entry>(_entries.begin(), _entries.end());
}

// --------------------------------------------------------------------------------------------------------------------
std::string RegisterTraceRecorder::mapName(const std::uint32_t mapId) const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return mapId < _mapNames.size() ? _mapNames[mapId] : std::string();
}

// --------------------------------------------------------------------------------------------------------------------
std::uint64_t RegisterTraceRecorder::dropped() const
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _dropped;
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::clear()
{
	std::lock_guard<std::mutex> lock(_mutex);
	_frontEntryNo += _entries.size();
	_entries.clear();
	_lastRead.clear();
	++_version;
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::write(std::ostream& os) const
{
	std::lock_guard<std::mutex> lock(_mutex);

	os << "# regtrace v1: seq first_us last_us count kind map index value mask shadow\n";
	os << "# dropped " << _dropped << "\n";

	char line[512];
	for (const Entry& e : _entries) {
		char mask[16];
		if (e.kind == RegisterAccess::WRITE_MASKED) {
			std::snprintf(mask, sizeof(mask), "0x%08X", e.mask);
		} else {
			std::snprintf(mask, sizeof(mask), "-");
		}
		std::snprintf(line, sizeof(line), "%llu %llu %llu %u %c %s 0x%X 0x%08X %s %d\n",
			static_cast<unsigned long long>(e.seq),
			static_cast<unsigned long long>(e.firstUs),
			static_cast<unsigned long long>(e.lastUs),
			e.count,
			kindChar(e.kind),
			_mapNames[e.mapId].c_str(),
			e.index,
			e.value,
			mask,
			e.fromShadow ? 1 : 0);
		os << line;
	}
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::_handler(const RegisterAccess& access, void* user)
{
	static_cast<RegisterTraceRecorder*>(user)->record(access);
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::install()
{
	g_installed = this;
	MappedFile::setTraceHandler(&RegisterTraceRecorder::_handler, this);
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::uninstall()
{
	if (g_installed == this) {
		MappedFile::setTraceHandler(nullptr);
		g_installed = nullptr;
	}
}

// --------------------------------------------------------------------------------------------------------------------
bool RegisterTraceRecorder::startFileSink(const std::string& path, const unsigned int periodMs)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_sinkRunning) {
		return false;
	}
	_sinkPath = path;
	_sinkPeriodMs = periodMs > 0 ? periodMs : 1;
	_sinkStop = false;
	_sinkWrittenVersion = _version - 1;	// write the (maybe empty) file at least once
	_sinkRunning = true;
	_startSinkThread();
	return true;
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::stopFileSink()
{
	bool join;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_sinkRunning) {
			return;
		}
		_sinkStop = true;
		// After fork() the writer thread does not exist in the child; there is nothing to join.
		join = (_sinkPid == getpid());
	}
	if (join) {
		pthread_join(_sinkThread, nullptr);
	}
	_writeFile();
	std::lock_guard<std::mutex> lock(_mutex);
	_sinkRunning = false;
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::_startSinkThread()
{
	// Caller holds _mutex.
	_sinkPid = getpid();
	if (pthread_create(&_sinkThread, nullptr, &RegisterTraceRecorder::_sinkMain, this) != 0) {
		_sinkRunning = false;
	}
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::_restartSinkIfForked()
{
	// Unlocked pre-check keeps the hot path cheap; _sinkPid only changes under the lock.
	// (A stale read can only delay the restart by one access.)
	if (_sinkRunning && _sinkPid != getpid()) {
		std::lock_guard<std::mutex> lock(_mutex);
		if (_sinkRunning && _sinkPid != getpid()) {
			_startSinkThread();
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
void* RegisterTraceRecorder::_sinkMain(void* self)
{
	RegisterTraceRecorder* r = static_cast<RegisterTraceRecorder*>(self);
	const unsigned int stepMs = 20;
	for (;;) {
		for (unsigned int waited = 0; waited < r->_sinkPeriodMs; waited += stepMs) {
			{
				std::lock_guard<std::mutex> lock(r->_mutex);
				if (r->_sinkStop) {
					return nullptr;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
		}
		bool changed;
		{
			std::lock_guard<std::mutex> lock(r->_mutex);
			changed = r->_version != r->_sinkWrittenVersion;
		}
		if (changed) {
			r->_writeFile();
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
void RegisterTraceRecorder::_writeFile()
{
	std::uint64_t version;
	std::string path;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		version = _version;
		path = _sinkPath;
	}
	if (path.empty()) {
		return;
	}
	// Replace atomically so that readers never see a half-written file.
	const std::string tmp = path + ".tmp";
	{
		std::ofstream f(tmp.c_str(), std::ios::out | std::ios::trunc);
		if (!f) {
			return;
		}
		write(f);
		if (!f) {
			return;
		}
	}
	if (std::rename(tmp.c_str(), path.c_str()) == 0) {
		std::lock_guard<std::mutex> lock(_mutex);
		_sinkWrittenVersion = version;
	}
}

} // namespace smart

#endif // SMART_REGISTER_TRACE
