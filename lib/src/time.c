// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <chiaki/time.h>

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

CHIAKI_EXPORT uint64_t chiaki_time_now_monotonic_us()
{
#if _WIN32
	LARGE_INTEGER f;
	if(!QueryPerformanceFrequency(&f))
		return 0;
	LARGE_INTEGER v;
	if(!QueryPerformanceCounter(&v))
		return 0;
	// Split into whole seconds and remainder: multiplying the raw counter by
	// 1000000 first overflows int64 after ~10.7 days of uptime at the usual
	// 10 MHz frequency, which made the clock jump to ~1.8e19 us and froze the
	// frame pacing loop.
	const uint64_t ticks = (uint64_t)v.QuadPart;
	const uint64_t freq = (uint64_t)f.QuadPart;
	return (ticks / freq) * 1000000 + ((ticks % freq) * 1000000) / freq;
#else
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return time.tv_sec * 1000000 + time.tv_nsec / 1000;
#endif
}
