#pragma once

#include "fft.h"
#include <cassert>

template<typename T>
class FFTS
{
public:
    void fftz( T* x, int xN, int N )
    {
        assert( N > 0 && xN > 0 );
        assert( N <= 0x10000 ); // checks N <= 2^16

        for( int n=xN; n < N; ++n )
        {
            int r = 2 * n;
            int i = r + 1;
            x[ r ] = (T)0;
            x[ i ] = (T)0;
        }

        fft( x, N );
    }

    void ifftz( T* x, int xN, int N )
    {
        assert( N > 0 && xN > 0 );
        assert( N <= 0x10000 ); // checks N <= 2^16

        for( int n=xN; n < N; ++n )
        {
            int r = 2 * n;
            int i = r + 1;
            x[ r ] = (T)0;
            x[ i ] = (T)0;
        }

        ifft( x, N );
    }

    void fft( T* x, int N )
    {
        assert( N > 0 );
        assert( N <= 0x100000 ); // checks N <= 2^20
        assert( (N & (N - 1)) == 0 ); // checks N == 2^n

        switch( N )
        {
            case 2:         { _1.fft(x); } break;
            case 4:         { _2.fft(x); } break;
            case 8:         { _3.fft(x); } break;
            case 16:        { _4.fft(x); } break;
            case 32:        { _5.fft(x); } break;
            case 64:        { _6.fft(x); } break;
            case 128:       { _7.fft(x); } break;
            case 256:       { _8.fft(x); } break;
            case 512:       { _9.fft(x); } break;
            case 1024:      { _10.fft(x); } break;
            case 2048:      { _11.fft(x); } break;
            case 4096:      { _12.fft(x); } break;
            case 8192:      { _13.fft(x); } break;
            case 16384:     { _14.fft(x); } break;
            case 32768:     { _15.fft(x); } break;
            case 65536:     { _16.fft(x); } break;
            case 131072:    { _17.fft(x); } break;
            case 262144:    { _18.fft(x); } break;
            case 524288:    { _19.fft(x); } break;
            case 1048576:   { _20.fft(x); } break;
        }
    }

    void ifft( T* x, int N )
    {
        assert( N > 0 );
        assert( N <= 0x100000 ); // checks N <= 2^20
        assert( (N & (N - 1)) == 0 ); // checks N == 2^n

        switch( N )
        {
            case 2:         { _1.ifft(x); } break;
            case 4:         { _2.ifft(x); } break;
            case 8:         { _3.ifft(x); } break;
            case 16:        { _4.ifft(x); } break;
            case 32:        { _5.ifft(x); } break;
            case 64:        { _6.ifft(x); } break;
            case 128:       { _7.ifft(x); } break;
            case 256:       { _8.ifft(x); } break;
            case 512:       { _9.ifft(x); } break;
            case 1024:      { _10.ifft(x); } break;
            case 2048:      { _11.ifft(x); } break;
            case 4096:      { _12.ifft(x); } break;
            case 8192:      { _13.ifft(x); } break;
            case 16384:     { _14.ifft(x); } break;
            case 32768:     { _15.ifft(x); } break;
            case 65536:     { _16.ifft(x); } break;
            case 131072:    { _17.ifft(x); } break;
            case 262144:    { _18.ifft(x); } break;
            case 524288:    { _19.ifft(x); } break;
            case 1048576:   { _20.ifft(x); } break;
        }
    }

private:
    FFT<2,T>        _1;
    FFT<4,T>        _2;
    FFT<8,T>        _3;
    FFT<16,T>       _4;
    FFT<32,T>       _5;
    FFT<64,T>       _6;
    FFT<128,T>      _7;
    FFT<256,T>      _8;
    FFT<512,T>      _9;
    FFT<1024,T>     _10;
    FFT<2048,T>     _11;
    FFT<4096,T>     _12;
    FFT<8192,T>     _13;
    FFT<16384,T>    _14;
    FFT<32768,T>    _15;
    FFT<65536,T>    _16;
    FFT<131072,T>   _17;
    FFT<262144,T>   _18;
    FFT<524288,T>   _19;
    FFT<1048576,T>  _20;
}; // class FFTS