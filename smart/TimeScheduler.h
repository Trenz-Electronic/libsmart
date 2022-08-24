/*
 * TimeScheduler.h
 *
 *  Created on: Feb 3, 2016
 *      Author: peeter
 */

#pragma once

#include <stdint.h>	// uint64_t


namespace smart {

/// Time scheduler.
class TimeScheduler
{
public:
	/// Create the scheduler
	TimeScheduler();

	///
	/// Check if the scheduled event needs to run.
	/// \param seconds_since_midnight	Time of the scheduled event.
	/// \param now_ticks_utc			Current time, in .NET ticks UTC.
	/// \return true if the time has passed. It returns true only once per day.
	bool tick(const unsigned int seconds_since_midnight, const uint64_t now_ticks_utc);

private:
	/// Seconds since midnight from the last invocation of tick().
	unsigned int					_seconds_since_midnight;
};

} // namespace smart
