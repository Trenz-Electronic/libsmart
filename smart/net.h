#pragma once

#include <vector>	// std::vector

#include <arpa/inet.h>	// struct in_addr

namespace smart {
/// Network functions.
namespace net {


/// Get a list of broadcast IPv4 addresses.
/// Loopback address will not be included.
std::vector<struct in_addr>	getBroadcastAddresses();

/// String representation of an address.
const std::string stringOf(const struct in_addr addr);

} // namespace net
} // namespace smart
