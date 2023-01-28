#pragma once
#include <stack>
#include <queue>
#include <thread>
#include <oboe/Oboe.h>
#include <oboe/Utilities.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include "log.h"
#include "util.h"
#include "FileDevDumper.h"
#include "LockFreeQueue.h"

#define DEFAULT_QUEUE_LENGTH 64
#define DEFAULT_RECORDER_BUFFER_LENGTH 256
#define DEFAULT_REQUEST_SAMPLING_RATE 11025

class OboeRecorder
{
private:
    bool                                    recording;
    struct timespec                         now;
    oboe::AudioStreamBuilder                asb;
    oboe::AudioStream*                      pas;
    oboe::AudioFormat                       af;
    std::thread                             th;
    double                                  R; // samples per second
    std::vector<std::int16_t*>              B; // all allocated buffers
    LockFreeQueue<std::int16_t*,DEFAULT_QUEUE_LENGTH,unsigned int>   Q;
    LockFreeQueue<std::int16_t*,DEFAULT_QUEUE_LENGTH,unsigned int>   F;
    std::int16_t*                           in;
    std::mutex                              lockStartAudio;
    int                                     bufferLen;
    double                                  requestR;
    FileDevDumper                           wav;
    bool                                    wavActive;

public:
    OboeRecorder() {
        this->R = 0.0;
        this->requestR = 0.0;
        this->recording = false;
        this->wavActive = false;
        this->bufferLen = DEFAULT_RECORDER_BUFFER_LENGTH;
        this->in = new std::int16_t[ this->bufferLen ];
        B.resize( DEFAULT_QUEUE_LENGTH );
        for( int n=0; n<DEFAULT_QUEUE_LENGTH; ++n ) {
            std::int16_t* b = new std::int16_t[ this->bufferLen ];
            this->F.push( b );
            this->B[n] = b;
        }
    }
    ~OboeRecorder() {
        delete[] this->in;
        for( int n=0; n<DEFAULT_QUEUE_LENGTH; ++n ) {
            std::int16_t* b = this->B[n];
            delete[] b;
        }
    }
protected:
    int64_t nanos() {
        clock_gettime(CLOCK_MONOTONIC, &this->now);
        return (int64_t) this->now.tv_sec*1000000000LL + this->now.tv_nsec;
    }
    void lockCopyAudioToOutput( ) {
        std::int16_t* b;
        if( F.pop( b ) ) {
            memcpy( (void*)b, (void*)this->in, sizeof( int16_t ) * this->bufferLen );
            Q.push( b );
        } else {
            LOGE("OboeRecorder no buffers");
        }
    }
    void run() {
        std::int64_t msTimeout = ((double)this->bufferLen / this->R * 1000) + 1;
        std::int64_t nsTimeoutOboe = msTimeout * oboe::kNanosPerMillisecond;
        std::int64_t nsTimeoutDropout = 2 * nsTimeoutOboe;
        this->af = this->pas->getFormat();
        if( this->af == oboe::AudioFormat::I16 ) {
            if( this->wavActive) { wav.openWav(1, (int) this->R, 16); }
            int samplesRead, samplesRequest, reqCount;
            std::int64_t nsStart, ns;
            while( this->recording ) {
                ns = 0;
                reqCount = 0;
                samplesRead = 0;
                nsStart = nanos();
                while( this->recording && samplesRead < this->bufferLen && ns < nsTimeoutDropout ) {
                    samplesRequest = this->bufferLen - samplesRead;
                    auto r = pas->read(&this->in[samplesRead], samplesRequest, nsTimeoutOboe );
                    if (r == oboe::Result::OK) {
                        samplesRead += r.value();
                    } else {
                        LOGE("OboeRecorder error %s", convertToText(r.error()));
                    }
                    ns = nanos() - nsStart;
                    ++reqCount;
                }
                if(ns >= nsTimeoutDropout ) {
                    LOGE( "OboeRecorder dropout reqs: %d %d read: %d ns: %jd nsd: %jd nso: %jd",
                          reqCount, this->bufferLen, samplesRead, ns, ns-nsTimeoutDropout, nsTimeoutOboe );
                }
                if (samplesRead < this->bufferLen) {
                    LOGE("OboeRecorder short reqs: %d %d read: %d ns: %jd nsd: %jd nso: %jd",
                         reqCount, this->bufferLen, samplesRead, ns, ns-nsTimeoutDropout, nsTimeoutOboe );

                    // fill the rest with zeros... (LAME!)
                    for( int n=samplesRead; n < this->bufferLen; ++n ) {
                        this->in[n] = 0;
                    }
                } else {
                    if (samplesRead > this->bufferLen) {
                        LOGE("OboeRecorder over reqs: %d %d read: %d ns: %jd nsd: %jd nso: %jd",
                             reqCount, this->bufferLen, samplesRead, ns, ns-nsTimeoutDropout, nsTimeoutOboe );
                        this->recording = false; // stop immediately, oboe has bug
                    }
                }
                this->lockCopyAudioToOutput();
                if( this->wavActive) { wav.writeWavData( this->in, this->bufferLen ); }
            }
        } else {
            LOGE("Oboe returned non-I16 format");
        }
        this->pas->close();
        if( this->wavActive) { wav.closeWav(); }
    }

public:
    void setWavPath( std::string path ) {
        this->wavActive = true;
        wav.setPath( path );
    }
    void disableWav( ) {
        this->wavActive = false;
    }
    OboeCustomReturns start() {
        std::lock_guard<std::mutex> guard( this->lockStartAudio );
        OboeCustomReturns ret = OboeCustomReturns::Success;
        if(!this->recording)
        {
            this->asb.setChannelCount(1);
            this->asb.setSampleRate( DEFAULT_REQUEST_SAMPLING_RATE );
            this->asb.setDirection(oboe::Direction::Input);
            this->asb.setAudioApi(oboe::AudioApi::Unspecified);
            this->asb.setFormat(oboe::AudioFormat::Unspecified);
            this->asb.setFormat(oboe::AudioFormat::I16);
            this->asb.setSharingMode(oboe::SharingMode::Exclusive);
            this->asb.setPerformanceMode(oboe::PerformanceMode::LowLatency);
            this->asb.setInputPreset(oboe::InputPreset::VoiceRecognition);
            this->asb.setFormatConversionAllowed(false);
            this->asb.setChannelConversionAllowed(false);
            this->asb.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::None);
//            this->asb.setBufferCapacityInFrames( this->bufferLen );
//            this->asb.setFramesPerCallback( this->bufferFrameLen );

            oboe::Result r = this->asb.openStream( &this->pas );
            if( r == oboe::Result::OK)
            {
                r = this->pas->requestStart();
                if( r == oboe::Result::OK) {
                    this->R = this->pas->getSampleRate();

                    LOGD("OboeRecorder AudioAPI = %d, channelCount = %d, deviceID = %d",
                         this->pas->getAudioApi(),
                         this->pas->getChannelCount(),
                         this->pas->getDeviceId());


                    //if( this->R == (double)DEFAULT_REQUEST_SAMPLING_RATE ) {
                    this->recording = true;

                    id_t pid = getpid();
                    setpriority(PRIO_PROCESS, pid, THREAD_PRIORITY_AUDIO);
                    this->th = std::thread(&OboeRecorder::run, this );
                    this->th.detach();

                    ret = OboeCustomReturns::Success;
                } // if( r == Result::OK) for requestStart()
                else {
                    LOGE("OboeRecorder %s: Failed to start recording stream. Error: %s", APP_NAME, convertToText(r));
                    ret = OboeCustomReturns::OpenStreamFailed;
                }
            } // if( r == Result::OK) for for openStream()
            else {
                LOGE("OboeRecorder %s: Failed to open recording stream. Error: %s", APP_NAME, convertToText(r));
                ret = OboeCustomReturns::OpenStreamFailed;
            }
        }
        else
            ret = OboeCustomReturns::Success;
        return ret;
    }
    void stop() {
        this->recording = false;
    }
    void getAudio( float* dest ) {
        std::int16_t* b;
        bool bPopped = false;
        std::int64_t nss = nanos(), ns = 0;
        std::int64_t msTimeout = ((double)this->bufferLen / this->R * 1000) + 1;
        std::int64_t nsTimeoutGetAudio = msTimeout * oboe::kNanosPerMillisecond;
        while( !(bPopped = Q.pop( b )) && ns < nsTimeoutGetAudio ) {
            ns = nss - nanos();
        }
        if( ns > nsTimeoutGetAudio ) {
            for( int n=0; n < this->bufferLen; ++n ) dest[n] = 0;
            LOGI("OboeRecorder getAudio empty buffers timeout");
        }
        if( bPopped ) {
            oboe::convertPcm16ToFloat( b, dest, this->bufferLen );
            F.push(b);
        } else {
            for( int n=0; n < this->bufferLen; ++n ) dest[n] = 0;
            LOGI("OboeRecorder getAudio zeros returned");
        }
    }
    bool live() {
        return this->recording;
    }
    double samplingRate() {
        return this->R;
    }
    int getBufferLength() {
        return this->bufferLen;
    }
};