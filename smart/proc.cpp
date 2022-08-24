#include <algorithm>	// std::copy
#include <iterator>		// std::begin
#include <limits>		// std::numeric_limits

#include <stdio.h>		// fopen, the most efficient file read.
#include <string.h>		// strtok

#include "string.h"
#include "File.h"	// smart::File

#include "proc.h"	// ourselves.



namespace smart {
namespace proc {

stat::CPU::CPU()
: user(0), nice(0), system(0), idle(0), iowait(0), irq(0), softirq(0)
{
}

stat::CPU stat::CPU::subtract(const CPU& rhs) const
{
	stat::CPU	r;
	r.user = user - rhs.user;
	r.nice = nice - rhs.nice;
	r.system = system - rhs.system;
	r.idle = idle - rhs.idle;
	r.iowait = iowait - rhs.iowait;
	r.irq = irq - rhs.irq;
	r.softirq = softirq - rhs.softirq;
	return r;
}

stat::stat()
:ctxt(0), btime(0), processes(0), procs_running(0), procs_blocked(0)
{
}


static const char* stat_delim = " \t\r\n";

static unsigned int element_or_default(const std::vector<unsigned int>& v, unsigned int index)
{
	if (index < v.size()) {
		return v[index];
	}
	return std::numeric_limits<unsigned int>::max();
}

static const stat::CPU cpu_of_ibuf(const std::vector<unsigned int>& v)
{
	stat::CPU	r;
	r.user = element_or_default(v, 0);
	r.nice = element_or_default(v, 1);
	r.system = element_or_default(v, 2);
	r.idle = element_or_default(v, 3);
	r.iowait = element_or_default(v, 4);
	r.irq = element_or_default(v, 5);
	r.softirq = element_or_default(v, 6);
	return r;
}

static void assign(std::vector<unsigned int>& dst, const std::vector<unsigned int>& src)
{
	dst.resize(src.size());
	std::copy(src.begin(), src.end(), std::begin(dst));
}

std::string stat::refresh()
{
	char	line[4000];
	FILE*	f = fopen("/proc/stat", "r");
	if (f == nullptr) {
		return "Stat::update: /proc/stat cannot be opened";
	}
	File	f_auto(f);

	unsigned int	cpu_count = 0;
	for (;;) {
		// Read the line into buffer.
		line[0] = 0;
		line[sizeof(line)-1u] = 0;
		char*	r_fgets = fgets(line, sizeof(line)-1u, f);
		if (r_fgets == nullptr) {
			break;
		}

		// Parse the name.
		char* saveptr = nullptr;
		// start the strtok
		char* name = strtok_r(line, stat_delim, &saveptr);
		if (name == nullptr) {
			continue;
		}

		// Parse the integers into ibuf_.
		ibuf_.clear();
		for (char* tok = strtok_r(nullptr, stat_delim, &saveptr); tok!=nullptr; tok=strtok_r(nullptr, stat_delim, &saveptr)) {
			unsigned int	i = 0;
			if (uint_of(tok, i)) {
				ibuf_.push_back(i);
			}
			else {
				ibuf_.push_back(-1);
			}
		}

		// What shall we do with the name?
		if (strcmp(name, "cpu") == 0) {
			total_cpu = cpu_of_ibuf(ibuf_);
			continue;
		}
		// let's assume alphabetic ordering :)
		if (starts_with(name, "cpu")) {
			if (cpu_count == cpu.size()) {
				cpu.push_back(cpu_of_ibuf(ibuf_));
				++cpu_count;
				continue;
			}
			const unsigned int	next_cpu_count = cpu_count + 1u;
			if (cpu_count<cpu.size()) {
				cpu.resize(next_cpu_count);
			}
			cpu[cpu_count] = cpu_of_ibuf(ibuf_);
			cpu_count = next_cpu_count;
			continue;
		}
		if (strcmp(name, "intr") == 0) {
			assign(intr, ibuf_);
			continue;
		}
		if (strcmp(name, "ctxt") == 0) {
			ctxt = element_or_default(ibuf_, 0);
			continue;
		}
		if (strcmp(name, "btime") == 0) {
			btime = element_or_default(ibuf_, 0);
			continue;
		}
		if (strcmp(name, "processes") == 0) {
			processes = element_or_default(ibuf_, 0);
			continue;
		}
		if (strcmp(name, "procs_running") == 0) {
			procs_running = element_or_default(ibuf_, 0);
			continue;
		}
		if (strcmp(name, "procs_blocked") == 0) {
			procs_blocked = element_or_default(ibuf_, 0);
			continue;
		}
		if (strcmp(name, "softirq") == 0) {
			assign(softirq, ibuf_);
			continue;
		}
	}
	cpu.resize(cpu_count);

	return std::string();
}

static void subtract_array(
		std::vector<unsigned int>& dst,
		const std::vector<unsigned int>& src,
		const std::vector<unsigned int>& rhs)
{
	dst.resize(src.size());
	for (unsigned int i=0; i<src.size(); ++i) {
		if (i < rhs.size()) {
			dst[i] = src[i] - rhs[i];
		}
		else {
			dst[i] = src[i];
		}
	}
}

stat stat::subtract(const stat& rhs) const
{
	stat r;
	r.total_cpu = total_cpu.subtract(rhs.total_cpu);
	for (unsigned int i=0; i<cpu.size(); ++i) {
		if (i < rhs.cpu.size()) {
			r.cpu.push_back(cpu[i].subtract(rhs.cpu[i]));
		}
		else {
			r.cpu.push_back(cpu[i]);
		}
	}
	subtract_array(r.intr, intr, rhs.intr);
	r.ctxt = ctxt - rhs.ctxt;
	r.btime = btime - rhs.btime;
	r.processes = processes - rhs.processes;
	r.procs_running = procs_running - rhs.procs_running;
	r.procs_blocked = procs_blocked - rhs.procs_blocked;
	subtract_array(r.softirq, softirq, rhs.softirq);
	return r;
}

} // namespace Proc
} // namespace smart
