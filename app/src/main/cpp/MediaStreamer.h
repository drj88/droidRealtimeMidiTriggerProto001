#pragma once

#include <vector>
#include <fcntl.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <sys/stat.h>

typedef struct {
    int trackNum;
    int sampleRate;
    int bitsPerSample; // only in API 29 and up
} MediaStreamerTrackInfo;

class MediaStreamer
{
private:
    int fd;
    AMediaExtractor* ex;
    AMediaCodec *codec;
    int64_t renderstart;
    bool sawInputEOS;
    bool sawOutputEOS;
    bool isPlaying;
    bool renderonce;
    std::vector<MediaStreamerTrackInfo> tracks;
private:
    void stopStreaming() {
        tracks.clear();
        if( this->codec != NULL ) {
            AMediaCodec_stop(this->codec);
            AMediaCodec_delete(this->codec);
            this->codec = NULL;
        }
        if( this->ex != NULL ) {
            AMediaExtractor_delete(this->ex);
            this->ex = NULL;
        }
        this->sawInputEOS = true;
        this->sawOutputEOS = true;
    }

public:
    MediaStreamer() {
        this->fd = -1;
        this->ex = NULL;
        this->codec = NULL;
        this->renderstart = 0;
        this->sawInputEOS = false;
        this->sawOutputEOS = false;
        this->isPlaying = false;
        this->renderonce = false;

    }
    ~MediaStreamer() {
        stopStreaming();
    }
public:
    bool openFile( const char* path )
    {
        tracks.clear();
        bool ret = false;
        struct stat st;
        int statRet = stat( path, &st );
        this->fd = open( path, O_RDONLY );
        if( statRet != 0 || this->fd == -1 ) {
            ret = false;
        } else {
            off_t outStart = (off_t)0;
            off_t outLen = (off_t)st.st_size;
            this->ex = AMediaExtractor_new();
            media_status_t err =
                AMediaExtractor_setDataSourceFd(
                    this->ex,
                    this->fd,
                    static_cast<off64_t>(outStart),
                    static_cast<off64_t>(outLen)
                );
            close( this->fd );
            this->fd = -1;
            if( err == AMEDIA_OK ) {
                int numtracks = AMediaExtractor_getTrackCount(this->ex);

                LOGV("input has %d tracks", numtracks);
                for (int i = 0; i < numtracks; i++) {
//                for (int i = 0; i < 1; i++) {
                    AMediaFormat *format = AMediaExtractor_getTrackFormat( this->ex, i ); // causes fault in debugger but runs??
                    const char *s = AMediaFormat_toString( format );

                    LOGV("track %d format: %s", i, s);

                    int32_t sampleRate;
                    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate)){
                        LOGD("Source sample rate %d", sampleRate);
                    } else {
                        LOGE("Failed to get sample rate");
                    };

                    int32_t bitsPerSample = 16;
//                    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_BITS_PER_SAMPLE, &bitsPerSample)){ // AMEDIAFORMAT_KEY_BITS_PER_SAMPLE
//                        LOGD("Source bits per sample %d", bitsPerSample);
//                    } else {
//                        LOGE("Failed to get bits per sample");
//                    };

                    MediaStreamerTrackInfo tnfo;
                    tnfo.trackNum = (int)i;
                    tnfo.sampleRate = (int)sampleRate;
                    tnfo.bitsPerSample = (int)bitsPerSample;

                    const char *mime;
                    if( !AMediaFormat_getString( format, AMEDIAFORMAT_KEY_MIME, &mime ) ) {
                        LOGV("no mime type");
                        ret = false;
                    } else if (!strncmp(mime, "audio/", 6)) {
                        // Omitting most error handling for clarity.
                        // Production code should check for errors.
                        AMediaExtractor_selectTrack( this->ex, i );
                        this->codec = AMediaCodec_createDecoderByType( mime );
                        AMediaCodec_configure( this->codec, format, NULL, NULL, 0 ); // AMediaCodec_configure(codec, format, this->window, NULL, 0);
                        //this->ex = ex;
                        this->renderstart = -1;
                        this->sawInputEOS = false;
                        this->sawOutputEOS = false;
                        this->isPlaying = false;
                        this->renderonce = true;
                        AMediaCodec_start( this->codec );
                        ret = true;
                    }
                    AMediaFormat_delete( format );
                }
            } else {
                LOGV("setDataSource error: %d", err);
                ret = false;
            }
        }

        return ret;
    }

    /*
    void open( const char* utf8Filename )
    {
        // convert Java string to UTF-8
        const char *utf8 = env->GetStringUTFChars(filename, NULL);
        LOGV("opening %s", utf8);

        off_t outStart, outLen;
        int fd = AAsset_openFileDescriptor(
                AAssetManager_open(
                        AAssetManager_fromJava(env, assetMgr),
                        utf8,
                        0
                ),
                &outStart,
                &outLen
        );

        env->ReleaseStringUTFChars(filename, utf8);
        if (fd < 0) {
            LOGE("failed to open file: %s %d (%s)", utf8, fd, strerror(errno));
            return JNI_FALSE;
        }

        data.fd = fd;

        MediaStreamerWorkData *d = &data;

        AMediaExtractor *ex = AMediaExtractor_new();
        media_status_t err =
            AMediaExtractor_setDataSourceFd(
                    ex,
                    d->fd,
                 static_cast<off64_t>(outStart),
                 static_cast<off64_t>(outLen));
        close(d->fd);
        if (err != AMEDIA_OK) {
            LOGV("setDataSource error: %d", err);
            return JNI_FALSE;
        }

        int numtracks = AMediaExtractor_getTrackCount(ex);

        AMediaCodec *codec = NULL;

        LOGV("input has %d tracks", numtracks);
        for( int i = 0; i < numtracks; i++ ) {
            AMediaFormat *format = AMediaExtractor_getTrackFormat(ex, i);
            const char *s = AMediaFormat_toString(format);
            LOGV("track %d format: %s", i, s);
            const char *mime;
            if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
                LOGV("no mime type");
                return JNI_FALSE;
            } else if (!strncmp(mime, "video/", 6)) {
                // Omitting most error handling for clarity.
                // Production code should check for errors.
                AMediaExtractor_selectTrack(ex, i);
                codec = AMediaCodec_createDecoderByType(mime);
                AMediaCodec_configure(codec, format, d->window, NULL, 0);
                d->ex = ex;
                d->codec = codec;
                d->renderstart = -1;
                d->sawInputEOS = false;
                d->sawOutputEOS = false;
                d->isPlaying = false;
                d->renderonce = true;
                AMediaCodec_start(codec);
            }
            AMediaFormat_delete(format);
        }
    }

    int32_t decode( char *file, uint8_t *targetData, AudioProperties targetProperties) {

        LOGD("Using NDK decoder: %s",file);

        // Extract the audio frames
        AMediaExtractor *extractor = AMediaExtractor_new();
        //using this method instead of AMediaExtractor_setDataSourceFd() as used for asset files in the rythem game example
        media_status_t amresult = AMediaExtractor_setDataSource(extractor, file);


        if (amresult != AMEDIA_OK) {
            LOGE("Error setting extractor data source, err %d", amresult);
            return 0;
        }
        // Specify our desired output format by creating it from our source
        AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, 0);

        int32_t sampleRate;
        if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &sampleRate)) {
            LOGD("Source sample rate %d", sampleRate);
            if (sampleRate != targetProperties.sampleRate) {
                LOGE("Input (%d) and output (%d) sample rates do not match. "
                     "NDK decoder does not support resampling.",
                     sampleRate,
                     targetProperties.sampleRate);
                return 0;
            }
        } else {
            LOGE("Failed to get sample rate");
            return 0;
        };

        int32_t channelCount;
        if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channelCount)) {
            LOGD("Got channel count %d", channelCount);
            if (channelCount != targetProperties.channelCount) {
                LOGE("NDK decoder does not support different "
                     "input (%d) and output (%d) channel counts",
                     channelCount,
                     targetProperties.channelCount);
            }
        } else {
            LOGE("Failed to get channel count");
            return 0;
        }

        const char *formatStr = AMediaFormat_toString(format);
        LOGD("Output format %s", formatStr);

        const char *mimeType;
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mimeType)) {
            LOGD("Got mime type %s", mimeType);
        } else {
            LOGE("Failed to get mime type");
            return 0;
        }

        // Obtain the correct decoder
        AMediaCodec *codec = nullptr;
        AMediaExtractor_selectTrack(extractor, 0);
        codec = AMediaCodec_createDecoderByType(mimeType);
        AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
        AMediaCodec_start(codec);

        // DECODE

        bool isExtracting = true;
        bool isDecoding = true;
        int32_t bytesWritten = 0;

        while (isExtracting || isDecoding) {

            if (isExtracting) {

                // Obtain the index of the next available input buffer
                ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec, 2000);
                //LOGV("Got input buffer %d", inputIndex);

                // The input index acts as a status if its negative
                if (inputIndex < 0) {
                    if (inputIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                        // LOGV("Codec.dequeueInputBuffer try again later");
                    } else {
                        LOGE("Codec.dequeueInputBuffer unknown error status");
                    }
                } else {

                    // Obtain the actual buffer and read the encoded data into it
                    size_t inputSize;
                    uint8_t *inputBuffer = AMediaCodec_getInputBuffer(codec, inputIndex, &inputSize);
                    //LOGV("Sample size is: %d", inputSize);

                    ssize_t sampleSize = AMediaExtractor_readSampleData(extractor, inputBuffer, inputSize);
                    auto presentationTimeUs = AMediaExtractor_getSampleTime(extractor);

                    if (sampleSize > 0) {

                        // Enqueue the encoded data
                        AMediaCodec_queueInputBuffer(codec, inputIndex, 0, sampleSize, presentationTimeUs, 0);
                        AMediaExtractor_advance(extractor);

                    } else {
                        LOGD("End of extractor data stream");
                        isExtracting = false;

                        // We need to tell the codec that we've reached the end of the stream
                        AMediaCodec_queueInputBuffer(codec, inputIndex, 0, 0, presentationTimeUs, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    }
                }
            }

            if (isDecoding) {
                // Dequeue the decoded data
                AMediaCodecBufferInfo info;
                ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);

                if (outputIndex >= 0) {

                    // Check whether this is set earlier
                    if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                        LOGD("Reached end of decoding stream");
                        isDecoding = false;
                    } else {
                        // Valid index, acquire buffer
                        size_t outputSize;
                        uint8_t *outputBuffer = AMediaCodec_getOutputBuffer(codec, outputIndex, &outputSize);

//                        LOGV("Got output buffer index %d, buffer size: %d, info size: %d writing to pcm index %d",
//                             outputIndex,
//                             outputSize,
//                             info.size,
//                             m_writeIndex);

                        // copy the data out of the buffer
                        memcpy(targetData + bytesWritten, outputBuffer, info.size);
                        bytesWritten += info.size;
                        AMediaCodec_releaseOutputBuffer(codec, outputIndex, false);
                    }

                } else {
                    // The outputIndex doubles as a status return if its value is < 0
                    switch (outputIndex) {
                        case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                            LOGD("dequeueOutputBuffer: try again later");
                            break;
                        case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                            LOGD("dequeueOutputBuffer: output buffers changed");
                            break;
                        case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
                            LOGD("dequeueOutputBuffer: output outputFormat changed");
                            format = AMediaCodec_getOutputFormat(codec);
                            LOGD("outputFormat changed to: %s", AMediaFormat_toString(format));
                            break;
                    }
                }
            }
        }

        // Clean up
        AMediaFormat_delete(format);
        AMediaCodec_delete(codec);
        AMediaExtractor_delete(extractor);
        return bytesWritten;
    }
    */
};