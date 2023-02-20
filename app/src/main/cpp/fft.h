#pragma once

/*
  Code derived from the meta-programming template class:

  A classy FFT and Inverse FFT C++ class library

  Author: Tim Molteno, tim@physics.otago.ac.nz

  Based on the article "A Simple and Efficient FFT Implementation in C++" by Volodymyr Myrnyy
  with just a simple Inverse FFT modification.

  Licensed under the GPL v3.
*/

template<int A, int B, int M, int N>
struct InfSeries {
    static double val() {
        return 1-(A*M_PI/B)*(A*M_PI/B)/M/(M+1)*InfSeries<A,B,M+2,N>::val();
    }
};

template<int A, int B, int N>
struct InfSeries<A,B,N,N> {
    static double val() {
        return 1.;
    }
};

template<int A, int B, typename T>
struct SineInfSeries;

template<int A, int B>
struct SineInfSeries<A,B,float> {
    static float val() {
        return (A*M_PI/B)*InfSeries<A,B,2,12>::val();
    }
};

template<int A, int B>
struct SineInfSeries<A,B,double> {
    static double val() {
        return (A*M_PI/B)*InfSeries<A,B,2,20>::val();
    }
};

template<int N, typename T>
class DanLanRecurant // Danielson-Lanczos Recursion
{
public:
    inline void recur( T* x, int iSign )
    {
        T wtemp = iSign * SineInfSeries<1,N,T>::val();

        T wr = 1.0;
        T wi = 0.0;
        T wpr = -2.0 * wtemp * wtemp;
        T wpi = -iSign * SineInfSeries<2,N,T>::val();

        R.recur( x, iSign );
        R.recur( x + N, iSign );

        for( int i=0; i<N; i+=2 )
        {
            int iN = i + N;

            T tempr = x[ iN ] * wr - x[ iN + 1 ] * wi;
            T tempi = x[ iN ] * wi + x[ iN + 1 ] * wr;

            x[ iN ] = x[ i ] - tempr;
            x[ iN + 1 ] = x[ i + 1 ] - tempi;

            x[ i ] += tempr;
            x[ i + 1 ] += tempi;

            wtemp = wr;
            wr += wr*wpr - wi*wpi;
            wi += wi*wpr + wtemp*wpi;
        } // for( int i=0; i<N; i+=2 )
    } // recur

protected:
    DanLanRecurant<N/2,T> R;
}; // class DanLanRecurant

template<typename T>
class DanLanRecurant<1,T> // Danielson-Lanczos Recursion
{
public:
    inline void recur( T* x, int iSign ) { }
};

template<int N, typename T>
class FFT
{
public:
    // in-place FFT for powers of two length x stored in [real,imag,][real,imag,]... flat format
    inline void fft( T* x )
    {
        revbin( x );
        danlan.recur( x, 1 );
    }

    // in-place IFFT for powers of two length x stored in [real,imag,][real,imag,]... flat format
    inline void ifft( T* x )
    {
        revbin( x );
        danlan.recur( x, -1 );
        scale( x );
    }

protected:
    inline void revbin( T* x )
    {
        for( int n=1, j=1; n < 2*N; n += 2 )
        {
            if (j>n) {
                T tmp = x[j-1];
                x[j-1] = x[n-1];
                x[n-1] = tmp;

                tmp = x[ j ];
                x[ j ] = x[ n ];
                x[ n ] = tmp;
            }

            int m = N;
            while( m >= 2 && j > m )
            {
                j -= m;
                m >>= 1;
            }
            j += m;
        } // for (int i=1; i<2*N; i+=2)
    } // void revbin(T* x)

    inline void scale( T* x )
    {
        /*  scale all results by 1/n */
        T scale = static_cast<T>(1)/N;
        for( int i=0; i<2*N; i++ )
        {
            x[i] *= scale;
        }
    } // void scale( T* x )

protected:
    DanLanRecurant<N,T> danlan;
}; // class FFT