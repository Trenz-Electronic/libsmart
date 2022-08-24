#pragma once

#include <limits>	// std::numeric_limits
#include <stdint.h>	// int32_t

namespace smart {

	/// Adoptation of System.Random for easier porting.
	class Random
	{
	private:
		const int32_t MBIG = std::numeric_limits<int32_t>::max();
		const int32_t MSEED = 161803398;
		const int32_t MZ = 0;

		int32_t inext;
		int32_t inextp;
		int32_t SeedArray[56];

	private:
		void Init(int32_t seed);

		double Sample();

		int32_t InternalSample();

	public:
		/// Seed the random generator from the current time.
		Random();
		/// Seed the random generator with the given value.
		Random(int32_t seed);

		/// Next value from the random generator, in the range [0 ... 1).
		double NextDouble();

		/// Next value from the random generator, in the range [0 ... Max).
		int32_t Next(int32_t maxValue);
	};

}