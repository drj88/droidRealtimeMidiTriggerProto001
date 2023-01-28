#pragma once

#include <algorithm>
#include <complex>
#include <float.h>
#include <map>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "fft.h"

#define MPM_CUTOFF 0.93
#define MPM_SMALL_CUTOFF 0.001
#define MPM_LOWER_PITCH_CUTOFF 80.0
#define PMPM_PA 0.01
#define PMPM_N_CUTOFFS 20
#define PMPM_PROB_DIST 0.05
#define PMPM_CUTOFF_BEGIN 0.8
#define PMPM_CUTOFF_STEP 0.01

template <int N, typename T> class BaseAlloc
{
public:
    std::vector<std::complex<float>> out_im;
    std::vector<T> out_real;
    T* fftForwardBuffer;
    T* fftInverseBuffer;
    FFT<N, T> F;

    BaseAlloc( ) :
        out_im( std::vector<std::complex<float>>( 2*N ) ),
        out_real( std::vector<T>(N) )
    {
        fftForwardBuffer = new T[ 2*N ];
        fftInverseBuffer = new T[ 2*N ];
    }

    ~BaseAlloc()
    {
        delete[] fftForwardBuffer;
        delete[] fftInverseBuffer;
    }

protected:
    void clear()
    {
        std::fill( out_im.begin(), out_im.end(), std::complex<T>(0.0, 0.0) );
    }
};

template <typename T> std::pair<T, T> parabolic_interpolation(const std::vector<T> &array, int x_)
{
    int x_adjusted;
    T x = (T)x_;

    if (x < 1) {
        x_adjusted = (array[x] <= array[x + 1]) ? x : x + 1;
    } else if (x > signed(array.size()) - 1) {
        x_adjusted = (array[x] <= array[x - 1]) ? x : x - 1;
    } else {
        T den = array[x + 1] + array[x - 1] - 2 * array[x];
        T delta = array[x - 1] - array[x + 1];
        return (!den) ? std::make_pair(x, array[x])
                    : std::make_pair(x + delta / (2 * den),
                            array[x] - delta * delta / (8 * den));
    }
    return std::make_pair(x_adjusted, array[x_adjusted]);
}

template <int N, typename T> void acorr_r( T* audio_buffer, BaseAlloc<N,T> *ba )
{
//    if (audio_buffer.size() == 0)
//        throw std::invalid_argument("audio_buffer shouldn't be empty");

    // std::transform(
    //     audio_buffer.begin(), 
    //     audio_buffer.begin() + ba->N,
    //     ba->out_im.begin(), 
    //     [](T x) -> std::complex<T> 
    //     {
    //         return std::complex(x, static_cast<T>(0.0));
    //     }
    // );

    ///ffts_execute(ba->fft_forward, ba->out_im.data(), ba->out_im.data());
    T* buf = audio_buffer;
    for (int n = 0; n < N; ++n)
    {
        ba->fftForwardBuffer[ 2*n ]   = buf[n]; // real
        ba->fftForwardBuffer[ 2*n+1 ] = 0;	    // imaginary
    }

    ba->F.fft( ba->fftForwardBuffer );

    for (int n=0, k=0, nx=2*N; n < nx; n += 2, ++k )
    {
        ba->out_im[k] = 
            std::complex<float>( 
                (float)ba->fftForwardBuffer[ n ],  // real
                (float)ba->fftForwardBuffer[ n+1 ] // imaginary
            );
    }

    ///std::complex<float> scale = { 1.0f / (float)(ba->N * 2), static_cast<T>(0.0) };
    std::complex<float> scale = { 1.0f / (float)(2*N), static_cast<T>(0.0) };
    for (int i = 0; i < N; ++i)
        ba->out_im[i] *= std::conj(ba->out_im[i]) * scale;

    ///ffts_execute(ba->fft_backward, ba->out_im.data(), ba->out_im.data());
    for (int n = 0; n < N; ++n)
    {
        ba->fftInverseBuffer[ 2*n ]   = ba->out_im[n].real(); // real
        ba->fftInverseBuffer[ 2*n+1 ] = ba->out_im[n].imag(); // imaginary
    }

    ba->F.ifft( ba->fftInverseBuffer );

    for (int n=0, k=0, nx=2*N; n < nx; n += 2, ++k )
    {
        ba->out_im[k] = 
            std::complex<float>( 
                (float)ba->fftInverseBuffer[ n ],  // real
                (float)ba->fftInverseBuffer[ n+1 ] // imaginary
            );
    }

    std::transform(
        ba->out_im.begin(), 
        ba->out_im.begin() + N,
        ba->out_real.begin(),
        [](std::complex<float> cplx) -> T { return std::real(cplx); }
    );
}

template <typename T> static std::vector<int> peak_picking(const std::vector<T> &nsdf)
{
	std::vector<int> max_positions{};
	int pos = 0;
	int cur_max_pos = 0;
	ssize_t size = nsdf.size();

	while (pos < (size - 1) / 3 && nsdf[pos] > 0)
		pos++;
	while (pos < size - 1 && nsdf[pos] <= 0.0)
		pos++;

	if (pos == 0)
		pos = 1;

	while (pos < size - 1) {
		if (nsdf[pos] > nsdf[pos - 1] && nsdf[pos] >= nsdf[pos + 1] &&
		    (cur_max_pos == 0 || nsdf[pos] > nsdf[cur_max_pos])) {
			cur_max_pos = pos;
		}
		pos++;
		if (pos < size - 1 && nsdf[pos] <= 0) {
			if (cur_max_pos > 0) {
				max_positions.push_back(cur_max_pos);
				cur_max_pos = 0;
			}
			while (pos < size - 1 && nsdf[pos] <= 0.0) {
				pos++;
			}
		}
	}
	if (cur_max_pos > 0) {
		max_positions.push_back(cur_max_pos);
	}
	return max_positions;
}

/*
* Allocate the buffers for MPM for re-use.
* Intended for multiple consistently-sized audio buffers.
*
* Usage: pitch_alloc::Mpm ma(1024)
*
* It will throw std::bad_alloc for invalid sizes (<1)
*/
template <int N, typename T> class Mpm : public BaseAlloc<N,T>
{
public:
    Mpm( ) : BaseAlloc<N,T>( ) { }

    T pitch( T* audio_buffer, int sample_rate, T* output, int outputLen )
    {
        acorr_r(audio_buffer, this);
        for( int n=0; n < outputLen && n < this->out_real.size(); ++n )
            output[n] = this->out_real[n];

        std::vector<int> max_positions = peak_picking( this->out_real );
        std::vector<std::pair<T, T>> estimates;

        T highest_amplitude = -DBL_MAX;

        for (int i : max_positions) {
            highest_amplitude = std::max(highest_amplitude, this->out_real[i]);
            if (this->out_real[i] > MPM_SMALL_CUTOFF) {
                auto x = parabolic_interpolation(this->out_real, i);
                estimates.push_back(x);
                highest_amplitude = std::max(highest_amplitude, std::get<1>(x));
            }
        }

        if (estimates.empty())
            return -1;

        T actual_cutoff = MPM_CUTOFF * highest_amplitude;
        period = 0;

        for (auto i : estimates) {
            if (std::get<1>(i) >= actual_cutoff) {
                period = std::get<0>(i);
                break;
            }
        }

        T pitch_estimate = (sample_rate / period);

        this->clear();

        return (pitch_estimate > MPM_LOWER_PITCH_CUTOFF) ? pitch_estimate : -1;
    }

    T getPeriod() { return period; }

protected:
    T period;
};