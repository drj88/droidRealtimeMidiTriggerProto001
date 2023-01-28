#pragma once

//#include <cpuid.h> // x86 only
#include <omp.h>
//#include <arm_neon.h>

//static void (*my_function)(int* dst, const int* src, int size); // function pointer
//extern void neon_function(int* dst, const int* src, int size); // defined in some other file
//extern void default_function(int* dst, const int* src, int size);
//void call_function(int* dst, const int* src, int size)
//{
//    // we assume init() was called earlier to set the my_function pointer
//    my_function(dst, src, size);
//}

class CpuFeatures
{
private:
//    AndroidCpuFamily cpu;
    uint64_t features;
    int cores;

public:
    static int has(uint64_t features, uint64_t mask) {
        return ((features & mask) == mask);
    }

    CpuFeatures() {
//        this->cpu = android_getCpuFamily( );
//        this->cores = android_getCpuCount( );
//        this->features = android_getCpuFeatures( );
    }

    int numCores() { return this->cores; }

    bool isARM() {
//        if( cpu == ANDROID_CPU_FAMILY_ARM )
//            return true;
        return false;
    }

    bool hasNeon() {
//        if( cpu == ANDROID_CPU_FAMILY_ARM && CpuFeatures::has(features, ANDROID_CPU_ARM_FEATURE_NEON) )
//            return true;
        return false;
    }
};