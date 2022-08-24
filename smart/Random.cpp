#include <cmath>		// std::abs
#include <chrono>		// std::chrono
#include <stdexcept>	// std::runtime_error

#include "Random.h"	// ourselves.

namespace smart {
	void Random::Init(int32_t seed)
	{
		int32_t ii;
		int32_t mj, mk;

		// Initialize our Seed array.
		// This algorithm comes from Numerical Recipes in C (2nd Ed.)
		const int32_t subtraction = (seed == std::numeric_limits<int32_t>::min()) ? std::numeric_limits<int32_t>::max() : std::abs(seed);
		mj = MSEED - subtraction;
		SeedArray[55] = mj;
		mk = 1;
		for (int i = 1; i < 55; i++) {  //Apparently the range [1..55] is special (Knuth) and so we're wasting the 0'th position.
			ii = (21 * i) % 55;
			SeedArray[ii] = mk;
			mk = mj - mk;
			if (mk < 0)
				mk += MBIG;
			mj = SeedArray[ii];
		}
		for (int k = 1; k < 5; k++) {
			for (int i = 1; i < 56; i++) {
				SeedArray[i] -= SeedArray[1 + (i + 30) % 55];
				if (SeedArray[i] < 0)
					SeedArray[i] += MBIG;
			}
		}
		inext = 0;
		inextp = 21;
	}

	double Random::Sample()
	{
		// Including this division at the end gives us significantly improved
		// random number distribution.
		return (InternalSample() * (1.0 / MBIG));
	}

	int32_t Random::InternalSample()
	{
		int retVal;
		int locINext = inext;
		int locINextp = inextp;

		if (++locINext >= 56)
			locINext = 1;
		if (++locINextp >= 56)
			locINextp = 1;

		retVal = SeedArray[locINext] - SeedArray[locINextp];

		if (retVal == MBIG)
			retVal--;
		if (retVal < 0)
			retVal += MBIG;

		SeedArray[locINext] = retVal;

		inext = locINext;
		inextp = locINextp;

		return retVal;
	}

	Random::Random()
	{
		Init(static_cast<int32_t>(std::chrono::system_clock::now().time_since_epoch().count()));
	}

	Random::Random(int32_t seed)
	{
		Init(seed);
	}

	double Random::NextDouble()
	{
		return Sample();
	}

	int32_t Random::Next(int32_t maxValue)
	{
		if (maxValue < 0) {
			throw std::runtime_error("Random::Next: Negative parameter not permitted");
		}
		return (int)(Sample() * maxValue);
	}
}
