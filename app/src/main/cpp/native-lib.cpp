#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <string>
#include "dsp.h"
#include "log.h"
#include "OboeRecorder.h"
#include "OboePlayer.h"
#include "MediaStreamer.h"
#include "LockFreeQueue.h"
#include "cpu.h"
#include "FileDevDumper.h"
#include "fft_test.h"

#define DEBUG_FILE_DUMPS

#ifdef DEBUG_FILE_DUMPS
FileDevDumper fDspOut( "/sdcard/dump/dump.dspapp.dspout.txt" );
FileDevDumper fDspOutNacIndices("/sdcard/dump/dump.dspapp.dspout.nacs.txt" );
FileDevDumper fDspOutPitchEst( "/sdcard/dump/dump.dspapp.dspout.pitchest.txt" );
FileDevDumper fDspOutPitchMidi( "/sdcard/dump/dump.dspapp.dspout.pitchmidi.txt" );
FileDevDumper fDspOutMidiNoteNum( "/sdcard/dump/dump.dspapp.dspout.midinotenum.txt" );
#endif

MediaStreamer media;
OboeRecorder recorder;
DSP* dspProcessor = NULL;
int dspProcessingMode = 0;
int outputBufferLen = 0;

// MainActivity class native JNI functions
extern "C" {
    JNIEXPORT jboolean JNICALL Java_com_yourdomain_yourapp_MainActivity_isRecording(JNIEnv *env, jobject thiz) {
        return recorder.live();
    }

    JNIEXPORT jint JNICALL Java_com_yourdomain_yourapp_MainActivity_startRecording(JNIEnv *env, jobject /* this */) {
//        recorder.setWavPath("/sdcard/dump/recorder.wav");
//        recorder.setWavPath("/storage/emulated/0/dump/recorder.wav");
//        recorder.setWavPath("/data/local/tmp/recorder.wav");
        recorder.setWavPath("/data/data/com.yourdomain.yourapp/recorder.wav");
        return (int) recorder.start();
    }

    JNIEXPORT void JNICALL Java_com_yourdomain_yourapp_MainActivity_stopRecording(JNIEnv *env, jobject /* this */) {
        recorder.stop();
        if( dspProcessor != NULL ) {
            delete dspProcessor;
            dspProcessor = NULL;
        }
    }

    JNIEXPORT void JNICALL Java_com_yourdomain_yourapp_MainActivity_setProcessingMode(JNIEnv *env, jobject thiz, jint mode) {
        #ifdef DEBUG_FILE_DUMPS
            fDspOut.openResetTextFile();
            fDspOutNacIndices.openResetTextFile();
            fDspOutPitchEst.openResetTextFile();
            fDspOutPitchMidi.openResetTextFile();
            fDspOutMidiNoteNum.openResetTextFile();
        #endif

        if( dspProcessor != NULL ) {
            delete dspProcessor;
        }
        switch( mode ) {
            default:
            case ProcessingModes::RawAudioOnly:
                dspProcessingMode = (int)ProcessingModes::RawAudioOnly;
                dspProcessor = NULL;
                outputBufferLen = recorder.getBufferLength();
                break;
            case ProcessingModes::MagnitudeSpectrum:
                dspProcessingMode = (int)ProcessingModes::MagnitudeSpectrum;
                dspProcessor = (DSP*) new FastFourierTransformMagnitudeSpectrum(recorder.getBufferLength());
                outputBufferLen = dspProcessor->getProcessOutputLen();
                break;
            case ProcessingModes::Autocorrelation:
                dspProcessingMode = (int)ProcessingModes::Autocorrelation;
                dspProcessor = (DSP*) new AutocorrelationNormalized(recorder.getBufferLength());
                outputBufferLen = dspProcessor->getProcessOutputLen();
                break;
            case ProcessingModes::PitchEstimation:
                dspProcessingMode = (int)ProcessingModes::PitchEstimation;
                dspProcessor = (DSP*) new PitchEstimator2(recorder.getBufferLength());
                outputBufferLen = dspProcessor->getProcessOutputLen();
                break;
        }
    }

    JNIEXPORT jfloat JNICALL Java_com_yourdomain_yourapp_MainActivity_getPitchEstimate(JNIEnv *env, jobject thiz) {
        if( dspProcessor != NULL ) {
            return dspProcessor->getPitch();
        } else {
            return 0.f;
        }
    }

    JNIEXPORT jint JNICALL Java_com_yourdomain_yourapp_MainActivity_getMidiNoteNumber(JNIEnv *env, jobject thiz) {
        if( dspProcessor != NULL ) {
            return dspProcessor->getMidiNoteNumber();
        } else {
            return 0.f;
        }
    }
}

// SurfaceViewDSP class native JNI functions
extern "C" {
    static float* __hopper = NULL;
    static float* __dspOutput = NULL;
    static jfloatArray __audioDataJVM = NULL;

    JNIEXPORT jint JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_getProcessingMode(JNIEnv *env, jobject thiz) {
        return dspProcessingMode;
    }

    JNIEXPORT void JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_initAudioBridge(JNIEnv *env, jobject /* this */) {
        if (__audioDataJVM == nullptr) {
            jfloatArray jArray = env->NewFloatArray(outputBufferLen);
            __audioDataJVM = (jfloatArray) env->NewGlobalRef(jArray);
            __dspOutput = new float[ outputBufferLen ];
            __hopper = new float[ outputBufferLen ];
        }
        assert(__audioDataJVM != NULL);
    }

    JNIEXPORT void JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_freeAudioBridge(JNIEnv *env, jobject /* this */) {
        if (__audioDataJVM != nullptr) {
            jfloatArray ref = __audioDataJVM;
            __audioDataJVM = NULL;
            env->DeleteGlobalRef(ref);
            delete[] __hopper;
            delete[] __dspOutput;
            __hopper = NULL;
            __dspOutput = NULL;
        }
    }

    JNIEXPORT jfloatArray JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_getDspOutput(JNIEnv *env, jobject thiz) {
        assert( __audioDataJVM != NULL);
        assert(__dspOutput != NULL);
        assert(__hopper != NULL);
        int64_t nsStart = 0LL, nsEnd = 0LL;
        nsStart = cnanos();
        if (recorder.live()) {
            recorder.getAudio( &__hopper[0] );
            if( dspProcessor != NULL ) {
                dspProcessor->setSamplingRate(recorder.samplingRate());
                dspProcessor->process( &__hopper[0], recorder.getBufferLength(), &__dspOutput[0] );
                for( int n=0; n < outputBufferLen; ++n ) __hopper[n] = __dspOutput[n];

                #ifdef DEBUG_FILE_DUMPS
                float pitchEst = dspProcessor->getPitch();
                float nacIndex = dspProcessor->getNacIndex();
                float pitchMidi = dspProcessor->getPitchMidi();
                float midiNoteNum = dspProcessor->getMidiNoteNumber();
                fDspOut.writeAppendCSV( &__hopper[0], outputBufferLen, true );
                fDspOutPitchEst.writeAppendCSV( &pitchEst, 1, false );
                fDspOutNacIndices.writeAppendCSV( &nacIndex, 1, false );
                fDspOutPitchMidi.writeAppendCSV( &pitchMidi, 1, false );
                fDspOutMidiNoteNum.writeAppendCSV( &midiNoteNum, 1, false );
                #endif
            }
        } else {
            for( int n=0; n < outputBufferLen; ++n ) __hopper[n] = 0;
            LOGI("getAudio: Recorder NOT recording, returning zero-filled result");
        }
        env->SetFloatArrayRegion( __audioDataJVM, 0, outputBufferLen, &__hopper[0] );
        nsEnd = cnanos();
        int ns = (int) (nsEnd - nsStart);
//        LOGI("getDspOutput %d ns", ns);
        return __audioDataJVM;
    }

    JNIEXPORT jfloat JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_getPitch(JNIEnv *env, jobject thiz) {
        if( dspProcessor != NULL ) {
            return dspProcessor->getPitch();
        } else {
            return 0.f;
        }
    }

    JNIEXPORT jfloat JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_getPitchMidi(JNIEnv *env, jobject thiz) {
        if( dspProcessor != NULL ) {
            return dspProcessor->getPitchMidi();
        } else {
            return 0.f;
        }
    }

    JNIEXPORT jint JNICALL Java_com_yourdomain_yourapp_SurfaceViewDSP_getMidiNoteNumber(JNIEnv *env, jobject thiz) {
        if( dspProcessor != NULL ) {
            return dspProcessor->getMidiNoteNumber();
        } else {
            return 0.f;
        }
    }
} // extern "C"

typedef struct {
    int fd;
    ANativeWindow* window;
    AMediaExtractor* ex;
    AMediaCodec *codec;
    int64_t renderstart;
    bool sawInputEOS;
    bool sawOutputEOS;
    bool isPlaying;
    bool renderonce;
} workerdata;

workerdata data = {-1, NULL, NULL, NULL, 0, false, false, false, false};

extern "C" JNIEXPORT jboolean JNICALL
Java_com_yourdomain_yourapp_MainActivity_openMediaFile(
    JNIEnv *env,
    jobject thiz,
    jstring filename
)
{
    jboolean retVal = JNI_FALSE;
    const char* utf8 = env->GetStringUTFChars( filename, NULL );
    if( media.openFile( utf8 ) ) {
        retVal = JNI_TRUE;
    } else {
        retVal = JNI_FALSE;
    }
    env->ReleaseStringUTFChars(filename, utf8);
    return JNI_TRUE;

//    LOGV("@@@ create");
//
//    // convert Java string to UTF-8
//    const char *utf8 = env->GetStringUTFChars(filename, NULL);
//    LOGV("opening %s", utf8);
//
//    off_t outStart, outLen;
////    int fd = AAsset_openFileDescriptor(AAssetManager_open(AAssetManager_fromJava(env, assetMgr), utf8, 0),
////                                       &outStart, &outLen);
//
//    struct stat st;
//    int statRet = stat( utf8, &st );
//    int fd = open( utf8, O_RDONLY );
//    if( statRet != 0 || fd == -1 )
//        return JNI_FALSE;
//
//    env->ReleaseStringUTFChars(filename, utf8);
//    if (fd < 0) {
//        LOGE("failed to open file: %s %d (%s)", utf8, fd, strerror(errno));
//        return JNI_FALSE;
//    }
//
//    outStart = 0;
//    outLen = (off_t)st.st_size;
//
//    data.fd = fd;
//
//    workerdata *d = &data;
//
//    AMediaExtractor *ex = AMediaExtractor_new();
//    media_status_t err = AMediaExtractor_setDataSourceFd(ex, d->fd,
//                                                         static_cast<off64_t>(outStart),
//                                                         static_cast<off64_t>(outLen));
//    close(d->fd);
//    if (err != AMEDIA_OK) {
//        LOGV("setDataSource error: %d", err);
//        return JNI_FALSE;
//    }
//
//    int numtracks = AMediaExtractor_getTrackCount(ex);
//
//    AMediaCodec *codec = NULL;
//
//    LOGV("input has %d tracks", numtracks);
//    for (int i = 0; i < numtracks; i++) {
//        AMediaFormat *format = AMediaExtractor_getTrackFormat(ex, i);
//        const char *s = AMediaFormat_toString(format);
//        LOGV("track %d format: %s", i, s);
//        const char *mime;
//        if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
//            LOGV("no mime type");
//            return JNI_FALSE;
//        } else {
//            // Omitting most error handling for clarity.
//            // Production code should check for errors.
//            AMediaExtractor_selectTrack(ex, i);
//            codec = AMediaCodec_createDecoderByType(mime);
//            AMediaCodec_configure(codec, format, d->window, NULL, 0);
//            d->ex = ex;
//            d->codec = codec;
//            d->renderstart = -1;
//            d->sawInputEOS = false;
//            d->sawOutputEOS = false;
//            d->isPlaying = false;
//            d->renderonce = true;
//            AMediaCodec_start(codec);
//        }
//        AMediaFormat_delete(format);
//    }
//
////    mlooper = new mylooper();
////    mlooper->post(kMsgCodecBuffer, d);
//
//    return JNI_TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SCRATCH TEST CODE
////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C"
JNIEXPORT void JNICALL
Java_com_yourdomain_yourapp_MainActivity_startup(JNIEnv *env, jobject thiz) {
//    AutocorrelationNormalized* ac;
//    ac = new AutocorrelationNormalized( 32 );
//    ac->process( &testIn[0], 32, &testOut[0] );
//    delete ac;

    // meta-FFT "stress" tests
//    FFTS<float> F;
//    float fa[ 8192 ];
//    int64_t nsStart = 0LL, nsEnd = 0LL;
//    for( int n=0; n < 30; ++n ) {
//        for (int N = 4; N <= 2048; N *= 2) {
//            nsStart = cnanos();
//            F.fft(&fa[0], N);
//            nsEnd = cnanos();
//            int ns = (int) (nsEnd - nsStart);
//            LOGI("FFT %d in %d ns", N, ns);
//        }
//    }

//    test_real();
}