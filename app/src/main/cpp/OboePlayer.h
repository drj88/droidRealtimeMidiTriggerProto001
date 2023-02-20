#pragma once

#include <thread>
#include <oboe/Oboe.h>
#include <oboe/Utilities.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include "log.h"
#include "util.h"

// #include "oboe/FifoBuffer.h"
#include "LockFreeQueue.h"

#define PLAYER_BUFFER_LENGTH_DEFAULT 1024

class OboePlayer : oboe::AudioStreamDataCallback
{
private:
    bool                                    playing;
    struct timespec                         now;
    oboe::AudioStreamBuilder                asb;
    oboe::AudioStream*                      as;
    oboe::AudioFormat                       af;
    std::thread                             th;
    double                                  R; // samples per second
    float*                                  out;
    int16_t*                                out16;
    std::mutex                              lockStartStop;
    std::mutex                              lockCopyAudio;
    int                                     bufferLen;
    double                                  requestR;
protected:
    int64_t nanos() {
        clock_gettime(CLOCK_MONOTONIC, &this->now);
        return (int64_t) this->now.tv_sec*1000000000LL + this->now.tv_nsec;
    }
public:
    OboePlayer() {
        this->R = 0.0;
        this->requestR = 0.0;
        this->playing = false;
        this->bufferLen = PLAYER_BUFFER_LENGTH_DEFAULT;
        this->out = new float[ this->bufferLen ];
        this->out16 = new int16_t[ this->bufferLen ];
    }
    ~OboePlayer() {
        delete[] this->out;
        delete[] this->out16;
    }
    OboeCustomReturns start() {
        std::lock_guard<std::mutex> guard(this->lockStartStop);
        OboeCustomReturns ret = OboeCustomReturns::Success;
        if( !this->playing )
        {
            this->asb.setChannelCount(1);
            this->asb.setDataCallback(this);
            //this->asb.setSampleRate( DEFAULT_REQUEST_SAMPLING_RATE );
            this->asb.setDirection(oboe::Direction::Output);
            this->asb.setAudioApi(oboe::AudioApi::AAudio);
            this->asb.setFormat(oboe::AudioFormat::Unspecified);
            this->asb.setSharingMode(oboe::SharingMode::Exclusive);
            this->asb.setPerformanceMode(oboe::PerformanceMode::LowLatency);
            oboe::Result r = this->asb.openStream( &this->as );
            if( r == oboe::Result::OK)
            {
                r = this->as->requestStart();
                if( r == oboe::Result::OK) {


                    //if( this->R == (double)DEFAULT_REQUEST_SAMPLING_RATE ) {
                    this->playing = true;


                    ret = OboeCustomReturns::Success;
//                    } else {
//                        //LOGE("%s: Failed to start playing stream. Error: %s", APP_NAME, convertToText(r));
//                        LOGE("%s: Failed to start playing stream.", APP_NAME);
//                        ret = OboeCustomReturns::SamplingRateRequestFailed;
//                    }
                } // if( r == Result::OK) for requestStart()
                else {
                    LOGE("%s: Failed to start playing stream. Error: %s", APP_NAME, convertToText(r));
                    ret = OboeCustomReturns::OpenStreamFailed;
                }
            } // if( r == Result::OK) for for openStream()
            else {
                LOGE("%s: Failed to open playing stream. Error: %s", APP_NAME, convertToText(r));

                ret = OboeCustomReturns::OpenStreamFailed;
            }
        }
        else
            ret = OboeCustomReturns::Success;
        return ret;
    }
    void stop() {
        this->playing = false;
    }
    bool live() {
        return this->playing;
    }
    double samplingRate() {
        return this->R;
    }
    int getBufferLength() {
        return this->bufferLen;
    }
    oboe::DataCallbackResult onAudioReady( oboe::AudioStream *oboeStream, void *audioData, int32_t numFrames ) {
        oboe::DataCallbackResult ret;
        if( this->playing ) {
            int numChannels = this->as->getChannelCount();//oboe::AAudioStream_getChannelCount( this->as );
            this->R = this->as->getSampleRate();
            this->af = this->as->getFormat();
            if( this->af == oboe::AudioFormat::I16 ) {
                int16_t *output = (int16_t *) audioData;
                oboe::convertFloatToPcm16(&this->out[0], &this->out16[0], this->bufferLen);
                for( int frameIndex = 0; frameIndex < numFrames && frameIndex < this->bufferLen; frameIndex++ ) {
                    for (int channelIndex = 0; channelIndex < numChannels; channelIndex++) {
                        *output++ = out16[ frameIndex ];
                    }
                }
                ret = oboe::DataCallbackResult::Continue;
            } else if( this->af == oboe::AudioFormat::Float ) {
                float *output = (float *) audioData;
                for( int frameIndex = 0; frameIndex < numFrames && frameIndex < this->bufferLen; frameIndex++ ) {
                    for( int channelIndex = 0; channelIndex < numChannels; channelIndex++ ) {
                        *output++ = this->out[ frameIndex ];
                    }
                }
                ret = oboe::DataCallbackResult::Continue;
            } else {
                LOGE("Unsupported audio format requested: %s", oboe::convertToText( this->af ) );
                ret = oboe::DataCallbackResult::Stop;
            }
        } else {
            LOGI( "onAudioReady playing flag set to false, stopping playback from callback" );
            ret = oboe::DataCallbackResult::Stop;
        }
        return ret;
    }
};