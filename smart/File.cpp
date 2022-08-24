/// \file  File.cpp
/// \brief	Implementation of the class File.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>		// Win32 API
#endif

#include <stdexcept>	// std::runtime_error

#include <errno.h>
#include <fcntl.h>		// file open flags.
#include <stdio.h>		// FILE
#include <string.h>		// memcpy
#include <sys/types.h>
#include <sys/stat.h>	// stat
#if !defined(_WIN32)
#	include <unistd.h>		// open
#endif

#include "File.h"
#include "MappedFile.h"
#include "string.h"


#if defined(WIN32)
typedef signed int ssize_t;
#	define	O_SYNC					0
// --------------------------------------------------------------------------------------------------------------------
static smart::File::Handle open(const char* filename, const int flags)
{
	smart::File::Handle r;
	if (flags & O_WRONLY) {
		r = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, 0, NULL);
	}
	else if (flags & O_RDWR) {
		r = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, 0, NULL);
	}
	else {
		// must be read-only.
		r = CreateFile(filename, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	}
	return r;
}

// --------------------------------------------------------------------------------------------------------------------
static smart::File::Handle open(const char* filename, const int flags, const int mode)
{
	return open(filename, flags);
}

// --------------------------------------------------------------------------------------------------------------------
static ssize_t read(smart::File::Handle h, void *buf, size_t count)
{
	DWORD	bytes_read = 0;
	BOOL	ok = ::ReadFile(h, buf, count, &bytes_read, nullptr);
	if (ok) {
		return bytes_read;
	}
	return -1;
}

// --------------------------------------------------------------------------------------------------------------------
static ssize_t write(smart::File::Handle h, const void *buf, size_t count)
{
	DWORD	bytes_written = 0;
	BOOL	ok = ::WriteFile(h, buf, count, &bytes_written, nullptr);
	if (ok) {
		return bytes_written;
	}
	return -1;
}

#	define	close(h)				CloseHandle(h)
#	define	stat_f					_stat
#	define	lstat					_stat
#	define	stat_st					_stat64i32
#else
#	define	INVALID_HANDLE_VALUE	(-1)
#	define	stat_f					stat
#	define	stat_st					stat
#endif


namespace smart {

class StdioFile {
public:
	StdioFile(FILE* f) : _f(f) { }
	~StdioFile()
	{
		if (_f != nullptr) {
			fclose(_f);
		}
	}
private:
	FILE*	_f;
};

// --------------------------------------------------------------------------
static File::Handle __open(const char* filename, const int flags)
{
	const File::Handle h = open(filename, flags);
	if (h == INVALID_HANDLE_VALUE) {
		throw std::runtime_error(ssprintf("Cannot open file '%s'", filename));
	}
	return h;
}

// --------------------------------------------------------------------------------------------------------------------
File::File(FILE* f)
: _handle(INVALID_HANDLE_VALUE),
  _cfile(f)
{
}

// --------------------------------------------------------------------------
File::File(Handle file_handle)
	: _handle(file_handle),
	_cfile(nullptr)
{
}

// --------------------------------------------------------------------------
File::File(const std::string& filename)
: _handle(__open(filename.c_str(), O_RDWR|O_SYNC)),
  _cfile(nullptr)
{
}

// --------------------------------------------------------------------------
File::File(const char* filename)
: _handle(__open(filename, O_RDWR|O_SYNC)),
  _cfile(nullptr)
{
}

// --------------------------------------------------------------------------
File::~File()
{
	_mappings.clear();
	if (_handle != INVALID_HANDLE_VALUE) {
		close(_handle);
	}
	if (_cfile != nullptr) {
		fclose(_cfile);
	}
}

// --------------------------------------------------------------------------
File::Handle File::getHandle()
{
	if (_handle == INVALID_HANDLE_VALUE && _cfile != nullptr) {
#if defined(_MSC_VER)
		return reinterpret_cast<Handle>(_fileno(_cfile));
#else
		return fileno(_cfile);
#endif
	}
	return _handle;
}

#if !defined(WIN32)
// --------------------------------------------------------------------------
MappedFile* File::createMapping(const std::uintptr_t offset, const std::size_t size)
{
	auto r = std::make_shared<MappedFile>(getHandle(), offset, size);
	_mappings.push_back(r);
	return r.get();
}
#endif

// --------------------------------------------------------------------------
bool File::exists(const char* path)
{
	struct stat_st	st = { 0 };
	const int	r_stat = stat_f(path, &st);
	return r_stat == 0;
}

// --------------------------------------------------------------------------------------------------------------------
bool File::existsAndIsFile(const char* path)
{
	struct stat_st st;

	// the path must have characers
	if (path==nullptr || *path==0) {
		return false;
	}

	// the root directory must exist
	bool exists = false;
	if (lstat(path, &st) != -1) {
		//printf( "%s st_mode=%o\n", path.c_str(), st.st_mode );
		exists = (st.st_mode & S_IFREG) != 0;
#if !defined(_MSC_VER)
		exists &= ((st.st_mode & (S_IFLNK ^ S_IFREG) ) == 0);
#endif
	}

	return exists;
}

// --------------------------------------------------------------------------------------------------------------------
/// Read all bytes into the given buffer of uint8_t/unsigned char.
template <typename T>
static void read_all_bytes(T& buffer, const char* filename)
{
	struct stat_st st;
	if (stat_f(filename, &st)<0) {
		throw std::runtime_error(ssprintf("File '%s' not found", filename));
	}

	const int   file_size = st.st_size;
	buffer.resize(file_size);
	FILE*   fin = fopen(filename, "rb");
	if (fin == NULL) {
		throw std::runtime_error(ssprintf("Cannot open file '%s' for reading", filename));
	}

	StdioFile	fin_auto(fin);
	if (file_size>0) {
		// Read all in one chunk if possible.
		buffer.resize(file_size);
		const int   r = fread(&buffer[0], 1, file_size, fin);
		if (r < 0) {
			throw std::runtime_error(ssprintf("Error when reading file '%s'", filename));
		}
		buffer.resize(r);
		return;
	}

	// Read it bit by bit.
	const unsigned int	CHUNK_SIZE = 16384;
	unsigned int		so_far = 0;
	int					this_round;
	do {
		buffer.resize(so_far + CHUNK_SIZE);
		this_round = fread(&buffer[so_far], 1, CHUNK_SIZE, fin);
		if (this_round > 0) {
			so_far += this_round;
		}
	} while (this_round == CHUNK_SIZE);
	buffer.resize(so_far);
}

// --------------------------------------------------------------------------------------------------------------------
void File::readAllBytes(std::vector<std::uint8_t>& buffer, const std::string& filename)
{
	read_all_bytes(buffer, filename.c_str());
}

// --------------------------------------------------------------------------------------------------------------------
void
File::writeAllBytes(const std::string& filename, FILE* fout, const void* data, const unsigned int size)
{
	const int   r = fwrite(data, size, 1, fout);
	if (r != 1) {
		throw std::runtime_error(ssprintf("writeAllBytes: Cannot write %u bytes to file '%s'", size, filename.c_str()));
	}
}

// --------------------------------------------------------------------------------------------------------------------
void File::writeAllBytes(const std::string& filename, const std::vector<std::uint8_t>& data)
{
	writeAllBytes(filename, data.size()==0 ? nullptr : &data[0], data.size());
}

// --------------------------------------------------------------------------------------------------------------------
/// Write all bytes to a file.
void File::writeAllBytes(const std::string& filename, const void* data, const unsigned int size)
{
	FILE*   fout = fopen(filename.c_str(), "wb");
	if (fout == NULL) {
		throw std::runtime_error(ssprintf("Cannot open file '%s' for writing.", filename.c_str()));
	}
	else if (size == 0) {
		// simplest case.
		fclose(fout);
	}
	else {
		File	fp(fout);
		writeAllBytes(filename, fout, data, size);
	}
}

// --------------------------------------------------------------------------------------------------------------------
std::string File::readAllText(const std::string& filename)
{
	std::string r;
	read_all_bytes(r, filename.c_str());
	return r;
}

// --------------------------------------------------------------------------------------------------------------------
void File::writeAllText(const std::string& filename, const std::string& contents)
{
	std::vector<std::uint8_t>	buffer;
	if (contents.size() > 0) {
		buffer.resize(contents.size());
		memcpy(&buffer[0], contents.c_str(), contents.size());
	}
	writeAllBytes(filename, buffer);
}

// --------------------------------------------------------------------------------------------------------------------
static void
_append_line(
	std::vector<std::string>&			lines,
	const std::vector<std::uint8_t>&	buffer,
	const unsigned int					last_start_pos,
	const unsigned int					lf_pos
	)
{
	if (last_start_pos >= lf_pos) {
		if (lf_pos < buffer.size()) {
			lines.push_back("");
		}
	} else {
		const char*	start_ptr = (const char*)&buffer[last_start_pos];
		if (buffer[lf_pos - 1] == '\r') {
			lines.push_back(std::string(start_ptr, (const char*)&buffer[lf_pos - 1]));
		} else {
			lines.push_back(std::string(start_ptr, (const char*)(&buffer[0] + lf_pos)));
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
void
File::readAllLines(std::vector<std::string>& lines, const std::string& filename)
{
	std::vector<std::uint8_t>	buffer;
	readAllBytes(buffer, filename);

	unsigned int	last_start = 0;
	for (unsigned int lf_pos = 0; lf_pos < buffer.size(); ++lf_pos) {
		if (buffer[lf_pos] == '\n') {
			_append_line(lines, buffer, last_start, lf_pos);
			last_start = lf_pos + 1;
		}
	}
	_append_line(lines, buffer, last_start, buffer.size());
}

// --------------------------------------------------------------------------------------------------------------------
void
File::deleteFile(const char* filename)
{
#if defined(WIN32)
	DeleteFile(filename);
#else
	::unlink(filename);
#endif
}

// --------------------------------------------------------------------------------------------------------------------
void File::renameFile(const char* oldPath, const char* newPath)
{
#if defined(WIN32)
	if (File::exists(newPath)) {
		File::deleteFile(newPath);
	}
	BOOL ok = MoveFile(oldPath, newPath);
	if (!ok) {
#else
	const int r_rename = rename(oldPath, newPath);
	if (r_rename != 0) {
#endif
		throw std::runtime_error(ssprintf("renameFile: Cannot rename '%s' to '%s'", oldPath, newPath));
	}
}

// --------------------------------------------------------------------------------------------------------------------
void File::copyFile(const char* sourceFilepath, const char* destinationFilepath)
{
	uint8_t	buf[64 * 1024];
	ssize_t nread;

	File	fd_from(open(sourceFilepath, O_RDONLY));
	if (fd_from.getHandle() == INVALID_HANDLE_VALUE) {
		throw std::runtime_error(ssprintf("Cannot open file '%s' for reading.", sourceFilepath));
	}

	File	fd_to(open(destinationFilepath, O_WRONLY | O_CREAT	, 0666));
	if (fd_to.getHandle() == INVALID_HANDLE_VALUE) {
		throw std::runtime_error(ssprintf("Cannot open file '%s' for writing.", destinationFilepath));
	}

	while (nread = read(fd_from.getHandle(), buf, sizeof buf), nread > 0) {
		uint8_t *out_ptr = buf;
		do {
			ssize_t nwritten = write(fd_to.getHandle(), out_ptr, nread);
			if (nwritten >= 0) {
				nread -= nwritten;
				out_ptr += nwritten;
			}
#if defined(WIN32)
			else
#else
			else if (errno != EINTR)
#endif
			{
				throw std::runtime_error(ssprintf("copyFile: cannot write to '%s'", destinationFilepath));
			}
		} while (nread > 0);
	}
}

} // namespace smart.
