/// \file	main.cpp
/// \brief	Main function of the program "uio" providing access to the UIO (Userspace I/O) devices.
///
/// \version 	1.0
/// \date		2017
/// \copyright	SPDX: BSD-3-Clause 2016-2017 Trenz Electronic GmbH

#include <exception>	// std::exception
#include <sstream>

#include <inttypes.h>
#include <memory>	// std::make_shared
#include <getopt.h>
#include <stdbool.h>	// uio
#include <stdio.h>		// printf
#include <string.h>		// string
#include <unistd.h>		// sysconf


#include <smart/File.h>
#include <smart/hw/AxiDataCapture.h>
#include <smart/string.h>
#include <smart/time.h>
#include <smart/UioDevice.h>
#include <smart/WavFormat.h>


using namespace smart;

// --------------------------------------------------------------------------
static void _print_usage()
{
	printf("Usage:\n");
	printf("    uio DEVICE COMMAND ARG1\n");
	printf("\n");
	printf("possible commands are\n");
	printf("    capture time output_file         Capture data from the AXI-Data-Capture IP core and write it as a raw file\n");
	printf("    dump map output_file             Dump the memory area\n");
	printf("    fill map 32-bit-value            Fill the memory area with 32-bit value\n");
	printf("Example: capture 1 second of data from the UIO device \"RMS-Stream\":\n");
	printf("    uio RMS-Stream capture 1 rms.raw\n");
}

// --------------------------------------------------------------------------
// device map filename
//static int uio_dump(const unsigned int device_index, const unsigned int map_index, const char* filename)
const static void uio_dump(const std::shared_ptr<UioDevice> device, const unsigned int argc, const char** argv)
{
	if(argc < 2) {
		std::stringstream err;
		err << "uio dump: " << argc << " arguments given, expected 2";
		throw std::runtime_error(err.str());
	}

	const unsigned int map_index = uint_of(argv[0]);
	const char* filename = argv[1];

	if (device->maps.size() <= map_index) {
		std::stringstream err;
		err << "uio dump: Map index " << map_index << " invalid, there are only " << device->maps.size() << " maps available";
		throw std::runtime_error(err.str());
	}

	MappedFile*		map = device->maps[map_index].map;

	FILE*	f = fopen(filename, "wb");
	if (f == nullptr) {
		std::stringstream err;
		err << "Cannot open file '" << filename << "' for writing";
		throw std::runtime_error(err.str());
	} else {
		printf("Writing file '%s'.\n", filename);
		const int	r_write = fwrite(map->data(), 1, map->size(), f);
		if (r_write != static_cast<int>(map->size())) {
			std::stringstream err;
			err << "Cannot write: fwrite returned " << r_write << ", errno=" << errno;
			throw std::runtime_error(err.str());
		}
		printf("Wrote %zu bytes.\n", map->size());
	}
}

// --------------------------------------------------------------------------
const static void uio_fill(const std::shared_ptr<UioDevice> device, const unsigned int argc, const char** argv)
{
	if(argc < 2) {
		std::stringstream err;
		err << "uio fill: " << argc << " arguments given, expected 2";
		throw std::runtime_error(err.str());
	}

	unsigned int map_index = uint_of(argv[0]);
	unsigned int value = uint_of(argv[1]);

	if (device->maps.size() <= map_index) {
		std::stringstream err;
		err << "uio fill: Map index " << map_index << " invalid, there are only " << device->maps.size() << " maps available.";
		throw std::runtime_error(err.str());
	}

	MappedFile*		map = device->maps[map_index].map;
	const unsigned int	size = map->size32();

	for (unsigned int i=0; i<size; ++i) {
		map->write32(i, value);
	}
	printf("Successfully wrote value %d to map %d\n", value, map_index);
}

#pragma pack(push, 1)
	class PacketBuffer {
	public:
		uint64_t	payload;
	};
#pragma pack(pop)

// --------------------------------------------------------------------------
const static void uio_capture(std::shared_ptr<UioDevice> device, const unsigned int argc, const char** args)
{
	if(argc < 2) {
		std::stringstream err;
		err << "uio capture: " << argc << " arguments given, expected 2";
		throw std::runtime_error(err.str());
	}
	printf("Capture device:   %s\n", device->name.c_str());
	printf("Capture filename: %s\n", args[1]);
	hw::AxiDataCapture	dev(device);

	dev.stopCapture();
	dev.clearBuffer();

	const unsigned int user_time = uint_of(args[0]);
	const char* filename = args[1];

	PacketBuffer	buffer = { 0 };
	constexpr int	bufsize = sizeof(buffer);
	const uint64_t t0 = time_us();
	const uint64_t target_time = t0 +  user_time * 1000 * 1000;
	uint64_t		bytes_written = 0;

	FILE*	f = fopen(filename, "w");

	dev.startCapture(smart::hw::AxiDataCapture::CAPTURE_STREAMING); // 0=streaming
	while (time_us() < target_time) {
		volatile PacketBuffer* res = dev.fetchPacket<PacketBuffer>(&buffer);
		if (res == nullptr) {
			usleep(1000);
			continue;
		}
		const int r_write = fwrite((const void*)res, sizeof(uint8_t), bufsize, f);
		if (r_write != bufsize) {
			throw std::runtime_error(ssprintf("Cannot write: fwrite returned %d, errno=%d", r_write, errno));
		}
		bytes_written += r_write;
	}

	const uint64_t		real_time = time_us() - t0;
	dev.stopCapture();

	printf("Capture time: %u ms\n", static_cast<unsigned int>(real_time / 1000u));
	printf("Bytes written: %" PRIu64 "\n", bytes_written);
	printf("Data rate:  %g bytes/sec\n", bytes_written / (real_time * 1e-6));
}

const static void uio_list(std::shared_ptr<UioDevice> device, const unsigned int argc, const char** argv)
{
	printf("List not implemented yet.\n");
}

/// Information about the command.
struct CommandInfo {
public:
	/// Name of the command; null when end of list.
	const char* name;

	/// Argument count.
	const int	argc;

	/// Command handler to be executed.
	const void (*handler)(std::shared_ptr<UioDevice>, const unsigned int, const char**);
};

static const CommandInfo commands[] = {
		{ "list", 0, uio_list },
		{ "capture", 2, uio_capture },
		{ "fill", 2, uio_fill },
		{ "dump", 2, uio_dump },
		{ nullptr, 0, nullptr }
};


// --------------------------------------------------------------------------
int main(const int argc, const char** argv)
{
	if (argc < 2) {
		printf("Not enough arguments.\n");
		_print_usage();
		return 1;
	}

	int arg_pos = 0;

	// first arg, device
	++arg_pos;
	printf("Opening device %s\n", argv[arg_pos]);
	const std::shared_ptr<UioDevice> uio_device = std::make_shared<UioDevice>(argv[arg_pos]);

	// second arg, command
	++arg_pos;
	const CommandInfo* cmd = nullptr;
	const char*	arg_name = argv[arg_pos];

	for (int i_cmd = 0; commands[i_cmd].name!=nullptr; ++i_cmd) {
		if (strcmp(commands[i_cmd].name, arg_name) == 0) {
			cmd = &commands[i_cmd];
			break;
		}
	}
	if( cmd == nullptr ){
		printf("Unknown command '%s'\n", argv[arg_pos]);
		_print_usage();
		return 2;
	}

	const int handled_tokens = ++arg_pos;
	// let the command handle the rest
	try {
		cmd->handler(uio_device, argc - handled_tokens, argv + handled_tokens);
	} catch (const std::runtime_error& ex) {
		printf("Error: %s\n", ex.what());
		_print_usage();
		return 1;
	} catch (const std::exception& ex) {
		printf("Error: %s\n", ex.what());
	}

    return 0;
}
