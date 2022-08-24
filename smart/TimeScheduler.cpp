#include <limits.h>

#include "time.h"

#include "TimeScheduler.h"

using namespace smart;


namespace smart {

/// Seconds per day.
static constexpr unsigned int	SECONDS_PER_DAY = 24 * 3600;

// --------------------------------------------------------------------------------------------------------------------
TimeScheduler::TimeScheduler()
: _seconds_since_midnight(UINT_MAX)
{
}

// --------------------------------------------------------------------------------------------------------------------
bool TimeScheduler::tick(const unsigned int seconds_since_midnight, const uint64_t now_ticks_utc)
{
	if (_seconds_since_midnight > SECONDS_PER_DAY) {
		if (now_ticks_utc < Ticks::YEAR_BUILD) {
			// do not accept uninitialized time.
			return false;
		}
		_seconds_since_midnight = time_seconds_since_midnight_of_ticks_utc(now_ticks_utc);
		return false;
	}

	const unsigned int	new_seconds_since_midnight = time_seconds_since_midnight_of_ticks_utc(now_ticks_utc);

	bool	r = false;
	if (_seconds_since_midnight <= new_seconds_since_midnight) {
		r = _seconds_since_midnight < seconds_since_midnight && seconds_since_midnight <= new_seconds_since_midnight;
	}
	else {
		// Small changes backwards are to be expected. We just skip them.
		// Only when the change involves the midnight.
		if (new_seconds_since_midnight < 10*60 && (24*3600-10*60)<_seconds_since_midnight) {
			// Day change.
			r = seconds_since_midnight <= new_seconds_since_midnight;
		}
	}
	_seconds_since_midnight = new_seconds_since_midnight;
	return r;
}

} // namespace smart
