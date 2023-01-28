#pragma once

#include <cmath>

#define THREAD_PRIORITY_AUDIO 0xfffffff0

enum OboeCustomReturns
{
    Success,
    SamplingRateRequestFailed,
    OpenStreamFailed,
    StartAudioFailed
};

struct timespec cnow;
int64_t cnanos() {
    clock_gettime(CLOCK_MONOTONIC, &cnow);
    return (int64_t) cnow.tv_sec*1000000000LL + cnow.tv_nsec;
}