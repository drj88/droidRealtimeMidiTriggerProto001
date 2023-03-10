#pragma once

#include <jni.h>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "fftpack.h"
#include "ls_fft.h"
#include "bluestein.h"
#include "log.h"
#include "util.h"

#define maxlen 4096

static void fill_random (double *data, double *odata, size_t length)
{
    size_t m;
    for (m=0; m<length; ++m)
        data[m] = odata[m] = rand()/(RAND_MAX+1.0)-0.5;
}
static void fill_random_c (double *data, double *odata, size_t length)
{
    size_t m;
    for (m=0; m<length; ++m)
    {
        data[2*m] = odata[2*m] = rand()/(RAND_MAX+1.0)-0.5;
        data[2*m+1] = odata[2*m+1] = 0.;
    }
}

static void normalize (double *data, size_t length, double norm)
{
    size_t m;
    for (m=0; m<length; ++m)
        data[m] /= norm;
}

static double errcalc (double *data, double *odata, size_t length)
{
    size_t m;
    double sum, errsum;
    sum = errsum = 0;
    for (m=0; m<length; ++m)
    {
        errsum += (data[m]-odata[m])*(data[m]-odata[m]);
        sum += odata[m]*odata[m];
    }
    return sqrt(errsum/sum);
}

static void test_real(void)
{
    double data[2*maxlen], odata[2*maxlen];
    int length;
    double err;
    const double epsilon=3e-15;
    real_plan plan;
    for (length=2; length<=maxlen; length *= 2)
    {
        fill_random_c (data, odata, length);
        plan = make_real_plan (length);

        int64_t nsStart = cnanos();
        real_plan_forward_c(plan, data);
        int64_t nsEnd = cnanos();
        int nsForward = (int) (nsEnd - nsStart);

        nsStart = cnanos();
        real_plan_backward_c(plan, data);
        nsEnd = cnanos();
        int nsInverse = (int) (nsEnd - nsStart);

        normalize (data, 2*length, length);
        kill_real_plan (plan);
        err = errcalc (data, odata, 2*length);
        LOGI("FFTPACK_FORWARD_1: %i: %e %d %d\n",length,err,nsForward,nsInverse);
    }
    for (length=2; length<=maxlen; ++length)
    {
        fill_random (data, odata, length);
        plan = make_real_plan (length);

        int64_t nsStart = cnanos();
        real_plan_forward_fftpack (plan, data);
        int64_t nsEnd = cnanos();
        int nsForward = (int) (nsEnd - nsStart);

        nsStart = cnanos();
        real_plan_backward_fftpack (plan, data);
        nsEnd = cnanos();
        int nsInverse = (int) (nsEnd - nsStart);

        normalize (data, length, length);
        kill_real_plan (plan);
        err = errcalc (data, odata, length);
        LOGI("FFTPACK_FORWARD_2: %i: %e %d %d\n",length,err,nsForward,nsInverse);
    }
}

static void test_complex(void)
{
    double data[2*maxlen], odata[2*maxlen];
    int length;
    double err;
    const double epsilon=3e-15;
    complex_plan plan;
    for (length=1; length<=maxlen; ++length)
//    for (length=2; length<=maxlen; length *= 2)
    {
        fill_random (data, odata, 2*length);
        plan = make_complex_plan (length);

        int64_t nsStart = cnanos();
        complex_plan_forward(plan, data);
        int64_t nsEnd = cnanos();
        int nsForward = (int) (nsEnd - nsStart);

        nsStart = cnanos();
        complex_plan_backward(plan, data);
        nsEnd = cnanos();
        int nsInverse = (int) (nsEnd - nsStart);
        normalize (data, 2*length, length);
        kill_complex_plan (plan);
        err = errcalc (data, odata, 2*length);
        LOGI("FFTPACK_FORWARD_3: %i: %e %d %d\n",length,err,nsForward,nsInverse);
    }
}