/// \file  Path.cpp
/// \brief	Definitions in the namespace Path.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#include <string.h>

#include "Path.h"


namespace smart {
namespace Path {

const char*	ALL_SEPARATORS =
#if defined(WIN32)
		"\\/";
#else
		"/";
#endif

const char* ILLEGAL_CHARACTERS_IN_FILENAME = ";\"&%(){}[]#=?*',><!|";

// --------------------------------------------------------------------------------------------------------------------
bool isSeparator(const char c)
{
	return strchr(ALL_SEPARATORS, c) != nullptr;
}

// --------------------------------------------------------------------------------------------------------------------
std::string
combine(const std::string& p1, const std::string& p2)
{
	if (p1.size() > 0) {
		std::string	r;
		r += p1;
		r += SEPARATOR;
		r += p2;
		return r;
	} else {
		return p2;
	}
}

// --------------------------------------------------------------------------------------------------------------------
std::string	combine(const std::string& p1, const std::string& p2, const std::string& p3)
{
	std::string	r = p1;

	if (r.size() > 0u && p2.size()>0u) {
		r += SEPARATOR;
		r += p2;
	}

	if (r.size() > 0u && p3.size() > 0u) {
		r += SEPARATOR;
		r += p3;
	}

	return r;
}

// --------------------------------------------------------------------------------------------------------------------
std::string getFilenameWoExt(const std::string& path)
{
	std::string::size_type sep_pos = std::string::npos;
	sep_pos = path.rfind('.');
	return path.substr(0, sep_pos );
}

// --------------------------------------------------------------------------------------------------------------------
std::string getFilename(const std::string& path)
{
	std::string::size_type sep_pos = std::string::npos;
	sep_pos = path.rfind(SEPARATOR);
	if (sep_pos == std::string::npos) {
#if defined(WIN32)
		if (path.size() >= 2 && isalpha(path[0]) && path[1] == ':') {
			return path.substr(2);
		}
#endif
		return path;
	}
	else {
		return path.substr(sep_pos + 1);
	}
}

// --------------------------------------------------------------------------------------------------------------------
std::string getDirectoryName(const std::string& path)
{
	size_t	n = path.size();
	bool	found_separator = false;

	// Search for the first separator backwards.
	while (n > 0u) {
		--n;
		if (strchr(ALL_SEPARATORS, path[n]) != nullptr) {
			found_separator = true;
			break;
		}
	}
	if (!found_separator) {
		return std::string();
	}

	// Skip all duplicate separators.
	while (n > 0u) {
		const size_t	n2 = n - 1u;
		if (strchr(ALL_SEPARATORS, path[n2]) == nullptr) {
			break;
		}
		n = n2;
	}
	return path.substr(0, n);
}

} // namespace Path
} // namespace smart
