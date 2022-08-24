#include <string>		// std::string
#include <stdexcept>	// std::runtime_error

#include <errno.h>		// errno
#include <ifaddrs.h>	// getifaddrs
#include <string.h>		// strlen, strstr.
#include <sys/types.h>

#include "net.h"		// ourselves.
#include "Process.h"	// Process:checkOutput
#include "string.h"		// ssprintf


namespace smart {
namespace net {

std::vector<struct in_addr>	getBroadcastAddresses()
{
	std::vector<struct in_addr>	r;
	struct ifaddrs*				ifaddr = nullptr;
	struct ifaddrs*				ifa;
	struct in_addr				bip;

	if (getifaddrs(&ifaddr) < 0) {
		throw std::runtime_error(ssprintf("getifaddrs failed, errno=%d", errno));
	}
	try {
		for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
			// Will not work if no address.
			// Will not work on addresses other than INET.
			if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
				continue;
			}
			bip = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_ifu.ifu_broadaddr)->sin_addr;
			if (bip.s_addr == htonl(INADDR_LOOPBACK)) {
				continue;
			}
			r.push_back(bip);
		}
		freeifaddrs(ifaddr);
	}
	catch (...) {
		freeifaddrs(ifaddr);
	}
	return r;
}

const std::string stringOf(const struct in_addr addr)
{
	std::string r;
	r.resize(100);
	if (inet_ntop(AF_INET, &addr, &r[0], r.size()) == nullptr) {
		throw std::runtime_error(ssprintf("inet_ntop: errno=%d", errno));
	}
	// resize to not to include the zero.
	for (unsigned int i=0; i<r.size(); ++i) {
		if (r[i]==0) {
			r.resize(i);
			return r;
		}
	}
	return r;
}


} // namespace net
} // namespace smart
