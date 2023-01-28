#pragma once

#include <algorithm>
#include <vector>
#include <omp.h>
#include "ffts.h"
#include "log.h"
#include "mpm.h"

enum ProcessingModes
{
    RawAudioOnly = 0,
    MagnitudeSpectrum = 1,
    Autocorrelation = 2,
    PitchEstimation = 3
};

class DSP
{
public:
    DSP() {}
    virtual ~DSP() {}

public:
    virtual int getMidiNoteNumber() { return 0; }
    virtual float getPitchMidi() { return 0; }
    virtual float getNacIndex() { return 0; }
    virtual float getPitch() { return 0; }
    virtual int getProcessOutputLen() = 0;
    virtual void process( float* src, int srcLen, float* dest ) = 0;
    void setSamplingRate( float sampsPerSec ) {
        this->R = sampsPerSec;
    }

protected:
    float R;
    struct timespec now;
    int64_t nanos() {
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (int64_t) now.tv_sec*1000000000LL + now.tv_nsec;
    }
};

class FastFourierTransformMagnitudeSpectrum : DSP {
public:
    FastFourierTransformMagnitudeSpectrum( int fftSize ) : DSP() {
        this->fftSize = fftSize;
        this->bufLen = 2 * fftSize;
        this->buffer = new float[ this->bufLen ];
    }
    ~FastFourierTransformMagnitudeSpectrum( ) {
        delete[] buffer;
    }
private:
    int bufLen;
    int fftSize;
    float* buffer;
    FFTS<float> dft;

public:
    virtual int getProcessOutputLen() {
        return fftSize;
    }
    virtual void process( float* src, int srcLen, float* dest ) {
        assert( srcLen == this->fftSize );

        // compute FFT
        for( int n = 0; n < this->fftSize; ++n )
        {
            buffer[2 * n]       = src[n];   // real
            buffer[2 * n + 1]   = 0;        // imaginary
        }
        this->dft.fft( &buffer[0], this->fftSize );

        // compute
        for( int n = 0; n < this->fftSize; ++n )
        {
            int _2n = 2 * n;
            int _2n_1 = _2n + 1;
            dest[n] = sqrt( buffer[_2n]*buffer[_2n] + buffer[_2n_1]*buffer[_2n_1] );
        }
    }
};

class AutocorrelationNormalized : DSP {
public:
    AutocorrelationNormalized(int acLen ) : DSP() {
        this->acLen = acLen;
        this->bufLen = 4 * acLen;
        this->buffer = new float[ this->bufLen ];
    }
    ~AutocorrelationNormalized( ) {
        delete[] buffer;
    }
private:
    int bufLen;
    int acLen;
    float* buffer;
    FFTS<float> dft;

public:
    virtual int getProcessOutputLen() {
        return acLen * 2;
    }
    virtual void process( float* src, int srcLen, float* dest ) {
        assert( srcLen == this->acLen );

        float srcMax = 0;
        for( int n = 0; n < this->acLen; ++n ) {
            float as = abs(src[n]);
            if( srcMax < as) {
                srcMax = as;
            }
        }
        if( srcMax < 0.9f ) {
            srcMax = 0.9f / srcMax;
        }

        // compute FFT
        for( int n = 0; n < this->acLen; ++n )
        {
            buffer[2 * n]       = src[n] * srcMax;   // real
            buffer[2 * n + 1]   = 0;        // imaginary
        }
        int n2 = 2 * this->acLen;
        for( int n = this->acLen; n < n2; ++n )
        {
            buffer[2 * n]       = 0;   // real
            buffer[2 * n + 1]   = 0;        // imaginary
        }
        this->dft.fft( &buffer[0], n2 );

        for( int n=0; n < n2; ++n ) {
            float r = buffer[ 2 * n ];
            float i = buffer[ 2 * n + 1 ];
            float m = r*r + i*i;
            buffer[ 2 * n ] = m;
            buffer[ 2 * n + 1 ] = 0;
        }

        this->dft.ifft( &buffer[0], n2 );

        for( int n=0; n < n2; ++n ) {
            dest[n] = buffer[2*n];
        }
    }
};

class AutocorrelationNormalized2 : DSP {
public:
    AutocorrelationNormalized2( int acLen ) : DSP() {
        this->acLen = acLen;
        this->bufLen = 2 * acLen;
        this->buffer = new float[ this->bufLen ];
    }
    ~AutocorrelationNormalized2( ) {
        delete[] buffer;
    }
private:
    int bufLen;
    int acLen;
    float* buffer;
    FFTS<float> dft;

public:
    virtual int getProcessOutputLen() {
        return acLen;
    }
    virtual void process( float* src, int srcLen, float* dest ) {
        assert( srcLen == this->acLen );

        // compute FFT
        for( int n = 0; n < this->acLen; ++n )
        {
            buffer[2 * n]       = src[n];   // real
            buffer[2 * n + 1]   = 0;        // imaginary
        }
        this->dft.fft( &buffer[0], this->acLen );

        for( int n=0; n < this->acLen; ++n ) {
            float r = buffer[ 2 * n ];
            float i = buffer[ 2 * n + 1 ];
            float m = r*r + i*i;
            buffer[ 2 * n ] = m;
            buffer[ 2 * n + 1 ] = 0;
        }
        this->dft.ifft( &buffer[0], this->acLen );

        for( int n=0; n < this->acLen; ++n ) {
            dest[n] = abs(buffer[2*n]);
        }
    }
};

class PitchEstimator : DSP {
public:
    PitchEstimator( int acLen ) : DSP() {
        this->acLen = acLen;
        this->N = this->acLen;
        this->N2 = 2 * this->N;
        this->bufLen = 4 * this->N;
        this->buffer = new float[ this->bufLen ];

        // possible notes to detect, standard tuning, A = (55, 110, 220, 440 ... ) Hz
        this->tuningN = 48;
        this->midiNoteNumsN = 48;
        this->tuning = new float[ this->tuningN ];
        this->midiNoteNums = new int[ this->midiNoteNumsN ];
        for( int n=0, nn=1; n < 48; ++n, ++nn ) {
            this->tuning[ n ] = (float)(pow(2.0, (double)nn / 12.0) * 55.0); // todo user changeable
            this->midiNoteNums[ n ] = 34 + n;
        }
    }
    ~PitchEstimator( ) {
        delete[] buffer;
        delete[] tuning;
        delete[] midiNoteNums;
    }

private:
    int N;
    int N2;
    int acLen;
    int bufLen;
    float* buffer;
    FFTS<float> dft;

    float pitch;

    float*  tuning;
    int     tuningN;

    float   pitchMidi;
    int     midiNoteNum;
    int*    midiNoteNums;
    int     midiNoteNumsN;

public:
    virtual float getPitch() { return this->pitch; }
    virtual int getMidiNoteNumber() { return this->midiNoteNum; }
    virtual float getPitchMidi() { return this->pitchMidi; }
    virtual int getProcessOutputLen() { return this->N2; }
    virtual void process( float* src, int srcLen, float* dest ) {
        assert( srcLen == this->N );

        int64_t nsStart = 0LL, nsEnd = 0LL;
        nsStart = this->nanos();

        const float energyThreshold = -13.f;
        float energy = 0;
        float srcMax = 0;
        for( int n = 0; n < this->N; ++n ) {
            float as = fabs(src[n]);
            if( srcMax < as) {
                srcMax = as;
            }
            energy += src[n]*src[n];
        }
        energy = 10.f * log10( energy );
        if( energy > energyThreshold ) {
            float maximizeFactor = 1;
            if (srcMax < 0.9f) {
                maximizeFactor = 0.9f / srcMax;
            }

            int _2n, _2n_1;

            // compute FFT
            #pragma omp parallel for
            for (int n = 0; n < this->N; ++n) {
                _2n = 2 * n;
                _2n_1 = _2n + 1;
                buffer[_2n] = src[n] * maximizeFactor;  // real
                buffer[_2n_1] = 0;                      // imaginary
            }

            #pragma omp parallel for
            for (int n = N; n < this->N2; ++n) {
                _2n = 2 * n;
                _2n_1 = _2n + 1;
                buffer[_2n] = 0;    // real
                buffer[_2n_1] = 0;  // imaginary
            }
            this->dft.fft(&buffer[0], this->N2);

            #pragma omp parallel for
            for (int n = 0; n < this->N2; ++n) {
                _2n = 2 * n;
                _2n_1 = _2n + 1;
                buffer[_2n] = buffer[_2n] * buffer[_2n] + buffer[_2n_1] * buffer[_2n_1];
                buffer[_2n_1] = 0;
            }
            this->dft.ifft(&buffer[0], this->N2);

            #pragma omp parallel for
            for (int n = 0; n < this->N2; ++n) {
                dest[n] = buffer[2*n];
            }

            // find first, greatest positive peak going left-to-right
            int dir = 0;
            bool b = true;
            int lastPeakNdx = 0;
            float lastPeakAmp = 0;
            for (int n = 1; /*b &&*/ n < this->N; ++n) {
                float d = dest[n] - dest[n - 1];
                if (d < 0) {
                    if (dir == 1) {
                        if (lastPeakAmp > 0) {
                            if (dest[n - 1] > lastPeakAmp) {
                                lastPeakAmp = dest[n - 1];
                                lastPeakNdx = n - 1;
                            } else {
                                //b = false;
                            }
                        } else {
                            lastPeakAmp = dest[n - 1];
                            lastPeakNdx = n - 1;
                        }
                    }
                    dir = -1;
                } else if (d > 0) {
                    dir = 1;
                } else {
                    dir = 0;
                }
            }
            if ( /*!b &&*/ lastPeakNdx < (this->R / 60.f) && lastPeakNdx > (this->R / 1600.f)) {
                if (lastPeakNdx > 0 && lastPeakNdx < (this->N - 1)) {
                    float a = dest[lastPeakNdx - 1];
                    float b = dest[lastPeakNdx];
                    float c = dest[lastPeakNdx + 1];
                    float pos = 0.5f * ((a - c) / (a - 2.f * b + c));
                    //float peak = b - 0.25f*(a-c)*pos;
                    this->pitch = this->R / (lastPeakNdx + pos);
                } else {
                    this->pitch = this->R / lastPeakNdx;
                }
            } else {
                this->pitch = 0;
            }
        } else {
            this->pitch = 0;
            #pragma omp parallel for
            for (int n = 0; n < this->N2; ++n) {
                dest[n] = 0;
            }
        }

        if( this->pitch > 0 ) {
            //std::binary_search()
            int ji = -1;
            float jd = std::numeric_limits<float>::max();
            for (int jn = 0; jn < this->tuningN; ++jn) {
                float d = fabs(this->tuning[jn] - this->pitch);
                if (d < jd) {
                    ji = jn;
                    jd = d;
                }
            }

            // todo user changeable & vibrato tracking
            if (ji > -1 && ji < tuningN) {
                this->pitchMidi = tuning[ji];
                this->midiNoteNum = this->midiNoteNums[ji];
            }
        } else {
            this->pitchMidi = 0;
            this->midiNoteNum = 0;
        }

        nsEnd = this->nanos();
        int ns = (int) (nsEnd - nsStart);
        LOGI("PitchEstimator::process %d ns", ns);
    }
};

class PitchEstimator2 : DSP {
public:
    PitchEstimator2( int acLen ) : DSP() {
        this->N = acLen;
        this->N2 = 2 * this->N;
        this->bufLen = 4 * this->N;
        this->buffer = new float[ this->bufLen ];

        this->xm = new float[ this->N ];
        this->db = new float[ this->N ];

        // possible notes to detect, standard tuning, A = (55, 110, 220, 440 ... ) Hz
        this->tuningN = 48;
        this->midiNoteNumsN = 48;
        this->tuning = new float[ this->tuningN ];
        this->midiNoteNums = new int[ this->midiNoteNumsN ];
        for( int n=0, nn=1; n < 48; ++n, ++nn ) {
            this->tuning[ n ] = (float)(pow(2.0, (double)nn / 12.0) * 55.0); // todo user changeable
            this->midiNoteNums[ n ] = 34 + n;
        }
    }
    ~PitchEstimator2( ) {
        delete[] db;
        delete[] xm;
        delete[] buffer;
        delete[] tuning;
        delete[] midiNoteNums;
    }

private:
    int N;
    int N2;
    int bufLen;
    float* buffer;

    float* xm;
    float* db;

    float pitch;
    float nacIndex;

    float*  tuning;
    int     tuningN;

    float   pitchMidi;
    int     midiNoteNum;
    int*    midiNoteNums;
    int     midiNoteNumsN;

    Mpm<256, float>         mpm;

public:
    virtual float getNacIndex() { return this->nacIndex; }
    virtual float getPitch() { return this->pitch; }
    virtual int getMidiNoteNumber() { return this->midiNoteNum; }
    virtual float getPitchMidi() { return this->pitchMidi; }
    virtual int getProcessOutputLen() { return this->N2; }
    virtual void process( float* src, int srcLen, float* dest ) {
        assert( srcLen == this->N );

        int64_t nsStart = 0LL, nsEnd = 0LL;
        nsStart = this->nanos();

        //const float energyThreshold = -13.f;
        const float energyThreshold = -15.f;
        float xMax = 0, xmSqrSum = 0, srcEnergy = 0, xmEnergy = 0;
        for( int n = 0; n < this->N; ++n ) {
            float as = fabs(src[n]);
            if(xMax < as) {
                xMax = as;
            }
            xm[n] = xMax;
            xmSqrSum += xm[n]*xm[n];
            srcEnergy += src[n] * src[n];
            db[n] = 10.f * log10( xmSqrSum );
            xmEnergy += db[n];
        }
        xmEnergy /= (float)this->N;
        srcEnergy = 10.f * log10( srcEnergy );
        if( xmEnergy > energyThreshold && srcEnergy > energyThreshold ) {
            float maximizeFactor = 1;
            if (xMax < 0.9f) {
                maximizeFactor = 0.9f / xMax;
            }

            double P = -1;
            P = mpm.pitch( src, this->R, dest, this->getProcessOutputLen() );
            if( P > 80.0 && P < 1600.0 ) {
                this->pitch = P;
                this->nacIndex = this->mpm.getPeriod();
            }
        } else {
            this->pitch = 0;
            this->nacIndex = 0;
#pragma omp parallel for
            for (int n = 0; n < this->N2; ++n) {
                dest[n] = 0;
            }
        }

        if( this->pitch > 0 ) {
            //std::binary_search()
            int ji = -1;
            float jd = std::numeric_limits<float>::max();
            for (int jn = 0; jn < this->tuningN; ++jn) {
                float d = fabs(this->tuning[jn] - this->pitch);
                if (d < jd) {
                    ji = jn;
                    jd = d;
                }
            }

            // todo user changeable & vibrato tracking
            if (ji > -1 && ji < tuningN) {
                this->pitchMidi = tuning[ji];
                this->midiNoteNum = this->midiNoteNums[ji];
            }
        } else {
            this->nacIndex = 0;
            this->pitchMidi = 0;
            this->midiNoteNum = 0;
        }

        nsEnd = this->nanos();
        int ns = (int) (nsEnd - nsStart);
        LOGI("PitchEstimator::process %d ns", ns);
    }
};