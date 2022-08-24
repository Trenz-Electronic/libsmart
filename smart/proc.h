#pragma once

#include <string>	// std::string
#include <vector>	// std::vector


namespace smart {
namespace proc {

/// Statistics from /proc/stat. See "man proc" for the detailed information about the fields.
/// The default constructor initializes all to zero.
class stat {
public:
	/// CPU stats.
	/// The default constructor initializes all to zero.
	class CPU {
	public:
		/// Time spent in user mode.
		unsigned int	user;

		/// Time spent in user mode with low priority (nice).
		unsigned int	nice;

		/// Time spent in system mode.
		unsigned int	system;

		/// Time spent in the idle task.  This value should be USER_HZ times the second entry in the /proc/uptime pseudo-file.
		unsigned int	idle;

		/// Time waiting for I/O to complete.  This value is not reliable, for the following reasons:
        /// 1. The CPU will not wait for I/O to complete; iowait is the time that a task is waiting for I/O to complete.  When a CPU goes into idle state for outstanding task I/O, another task will be scheduled on this CPU.
        /// 2. On a multi-core CPU, the task waiting for I/O to complete is not running on any CPU, so the iowait of each CPU is difficult to calculate.
        /// 3. The value in this field may decrease in certain conditions.
		unsigned int	iowait;

		/// Time servicing interrupts.
		unsigned int	irq;

		/// Time servicing interrupts.
		unsigned int	softirq;

		/// Construct all to zero.
		CPU();

		/// Subtract the CPU-s.
		CPU subtract(const CPU& rhs) const;
	};

	/// Total statistics.
	CPU					total_cpu;

	/// Statistics for individual CPU-s.
	std::vector<CPU>	cpu;

	/// Counts of interrupts serviced since
	/// boot time, for each of the possible system interrupts.
	/// The first column is the total of all interrupts serviced
	/// including unnumbered architecture specific interrupts;
	/// each subsequent column is the total for that
	/// particular numbered interrupt.  Unnumbered interrupts
	/// are not shown, only summed into the total.
	std::vector<unsigned int>	intr;

	/// The number of context switches that the system underwent.
	unsigned int	ctxt;

	/// Time since boot.
	unsigned int	btime;

	/// Number of forks since boot.
	unsigned int	processes;

	/// Number of processes in runnable state.
	unsigned int	procs_running;

	/// Number of processes blocked waiting for I/O to complete.
	unsigned int	procs_blocked;

	/// Number of softirq for all CPUs.
	/// The first column is the total of all softirqs and each
	/// subsequent column is the total for particular softirq.
	std::vector<unsigned int>	softirq;

	/// Constructor.
	stat();

	/// Refresh the stats from the live system.
	/// \return Empty string iff successful, error message otherwise.
	std::string refresh();

	/// Subtract data from this one.
	stat subtract(const stat& rhs) const;

private:
	std::vector<unsigned int>	ibuf_;
}; // class Stat

} // namespace Proc
} // namespace smart
