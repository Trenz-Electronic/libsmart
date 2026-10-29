/// \file  RegisterTrace.h
/// \brief	Recorder for MappedFile register accesses (debug builds only).
///
/// Everything in this file exists only when built with -DSMART_REGISTER_TRACE.
///
/// \copyright	SPDX: BSD-3-Clause Trenz Electronic GmbH

#pragma once

#ifdef SMART_REGISTER_TRACE

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint32_t, etc.
#include <atomic>		// std::atomic
#include <deque>		// std::deque
#include <iosfwd>		// std::ostream
#include <mutex>		// std::mutex
#include <string>		// std::string
#include <unordered_map>	// std::unordered_map
#include <vector>		// std::vector

#include <pthread.h>		// pthread_t
#include <sys/types.h>		// pid_t

#include "MappedFile.h"		// RegisterAccess

namespace smart {

/// Records the register accesses reported by MappedFile in a ring buffer.
///
/// Repeated reads of one register that return the same value (status polling) are
/// folded into a single entry with a repeat count, as long as the register has not been
/// written in between. The folded entry stays at the position of its first read, so
/// entries are ordered by the first access, not by the last one.
///
/// Accesses can be attributed to scopes (RegisterTraceScope) and filtered by them, see setScopeFilter().
///
/// Thread-safe. The recorder must outlive its installation (see install()).
class RegisterTraceRecorder {
public:
	struct Entry {
		/// Sequence number of the first access in this entry; strictly increasing over entries.
		std::uint64_t		seq;
		/// Timestamps (microseconds, monotonic) of the first and the last access folded into this entry.
		std::uint64_t		firstUs;
		std::uint64_t		lastUs;
		/// Number of accesses folded into this entry (1 if not folded).
		std::uint32_t		count;
		RegisterAccess::Kind	kind;
		/// Index into mapName().
		std::uint32_t		mapId;
		std::uint32_t		index;
		std::uint32_t		value;
		std::uint32_t		mask;
		bool			fromShadow;
		/// Index into scopeName(); 0 is "-" (no scope).
		std::uint32_t		scopeId;
	};

	/// capacity	Maximum number of entries kept; the oldest are dropped first.
	explicit RegisterTraceRecorder(std::size_t capacity = 16384);
	~RegisterTraceRecorder();

	RegisterTraceRecorder(const RegisterTraceRecorder&) = delete;
	RegisterTraceRecorder& operator=(const RegisterTraceRecorder&) = delete;

	/// Record one access. This is what the MappedFile handler calls.
	void record(const RegisterAccess& access);

	/// Copy of the entries currently kept, oldest first.
	std::vector<Entry> snapshot() const;

	/// "filename@offset" of a map id used in Entry::mapId.
	std::string mapName(std::uint32_t mapId) const;

	/// Name of a scope id used in Entry::scopeId; "-" for id 0 (accesses outside any scope).
	std::string scopeName(std::uint32_t scopeId) const;

	/// Number of entries dropped because the ring was full.
	std::uint64_t dropped() const;

	/// Number of accesses ignored by the scope filter.
	std::uint64_t filtered() const;

	/// Record only the accesses that pass this filter; the others are counted but not stored.
	/// spec is a comma-separated list of scope names. A name with a leading '-' excludes that scope,
	/// any other name (optionally with a leading '+') includes it; if there is at least one include,
	/// only included scopes are recorded. A name matches a scope with the same name and the scopes
	/// below it ("setup" matches "setup/blade"). "none" stands for accesses outside any scope.
	/// Examples: "-poll" everything except polling; "+setup,+blade" only configuration.
	/// An empty spec records everything (the default).
	void setScopeFilter(const std::string& spec);

	/// Remove all entries. Sequence numbers keep increasing.
	void clear();

	/// Write the entries as text, one line per entry:
	///   seq first_us last_us count kind map index value mask shadow scope
	/// kind is R, W or M (masked write); map is filename@offset; index, value and mask
	/// are hex (mask is "-" except for M); shadow is 1 for reads served from the shadow copy;
	/// scope is the scope name or "-".
	/// Lines starting with '#' are comments.
	void write(std::ostream& os) const;

	/// Make this recorder the one MappedFile reports to (replaces any other).
	void install();

	/// Stop MappedFile reporting to this recorder. A no-op if it is not the installed one.
	void uninstall();

	/// Keep the file at path up to date with write() output: it is replaced atomically
	/// every periodMs milliseconds while there is something new, and on stopFileSink().
	/// The writer survives fork() (e.g. daemonizing): it is restarted on the next access
	/// in the child. Returns false if a sink is already running.
	bool startFileSink(const std::string& path, unsigned int periodMs = 500);

	/// Write the file one last time and stop the writer.
	void stopFileSink();

private:
	static void		_handler(const RegisterAccess& access, void* user);
	static void*		_sinkMain(void* self);
	void			_writeFile();
	void			_restartSinkIfForked();
	void			_startSinkThread();

	const std::size_t			_capacity;

	mutable std::mutex			_mutex;
	std::deque<Entry>			_entries;
	std::uint64_t				_frontEntryNo;	// entry number of _entries.front()
	std::uint64_t				_nextSeq;
	std::uint64_t				_dropped;
	std::uint64_t				_filtered;
	std::uint64_t				_version;	// bumped on every change, for the file sink

	std::vector<std::string>		_scopeNames;
	std::unordered_map<std::string, std::uint32_t>	_scopeIds;
	std::string				_filterSpec;
	std::vector<std::string>		_filterInclude;
	std::vector<std::string>		_filterExclude;

	std::vector<std::string>		_mapNames;
	std::unordered_map<std::string, std::uint32_t>	_mapIds;
	/// (mapId << 32 | index) -> entry number of the last READ entry of that register.
	std::unordered_map<std::uint64_t, std::uint64_t>	_lastRead;

	std::string		_sinkPath;
	unsigned int		_sinkPeriodMs;
	std::atomic<bool>	_sinkRunning;	// atomics: read without the lock on the record() hot path
	std::atomic<pid_t>	_sinkPid;
	pthread_t		_sinkThread;
	std::uint64_t		_sinkWrittenVersion;
	bool			_sinkStop;
};

} // namespace smart

#endif // SMART_REGISTER_TRACE
