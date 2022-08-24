/// \file  File.h
/// \brief	Interface of the class File.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH
#pragma once

#include <string>	// std::string
#include <memory>	// std::shared_ptr
#include <vector>	// std::vector
#include <cstdint>	// std::uint32_t

#include <stdio.h>	// FILE

#include "MappedFile.h"

namespace smart {

// Forward declaration, if needed.
class MappedFile;

/// File abstraction.
class File {
public:
		/// File handle.
#if defined(WIN32)
		typedef void*		Handle;
		static constexpr Handle NullHandle = reinterpret_cast<Handle>(0);
#else
		typedef	int			Handle;
		static constexpr Handle NullHandle = reinterpret_cast<Handle>(-1);
#endif

		/// A wrapper around FILE; closes automatically when going out of scope.
		/// createMapping is not supported.
		File(FILE* f);

		/// A wrapper around file_handle; closes automatically when going out of scope.
		File(Handle file_handle);

		/// Open a file in read-write mode. createMapping supported.
		File(const std::string& filename);

		/// Open a file in read-write mode. createMapping supported.
		File(const char* s);

		/// Close outstanding file handles.
		~File();

		/// Get file handle.
		Handle getHandle();

#if !defined(WIN32)
		MappedFile*	createMapping(const std::uintptr_t offset, const std::size_t size);
#endif

public:
		/// Does the file or directory exist?
		static bool exists(const char* path);

		/// \return true if the file exists and is file; it must not be a link
		static bool existsAndIsFile(const char* path);

		/// Read all bytes from a file.
		static void readAllBytes(std::vector<std::uint8_t>& buffer, const std::string& filename);

		/// Write all bytes to the file; throw exceptions on errors.
		static void writeAllBytes(const std::string& filename, FILE* f, const void* data, const unsigned int size);

		/// Write all bytes to a file.
		static void writeAllBytes(const std::string& filename, const std::vector<std::uint8_t>& data);

		/// Write all bytes to a file.
		static void writeAllBytes(const std::string& filename, const void* data, const unsigned int size);

		/// Read file contents as text.
		static std::string readAllText(const std::string& filename);

		/// Write all of the string to a file.
		static void writeAllText(const std::string& filename, const std::string& contents);

		/// Read file contents as text lines. Both CRLF and LF line separators are supported.
		static void readAllLines(std::vector<std::string>& lines, const std::string& filename);

		/// Delete the given file.
		/// @param filename File to be deleted.
		static void deleteFile(const char* filename);

		/// Rename a file.
		static void renameFile(const char* oldPath, const char* newPath);

		/// Copy a file.
		/// \param	sourceFilepath		Source file path.
		/// \param	destinationFilepath	Destination file path
		static void copyFile(const char* sourceFilepath, const char* destinationFilepath);
private:
		Handle const	_handle;

		// C standard library file handle.
		FILE* const		_cfile;
		std::vector<std::shared_ptr<MappedFile>>	_mappings;
};

} // namespace smart
