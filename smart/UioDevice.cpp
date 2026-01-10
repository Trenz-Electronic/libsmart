/// \file  UioDevice.cpp
/// \brief	Implementation of the class UioDevice.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#include "UioDevice.h"


#include <cstdint>		// std::uintptr_t
#include <memory>		// std::shared_ptr
#include <limits>		// std::limits
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string
#include <vector>		// std::vector

#include <ctype.h>		// isspace
#if !defined(WIN32)
#include <dirent.h>		// scandir
#include <errno.h>		// errno
#include <fcntl.h>		// open
#include <inttypes.h>		// PRIuPTR
#include <unistd.h>		// getpagesize, close
#include <string.h>		// strcasecmp
#include <sys/ioctl.h>		// ioctl
#include <sys/types.h>	// stat
#include <sys/stat.h>	// stat
#endif

#include "Directory.h"		// Directory::getFiles
#include "File.h"			// File::readAllText
#include "MappedFile.h"		// MappedFile
#include "Path.h"
#include "string.h"			// ssprintf

#include "UioDevice.h"		// ourselves.

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif

namespace smart {

// --------------------------------------------------------------------------------------------------------------------
// Cache sync ioctl definitions (from smartio.h)
#if !defined(WIN32)
#define SMARTIO_IOC_MAGIC 'S'

struct smartio_cache_op {
	std::uint64_t offset;
	std::uint64_t size;
};

#define SMARTIO_SYNC_FOR_CPU  _IOW(SMARTIO_IOC_MAGIC, 1, struct smartio_cache_op)
#endif

/// Prefix of the UIO path.
#define	UIO_PATH		"/sys/class/uio"
#define	UIO_PATH_PREFIX	UIO_PATH	"/uio"

// -------------------------------------------------------------------------------------
static std::uintptr_t read_uint(const char* base_dir, const char* filename)
{
	char		full_path[200];
	sprintf(full_path, "%s/%s", base_dir, filename);
	const auto	s = File::readAllText(full_path);
	return uint_of(s);
}

// -------------------------------------------------------------------------------------
static std::string read_text(const char* base_dir, const char* filename)
{
	char full_path[200];
	sprintf(full_path, "%s/%s", base_dir, filename);
	auto		s = File::readAllText(full_path);
	while (s.size()>0 && isspace(s[s.size()-1])) {
		s.resize(s.size()-1);
	}
	return s;
}

// -------------------------------------------------------------------------------------
static void read_map(
	UioMap&			map,
	const char*			device_dir,
	const unsigned int	map_index)
{
	char map_dir[200];

	sprintf(map_dir, "%s/maps/map%u", device_dir, map_index);
	map.addr = read_uint(map_dir, "addr");
	map.name = read_text(map_dir, "name");
	map.offset = read_uint(map_dir, "offset");
	map.size = read_uint(map_dir, "size");
}

// --------------------------------------------------------------------------------------------------------------------
static const char* const device_tree_roots[] = {
	"/sys/firmware/devicetree/base/amba_pl/",
	"/sys/firmware/devicetree/base/amba/",
	"/proc/device-tree/amba@0/",
	nullptr
	};

// --------------------------------------------------------------------------------------------------------------------
static void getIpCoreConfiguration(
	std::map<std::string, std::vector<std::uint8_t>>&	configuration,
	const std::string&									ipCoreName,
	const std::uintptr_t								hwAddress)
{
	const std::string	s_addr = ssprintf("@%08x", hwAddress);
	for (unsigned int root_index=0; device_tree_roots[root_index]; ++root_index) {
		const char*	dtr = device_tree_roots[root_index];
		if (File::exists(dtr)) {
			const auto dirs = Directory::getFiles(dtr);
			for (const auto& sdir : dirs) {
				if (ends_with(sdir, s_addr)) {
					const auto ip_core_directory = Path::combine(dtr, sdir);
					auto files = Directory::getFiles(ip_core_directory);
					for (const auto& s : files) {
						const std::string	filename = Path::combine(ip_core_directory, s);

						std::vector<std::uint8_t>	bytes;
						File::readAllBytes(bytes, filename);
						configuration.insert(std::pair<std::string, std::vector<std::uint8_t>>(s, bytes));
					}
					return;
				}
			}
		}
	}
}

// -------------------------------------------------------------------------------------
std::uint32_t UioDevice::getConfigurationUInt32(const std::string& name, const std::uint32_t defaultValue)
{
	uint32_t r = 0;
	std::string	error_message;
	if (_getConfigurationUInt32(r, error_message, name)) {
		return r;
	} else {
		return defaultValue;
	}
}


// -------------------------------------------------------------------------------------
std::uint32_t
UioDevice::getConfigurationUInt32(const std::string& name)
{
	uint32_t r = 0;
	std::string	error_message;
	if (_getConfigurationUInt32(r, error_message, name)) {
		return r;
	} else {
		throw std::runtime_error(error_message);
	}
}

// -------------------------------------------------------------------------------------
void
UioDevice::getConfigurationUInt32Array(
	std::vector<std::uint32_t>&	buffer,
	const std::string&		name)
{
	auto it = _find_config(name);
	if (it == ipCoreConfiguration.end()) {
		return;
	}
	const unsigned int	n = it->second.size() / sizeof(std::uint32_t);

	for (unsigned int i=0; i<n; ++i) {
		std::uint32_t	r = 0;
		for (unsigned int j=0; j<sizeof(std::uint32_t); ++j) {
			r = (r << 8) | it->second[i*sizeof(std::uint32_t) + j];
		}
		buffer.push_back(r);
	}
}

// -------------------------------------------------------------------------------------
std::string
UioDevice::getConfigurationString(const std::string& name)
{
	auto it = _find_config(name);
	if (it == ipCoreConfiguration.end()) {
		throw std::runtime_error(ssprintf("%s: key '%s' not found in device tree", this->name.c_str(), name.c_str()));
	}
	std::string r;
	for (const auto c : it->second) {
		// reinterpret_cast<> won't cut it.
		r += (char)(c);
	}
	return r;
}

// -------------------------------------------------------------------------------------
MappedFile*
UioDevice::getRequiredMap(unsigned int mapIndex)
{
	if (mapIndex < maps.size()) {
		return maps[mapIndex].map;
	} else {
		const auto n = maps.size();
		switch (n) {
		case 0:
			throw std::runtime_error(ssprintf("%s: No map[%u], there no maps at all", name.c_str(), mapIndex));
		case 1:
			throw std::runtime_error(ssprintf("%s: No map[%u], there is 1 map only", name.c_str(), mapIndex));
		default:
			throw std::runtime_error(ssprintf("%s: No map[%u], there %u maps only", name.c_str(), mapIndex, n));
		}
	}
}

// -------------------------------------------------------------------------------------
File::Handle UioDevice::getFileHandle()
{
	if (_file) {
		return _file->getHandle();
	}
	return File::NullHandle;
}

#if defined(WIN32)
// -------------------------------------------------------------------------------------
static const int int8_of_hexchar(const char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	} else {
		throw std::runtime_error(ssprintf("uint8_of_hex: invalid argument '%c' (0x%02X)", c, c));
	}
}
#endif

// -------------------------------------------------------------------------------------
UioDevice::UioDevice(const unsigned int	device_index)
{
	char	device_dir[200];
	sprintf(device_dir, UIO_PATH_PREFIX "%u", device_index);
	const auto	device_name = read_text(device_dir, "name");
	_init(device_index, device_name.c_str());
}

// -------------------------------------------------------------------------------------
/// \param uioName   	Output parameter: path to the UIO device file. This can also throw an exception.
/// \param deviceIndex	Output parameter: UIO device index.
/// \param deviceName	Name of the UIO device.
/// \param pdeviceName	Device to search for, either a) device index N, b) uioN, c) /dev/uioN, d) UIO name
/// \return True iff the device was found, false otherwise.
static bool scan_by_name(unsigned int& deviceIndex, std::string& deviceName, const char* pDeviceName)
{
	char		device_dir[200];
	const int	dnl = strlen(pDeviceName);

	// a) A numerical ID?
	bool is_index_valid = uint_of(pDeviceName, deviceIndex);

	// b) uioN
	if (!is_index_valid) {
		is_index_valid = dnl>3 && strncmp(pDeviceName, "uio", 3)==0 && uint_of(pDeviceName+3, deviceIndex);
	}

	// c) /dev/uioN
	if (!is_index_valid) {
		is_index_valid = dnl>8 && strncmp(pDeviceName, "/dev/uio", 8)==0 && uint_of(pDeviceName+8, deviceIndex);
	}

	if (is_index_valid) {
		sprintf(device_dir, UIO_PATH_PREFIX "%u", deviceIndex);
		deviceName = read_text(device_dir, "name");
		return true;
	}

	// d) scan for the name.
	for (unsigned int device_index=0; UioDevice::isDevicePresent(device_index); ++device_index) {
		sprintf(device_dir, UIO_PATH_PREFIX "%u", device_index);
		deviceName = read_text(device_dir, "name");
		if (strcasecmp(deviceName.c_str(), pDeviceName)==0) {
			deviceIndex = device_index;
			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------------------------
UioDevice::UioDevice(const char* device_name)
{
	unsigned int	uio_device_index = UINT32_MAX;
	std::string		uio_device_name;

	if (scan_by_name(uio_device_index, uio_device_name, device_name)) {
		_init(uio_device_index, uio_device_name.c_str());
	}
	else {
		throw std::runtime_error(ssprintf("UIO Device '%s' not found", device_name));
	}
}

// -------------------------------------------------------------------------------------
void UioDevice::_init(const unsigned int device_index, const char* device_name)
{
#if defined(WIN32)
	throw std::runtime_error("UioDevice::_init: Not implemented on WIN32");
#else
	char		device_dir[200];
	char		maps_dir[200];
	index = device_index;
	_syncFd = -1;
	_file = std::make_shared<File>(ssprintf("/dev/uio%u", device_index));

	sprintf(device_dir, UIO_PATH_PREFIX "%u", device_index);
	sprintf(maps_dir, "%s/maps", device_dir);

	std::vector<std::string>	map_dirs(Directory::getFiles(maps_dir));

	name = device_name;
	version = read_text(device_dir, "version");

	unsigned int map_index = 0;
	for (const auto& dir : map_dirs) {
		std::string	dir2 = ssprintf("map%u", map_index);
		if (dir2 == dir) {
			UioMap	map;
			read_map(map, device_dir, map_index);
			map.map = _file->createMapping(map_index * MappedFile::pageSize(), map.size);
			maps.push_back(map);
			++map_index;
		}
		else {
			throw std::runtime_error(ssprintf("UioDevice %u: Expected map '%s', got map '%s'", map_index, dir2.c_str(), dir.c_str()));
		}
	}
	getIpCoreConfiguration(ipCoreConfiguration, name, maps.size()==0 ? 0 : maps[0].addr);

	// Check for non-coherent sync device name in device tree config
	auto it = _find_config("sync-name");
	if (it != ipCoreConfiguration.end() && !it->second.empty()) {
		std::string syncName;
		for (auto c : it->second) {
			if (c != 0) syncName += (char)c;
		}
		if (!syncName.empty()) {
			std::string syncPath = "/dev/" + syncName;
			_syncFd = open(syncPath.c_str(), O_RDWR);
			// Note: if open fails, _syncFd remains -1 (coherent mode)
		}
	}
#endif
}

// -------------------------------------------------------------------------------------
bool
UioDevice::isDevicePresent(const unsigned int device_index)
{
#if (WIN32)
	return device_index == 0 || device_index == 1;
#else
	char		device_dir[200];
	struct stat	st;

	sprintf(device_dir, UIO_PATH_PREFIX "%u", device_index);
	const int	r_stat = stat(device_dir, &st);
	return r_stat == 0;
#endif
}

// -------------------------------------------------------------------------------------
void
UioDevice::debug_print()
{
#if !defined(WIN32)
	fprintf(stderr, "uio%u: name=%s, version=%s\n", index, name.c_str(), version.c_str());
	for (unsigned int i=0; i<maps.size(); ++i) {
		const auto& map = maps[i];
		fprintf(stderr, "\tmap[%u]: name=%s, addr=0x%p, size=%" PRIuPTR ", offset=0x%p\n",
			i, map.name.c_str(), (void*)map.addr, map.size, (void*)map.offset);
	}

	fprintf(stderr, "\tIP core configuration (%zu items):\n", ipCoreConfiguration.size());
	std::string	tmp;
	for (const auto& k : ipCoreConfiguration) {
		tmp.clear();
		for (const auto b : k.second) {
			tmp += ssprintf("%02X ", (int)b);
		}
		fprintf(stderr, "\t\t%s : %s\n", k.first.c_str(), tmp.c_str());
	}
#endif
}

// -------------------------------------------------------------------------------------
UioDevice::UioDevice(const unsigned int device_index, const char* device_name)
{
	_init(device_index, device_name);
}

// -------------------------------------------------------------------------------------
UioDevice::IpCoreConfigurationMap::const_iterator
UioDevice::_find_config(const std::string& name)
{
	auto it = ipCoreConfiguration.find(name);
	if (it == ipCoreConfiguration.end()) {
		for (auto ii=ipCoreConfiguration.begin(); ii!=ipCoreConfiguration.end(); ++ii) {
			const auto comma_pos = ii->first.find(',');
			if (comma_pos != std::string::npos) {
				const auto name2 = ii->first.substr(comma_pos+1);
				if (name2 == name) {
					it = ii;
					break;
				}
			}
		}
	}
	return it;
}

// -------------------------------------------------------------------------------------
bool UioDevice::_getConfigurationUInt32(std::uint32_t& destination, std::string& errorMessage, const std::string& name)
{
	auto it = _find_config(name);
	if (it == ipCoreConfiguration.end()) {
		ssprintf(errorMessage, "%s: key '%s' not found in device tree", this->name.c_str(), name.c_str());
		return false;
	}
	if (it->second.size() < sizeof(std::uint32_t)) {
		ssprintf(errorMessage, "%s: key '%s' in device tree is short of 4 bytes.", this->name.c_str(), name.c_str());
		return false;
	}

	std::uint32_t	r = 0;
	for (unsigned int i=0; i<sizeof(std::uint32_t); ++i) {
		r = (r << 8) | it->second[i];
	}
	destination = r;
	return true;
}

// -------------------------------------------------------------------------------------
UioDevice::~UioDevice()
{
#if !defined(WIN32)
	if (_syncFd >= 0) {
		close(_syncFd);
		_syncFd = -1;
	}
#endif
}

// -------------------------------------------------------------------------------------
bool UioDevice::syncBufferForCpu(std::uint64_t offset, std::uint64_t size)
{
#if !defined(WIN32)
	if (_syncFd < 0) {
		return false;  // No sync device, coherent buffer
	}

	struct smartio_cache_op op;
	op.offset = offset;
	op.size = size;  // 0 means entire buffer

	int ret = ioctl(_syncFd, SMARTIO_SYNC_FOR_CPU, &op);
	if (ret < 0) {
		throw std::runtime_error(ssprintf("%s: syncBufferForCpu failed: %s",
						  name.c_str(), strerror(errno)));
	}
	return true;
#else
	return false;
#endif
}

// -------------------------------------------------------------------------------------
bool UioDevice::isNonCoherent() const
{
#if !defined(WIN32)
	return _syncFd >= 0;
#else
	return false;
#endif
}

} // namespace smart
