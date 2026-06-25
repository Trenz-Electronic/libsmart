#pragma once

#include <string>	// std::string
#include <vector>	// std::vector

#include <arpa/inet.h>	// struct in_addr

struct ifaddrs;	// fwd decl (defined in <ifaddrs.h>); broadcastAddressesOf() takes a list of these.

namespace smart {
/// Network functions.
namespace net {


/// Get a list of broadcast IPv4 addresses.
/// Loopback address will not be included.
std::vector<struct in_addr>	getBroadcastAddresses();

/// Pure filter behind getBroadcastAddresses(): collect the IPv4 broadcast
/// addresses from an ifaddrs list, skipping non-INET entries and the loopback
/// broadcast. Exposed so the filtering logic can be unit-tested without the
/// live getifaddrs() syscall.
std::vector<struct in_addr>	broadcastAddressesOf(const struct ifaddrs* ifaddr);

/// String representation of an address.
const std::string stringOf(const struct in_addr addr);

} // namespace net
} // namespace smart
