/// \file  MappedFile.cpp
/// \brief	Interface of the class MappedFile.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>		// Win32 API
#else
#	include <sys/time.h>
#endif

#include <string>		// std::string
#include <stdexcept>	// std::runtime_error
#include <cstdint>		// std::uintptr_t, etc.

#include <stdio.h>
#if !defined(WIN32)
#include <unistd.h>	// open
#endif
#include <fcntl.h>	// file open flags.
#if !defined(WIN32)
#include <sys/mman.h>	// mmap
#endif

#include "string.h"

#include "MappedFile.h"	// ourselves.

#ifdef SMART_REGISTER_TRACE
#	include <atomic>	// std::atomic
#	include <chrono>	// std::chrono::steady_clock
#endif

#if defined(WIN32)
#	define	MAP_FAILED				((std::uint32_t*)(-1))
#	define	O_SYNC					0
#	define	PROT_READ				0
#	define	PROT_WRITE				0
#	define	MAP_SHARED				0
#	define	_SC_PAGE_SIZE			0
#	define	mmap(p1,p2,p3,p4,p5,p6)	(-1)
#	define	munmap(p1,p2)			(-1)
#	define	open(fn, mode)			(INVALID_HANDLE_VALUE)
#	define	sysconf(p1)				(-1)
#else
#	define	CloseHandle(fh)		close(fh)
#	define	INVALID_HANDLE_VALUE	(-1)
#	define	HANDLE					MappedFile::Handle
#endif

namespace smart {

unsigned int MappedFile::_pageSize = 0;

#ifdef SMART_REGISTER_TRACE
namespace {

struct TraceHandlerHolder {
	RegisterTraceHandler	handler;
	void*			user;
};

// The handler can be swapped while other threads access registers. A holder is immutable once
// published and never freed (a reader may still be using the old one); that leaks a few bytes per
// setTraceHandler() call, which is fine for a debug facility. (GCC 4.9 on the target has no
// std::atomic_load for shared_ptr.)
std::atomic<const TraceHandlerHolder*>	g_traceHandler(nullptr);

// Scope stack of the calling thread. Deeper nesting than the stack is counted but not stored,
// so current() then reports the innermost scope that fitted.
const unsigned int	SCOPE_STACK_SIZE = 16;
__thread const char*	t_scopes[SCOPE_STACK_SIZE];
__thread unsigned int	t_scopeDepth = 0;

void traceAccess(
	const RegisterAccess::Kind	kind,
	const std::string&		filename,
	const std::uintptr_t		offset,
	const std::uint32_t		index,
	const std::uint32_t		value,
	const std::uint32_t		mask,
	const bool			fromShadow)
{
	const TraceHandlerHolder* const holder = g_traceHandler.load(std::memory_order_acquire);
	if (holder) {
		const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
		const RegisterAccess a = { kind, filename, offset, index, value, mask, fromShadow, static_cast<std::uint64_t>(us),
			RegisterTraceScope::current() };
		holder->handler(a, holder->user);
	}
}

} // anonymous namespace

RegisterTraceScope::RegisterTraceScope(const char* name)
{
	if (t_scopeDepth < SCOPE_STACK_SIZE) {
		t_scopes[t_scopeDepth] = name;
	}
	++t_scopeDepth;
}

RegisterTraceScope::~RegisterTraceScope()
{
	--t_scopeDepth;
}

const char* RegisterTraceScope::current()
{
	if (t_scopeDepth == 0) {
		return nullptr;
	}
	return t_scopes[(t_scopeDepth < SCOPE_STACK_SIZE ? t_scopeDepth : SCOPE_STACK_SIZE) - 1];
}

void MappedFile::setTraceHandler(RegisterTraceHandler handler, void* user)
{
	const TraceHandlerHolder* holder = nullptr;
	if (handler) {
		holder = new TraceHandlerHolder{ handler, user };
	}
	g_traceHandler.store(holder, std::memory_order_release);
}

#	define SMART_REGISTER_TRACE_EVENT(...)	traceAccess(__VA_ARGS__)
#else
#	define SMART_REGISTER_TRACE_EVENT(...)	((void)0)
#endif

// --------------------------------------------------------------------------------------------------------------------
MappedFile::MappedFile(
	const std::string&		filename,
	const std::uintptr_t		offset,
	const std::size_t		length)
:
	_file(INVALID_HANDLE_VALUE),
	_data((std::uint32_t*)MAP_FAILED),
	_size(length),
	_shadow_read_mask(0),
	filename(filename),
	offset(offset)
{
	_file = open(filename.c_str(), O_RDWR|O_SYNC);
	if (_file == INVALID_HANDLE_VALUE) {
		_closeMapping();
		throw std::runtime_error("MappedFile: Unable to open file");
	}
	_data = reinterpret_cast<std::uint32_t*>(mmap(0, _size, PROT_READ|PROT_WRITE, MAP_SHARED, _file, offset));
	if (_data == MAP_FAILED) {
		_closeMapping();
		throw std::runtime_error("MappedFile: Unable to mmap file.");
	}
}

// --------------------------------------------------------------------------------------------------------------------
MappedFile::MappedFile(Handle h,
		const std::uintptr_t		offset,
		const std::size_t		length)
:
	_file(INVALID_HANDLE_VALUE),
	_data((std::uint32_t*)MAP_FAILED),
	_size(length),
	_shadow_read_mask(0),
	filename(ssprintf("filehandle:%d", h)),
	offset(offset)
{
	_data = reinterpret_cast<std::uint32_t*>(mmap(0, _size, PROT_READ|PROT_WRITE, MAP_SHARED, h, offset));
	if (_data == MAP_FAILED) {
		_closeMapping();
		throw std::runtime_error("MappedFile: Unable to mmap file.");
	}
}

// --------------------------------------------------------------------------------------------------------------------
void
MappedFile::_closeMapping()
{
	if (_data != nullptr) {
		munmap(_data, _size);
		_data = nullptr;
	}
	if (_file != INVALID_HANDLE_VALUE) {
		CloseHandle((HANDLE)_file);
		_file = INVALID_HANDLE_VALUE;
	}

}

// --------------------------------------------------------------------------------------------------------------------
std::uint32_t MappedFile::_read32Unchecked(const std::uint32_t index)
{
	if (index < _shadow_data.size() && (index>=8*sizeof(_shadow_read_mask) || (_shadow_read_mask & (1 << index))==0)) {
		return _shadow_data[index];
	}
	else {
		const auto r = reinterpret_cast<volatile std::uint32_t*>(_data)[index];
		if (index < _shadow_data.size()) {
			_shadow_data[index] = r;
		}
		return r;
	}
}

// --------------------------------------------------------------------------------------------------------------------
MappedFile::~MappedFile()
{
	_closeMapping();
}

// --------------------------------------------------------------------------------------------------------------------
std::size_t MappedFile::size() const
{
	return _size;
}

// --------------------------------------------------------------------------------------------------------------------
std::size_t MappedFile::size32() const
{
	return _size / sizeof(std::uint32_t);
}

// --------------------------------------------------------------------------------------------------------------------
void
MappedFile::createShadow(const unsigned int readMask)
{
	_shadow_data.resize(size());
	_shadow_read_mask = readMask;
}

// --------------------------------------------------------------------------------------------------------------------
void MappedFile::write32(const std::uint32_t index, const std::uint32_t value)
{
	if (index>_size || index*sizeof(std::uint32_t)>_size) {
		throw std::runtime_error("MappedFile::write32: index out of bounds");
	} else {
		_data[index] = value;
		if (index < _shadow_data.size()) {
			_shadow_data[index] = value;
		}
		SMART_REGISTER_TRACE_EVENT(RegisterAccess::WRITE, filename, offset, index, value, 0xFFFFFFFFu, false);
	}
}

// --------------------------------------------------------------------------------------------------------------------
void MappedFile::write32Masked(const std::uint32_t index, const std::uint32_t mask, const std::uint32_t value)
{
	if (index>_size || index*sizeof(std::uint32_t)>_size) {
		throw std::runtime_error("MappedFile::write32: index out of bounds");
	} else {
		const std::uint32_t	v_old = _read32Unchecked(index);
		const std::uint32_t	v_new = (v_old & ~mask) | (value & mask);

		_data[index] = v_new;
		if (index < _shadow_data.size()) {
			_shadow_data[index] = v_new;
		}
		SMART_REGISTER_TRACE_EVENT(RegisterAccess::WRITE_MASKED, filename, offset, index, v_new, mask, false);
	}
}

// --------------------------------------------------------------------------------------------------------------------
uint32_t MappedFile::read32(const std::uint32_t index)
{
	if (index>_size || index*sizeof(std::uint32_t)>_size) {
		throw std::runtime_error("MappedFile::read32: index out of bounds");
	} else {
#ifdef SMART_REGISTER_TRACE
		// Same condition as in _read32Unchecked(): was the value served from the shadow copy?
		const bool fromShadow = index < _shadow_data.size()
			&& (index >= 8*sizeof(_shadow_read_mask) || (_shadow_read_mask & (1 << index)) == 0);
		const std::uint32_t r = _read32Unchecked(index);
		SMART_REGISTER_TRACE_EVENT(RegisterAccess::READ, filename, offset, index, r, 0xFFFFFFFFu, fromShadow);
		return r;
#else
		return _read32Unchecked(index);
#endif
	}
}

// --------------------------------------------------------------------------------------------------------------------
MappedFile::Handle MappedFile::getFile()
{
	return _file;
}

// --------------------------------------------------------------------------------------------------------------------
unsigned int
MappedFile::pageSize()
{
	if (_pageSize == 0) {
		_pageSize = sysconf(_SC_PAGE_SIZE);
	}
	return _pageSize;
}

} // namespace smart
