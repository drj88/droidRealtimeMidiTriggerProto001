#pragma once

#include "log.h"
#include <string>
//#include <fcntl.h>
//#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <vector>

class FileDevDumper
{
private:
    //int         fd;
    std::string path;
    std::ofstream fdWav;
    size_t data_chunk_pos;

    void write_word( std::ostream& outs, int value, unsigned size = sizeof( int ) )
    {
        for (; size; --size, value >>= 8)
            outs.put( static_cast <char> (value & 0xFF) );
    }

public:
    FileDevDumper() {}

    FileDevDumper( std::string fileNamePath ) {
        this->path = fileNamePath;
        //this->fd = open( this->path, O_CREAT | O_APPEND | O_WRITE );
    }

    ~FileDevDumper( ) { }

public:
    void setPath( std::string fileNamePath ) {
        this->path = fileNamePath;
    }
    bool openResetTextFile()
    {
        std::ofstream F( this->path.c_str(),std::ios_base::trunc | std::ios_base::out );
        if( F.is_open() ) {
            F.close();
            return true;
        }
        return false;
    }

    bool writeAppendCSV( std::vector<float>& values, bool csvRowPerCall = false )
    {
        return writeAppendCSV( (float*)values.data(), values.size(), csvRowPerCall );
    }

    bool writeAppendCSV( float* values, int N, bool csvRowPerCall = false )
    {
        std::ofstream F(this->path.c_str(), std::ios_base::app | std::ios_base::out );
        if (F.is_open()) {
            for (int n = 0; n < N; ++n) {
                if (csvRowPerCall) {
                    if (n < N - 1)
                        F << values[n] << ",";
                    else
                        F << values[n];
                } else
                    F << values[n] << "\n";
            }
            if (csvRowPerCall) F << "\n";
            F.close();
            return true;
        }
        return false;
    }

    bool wavOpened( )
    {
        return fdWav.is_open();
    }

    bool openWav( int numChannels, int recordingFreq, int bitsPerSample ) {
        fdWav.open(path, std::ios::binary | std::ios_base::trunc | std::ios_base::out);
        if( !fdWav.is_open() ) {
            LOGE("File not opened!");
            return false;
        }
        // Write the file headers
        fdWav << "RIFF----WAVEfmt ";     // (chunk size to be filled in later)
        write_word( fdWav,     16, 4 );  // no extension data
        write_word( fdWav,      1, 2 );  // PCM - integer samples
        write_word( fdWav,      numChannels, 2 );  // one channel (mono) or two channels (stereo file)
        write_word( fdWav,  recordingFreq, 4 );  // samples per second (Hz)
        //write_word( fdWav, 176400, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
        write_word( fdWav,(recordingFreq * bitsPerSample * numChannels) / 8, 4 );  // (Sample Rate * BitsPerSample * Channels) / 8
        write_word( fdWav,      4, 2 );  // data block size (size of two integer samples, one for each channel, in bytes)
        write_word( fdWav,     bitsPerSample, 2 );  // number of bits per sample (use a multiple of 8)

        // Write the data chunk header
        data_chunk_pos = fdWav.tellp();
        fdWav << "data----";  // (chunk size to be filled in later)

        return true;
    }

    void writeWavData( int data ) {
        write_word( fdWav, (int)(data), 2 );
    }

    void writeWavData( std::int16_t* data, int len ) {
        fdWav.write( (char*)&data[0], len * sizeof( std::int16_t ) );
    }

    void closeWav( )
    {
        // (We'll need the final file size to fix the chunk sizes above)
        size_t file_length = fdWav.tellp();

        // Fix the data chunk header to contain the data size
        fdWav.seekp( data_chunk_pos + 4 );
        write_word( fdWav, file_length - data_chunk_pos + 8 );

        // Fix the file header to contain the proper RIFF chunk size, which is (file size - 8) bytes
        fdWav.seekp( 0 + 4 );
        write_word( fdWav, file_length - 8, 4 );
        fdWav.close();
    }
};