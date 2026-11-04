/// \file  MappedFile.h
/// \brief	Interface of the class MappedFile.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH
#pragma once

#include <cstdint>	// std::int32_t, etc.
#include <string>	// std::string
#include <vector>	// std::vector
#include <stdexcept>	// std::runtime_error
#include <cstdint>	// std::uintptr_t, etc.
#include <cstddef>	// std::size_t

#include "File.h"

namespace smart {

#ifdef SMART_REGISTER_TRACE
/// One traced register access. Only available when built with -DSMART_REGISTER_TRACE.
struct RegisterAccess {
	enum Kind {
		READ,			///< read32(); value is the value returned.
		WRITE,			///< write32(); value is the value written.
		WRITE_MASKED		///< write32Masked(); value is the resulting word written, mask the bits that were requested.
	};

	Kind			kind;
	/// Identity of the mapped area (see MappedFile::filename / MappedFile::offset).
	const std::string&	filename;
	std::uintptr_t		offset;
	/// 32-bit word index into the mapped area.
	std::uint32_t		index;
	std::uint32_t		value;
	/// Bits that were written for WRITE_MASKED; all ones otherwise.
	std::uint32_t		mask;
	/// READ only: the value came from the shadow copy, the hardware was not touched.
	bool			fromShadow;
	/// Monotonic timestamp in microseconds.
	std::uint64_t		timestampUs;
	/// Innermost RegisterTraceScope active on the accessing thread, or nullptr (must outlive the call only).
	const char*		scope;
};

/// Callback for traced register accesses.
/// Called on the thread that does the access, after the access has completed; it must be
/// thread-safe and fast, and must not access any MappedFile (that would recurse).
typedef void (*RegisterTraceHandler)(const RegisterAccess& access, void* user);

/// Names the code region the current thread is in, so that register accesses can be attributed to
/// it and filtered by it (e.g. "poll", "setup"). Scopes nest; the innermost one wins and is reported
/// in RegisterAccess::scope. The name must be a string literal (or otherwise outlive the scope) and
/// should not contain whitespace. Use through SMART_REGISTER_TRACE_SCOPE().
class RegisterTraceScope {
public:
	explicit RegisterTraceScope(const char* name);
	~RegisterTraceScope();

	RegisterTraceScope(const RegisterTraceScope&) = delete;
	RegisterTraceScope& operator=(const RegisterTraceScope&) = delete;

	/// Innermost active scope of the calling thread, or nullptr.
	static const char* current();
};

#	define SMART_REGISTER_TRACE_CONCAT2(a, b)	a##b
#	define SMART_REGISTER_TRACE_CONCAT(a, b)	SMART_REGISTER_TRACE_CONCAT2(a, b)
/// Attribute the register accesses made until the end of the enclosing block to the scope "name".
/// Use as a statement: SMART_REGISTER_TRACE_SCOPE("poll");
#	define SMART_REGISTER_TRACE_SCOPE(name)	::smart::RegisterTraceScope SMART_REGISTER_TRACE_CONCAT(smart_register_trace_scope_, __LINE__)(name)
#else
#	define SMART_REGISTER_TRACE_SCOPE(name)	((void)0)
#endif // SMART_REGISTER_TRACE

/// Memory-mapped file.
/// NB! 32-bit access granularity.
/// Supports shadow copy of the mapped area in order to simplify register access.
class MappedFile {
public:
	/// File handle.
#if defined(WIN32)
	typedef void*		Handle;
#else
	typedef	int			Handle;
#endif

private:
	/// Underlying file handle, if any.
	Handle				_file;
	std::uint32_t*		_data;
	std::size_t			_size;

	/// Shadow copy of the data, if any.
	std::vector<std::uint32_t>		_shadow_data;

	/// Bitmask of registers that support reading.
	unsigned int					_shadow_read_mask;

	/// Size of the memory page.
	static unsigned int		_pageSize;

	/// Close everything.
	void				_closeMapping();

	/// Unchecked read.
	std::uint32_t		_read32Unchecked(const std::uint32_t index);
public:
	/// Create new memory mapping.
	/// NB! Use the pageSize() function to determine granularity.
	/// filename		Name of the operating system file.
	/// offset			Offset to the file, must be aligned to memory page size.
	/// length			Length of the mapped size, must be aligned to memory page size.
	MappedFile(
		const std::string&		filename,
		const std::uintptr_t		offset,
		const std::size_t		length);

	/// Create new memory mapping.
	/// NB! The file handle will not be owned by the file mapping.
	/// h				Operating system file handle.
	/// offset			Offset to the file, must be aligned to memory page size.
	/// length			Length of the mapped size, must be aligned to memory page size.
	MappedFile(Handle h,
			const std::uintptr_t		offset,
			const std::size_t		length);

	/// Close memory mapping.
	~MappedFile();

	const std::string	filename;
	const std::uintptr_t	offset;

	/// Number of bytes mapped.
	std::size_t size() const;

	/// Number of 32-bit words mapped.
	std::size_t size32() const;

	/// Create shadow copy of the data.
	/// Reads will be done from the shadow copy except for those registers that have a corresponding bit set in the readMask.
	void createShadow(const unsigned int readMask);

	/// Write the given word at the given index.
	void write32(const std::uint32_t index, const std::uint32_t value);

	/// Write the given ward at the given index with the given mask.
	/// Only the bits given in the mask will be written.
	void write32Masked(const std::uint32_t index, const std::uint32_t mask, const std::uint32_t value);

	/// Read a word from the given index.
	uint32_t read32(const std::uint32_t index);

	/// Return the data pointer.
	void* data()
	{
		return _data;
	}

	/// Get file handle.
	Handle getFile();

	/// Size of a memory page.
	static unsigned int pageSize();

#ifdef SMART_REGISTER_TRACE
	/// Install the handler for all register accesses of all MappedFile instances.
	/// Pass nullptr to remove it. Compiled in only with -DSMART_REGISTER_TRACE.
	static void setTraceHandler(RegisterTraceHandler handler, void* user = nullptr);
#endif
}; // class MappedFile

} // namespace smart
