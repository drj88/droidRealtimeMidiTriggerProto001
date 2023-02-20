package com.yourdomain.yourapp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceView;
import android.view.SurfaceHolder;

public class SurfaceViewDSP extends SurfaceView implements Runnable {
    static final String TAG = SurfaceViewDSP.class.getName();

    SurfaceHolder surfaceHolder;
    int pxViewWidth = 0;
    int pxViewHeight = 0;

    boolean running = false;
    Thread thread = null;

    Context context;

    Paint paintBlue;
    Paint paintGreen;
    Paint paintBg;
    Paint paintTxtWhite;
    Paint paintWhite;

    float framesPerSecond;

    public SurfaceViewDSP(Context context) {
        this(context, null);
    }

    public SurfaceViewDSP(Context context, AttributeSet attrs) {
        super(context, attrs);

        this.context = context;
        this.surfaceHolder = getHolder();

        this.paintWhite = new Paint();
        this.paintWhite.setColor(Color.WHITE);

        this.paintTxtWhite = new Paint();
        this.paintTxtWhite.setColor(Color.WHITE);
        this.paintTxtWhite.setTextSize(36.f);

        this.paintGreen = new Paint();
        this.paintGreen.setColor(Color.GREEN);

        this.paintBg = new Paint();
        this.paintBg.setColor(Color.BLACK);

        this.paintBlue = new Paint();
        this.paintBlue.setColor(Color.BLUE);
    }

    /**
     * We cannot get the correct dimensions of views in onCreate because
     * they have not been inflated yet. This method is called every time the
     * size of a view changes, including the first time after it has been
     * inflated.
     *
     * @param w Current width of view.
     * @param h Current height of view.
     * @param oldw Previous width of view.
     * @param oldh Previous height of view.
     */
    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        this.pxViewWidth = w;
        this.pxViewHeight = h;
    }

    //static boolean flip = true;
    public void run() {
        // See https://developer.android.com/reference/android/view/
        //    SurfaceHolder.html#lockHardwareCanvas()
        if( this.pxViewWidth == 0 ) { this.pxViewWidth = this.getWidth(); }
        if( this.pxViewHeight == 0 ) { this.pxViewHeight = this.getHeight(); }

        initAudioBridge();

        this.pitchHistoryIndex = -1;
        this.pitchMidiHistoryIndex = -1;
        this.midiNoteNumHistoryIndex = -1;

        long msTargetRate = 16;
        this.framesPerSecond = 0;
        while (this.running) {
            long start = System.currentTimeMillis();

            // If we can obtain a valid drawing surface...
            if (this.surfaceHolder.getSurface().isValid()) {
                Canvas canvas = this.surfaceHolder.lockCanvas();
                try {
                    switch( getProcessingMode() )
                    {
                        default:
                        case enums.ProcessingMode_RawAudioSignal:
                            drawRawAudioLines( canvas );
                            break;

                        case enums.ProcessingMode_MagnitudeSpectrum:
                            drawMagSpecLines( canvas );
                            break;

                        case enums.ProcessingMode_AutoCorrelation:
//                        case enums.ProcessingMode_PitchEstimation:
                            drawAutocorr( canvas );
                            break;

                        case enums.ProcessingMode_PitchEstimation:
                            drawPitch( canvas );
                            break;
                    }
                } catch( Exception e ) {
                    Log.e(TAG,Log.getStackTraceString(e));
                } finally {
                    this.surfaceHolder.unlockCanvasAndPost( canvas );
                }
            }

                long end = System.currentTimeMillis();
                long ts = end - start;
                if( ts < msTargetRate ) {
                    try {
                        Thread.sleep(msTargetRate - ts );
                    } catch (InterruptedException e) {
                        Log.e(TAG,Log.getStackTraceString(e));
                    }
                }

            // compute framerate
            end = System.currentTimeMillis();
            this.framesPerSecond = 1.f / ((float)(end - start) / 1000.f);
        }

        freeAudioBridge();
    }

    public float getFramesPerSecond() {
        return framesPerSecond;
    }

    public void stop() {
        this.running = false;
        try {
            // Stop the thread == rejoin the main thread.
            this.thread.join();
        } catch (InterruptedException e) {
        }
    }

    public void start(){
        if(!this.running) {
            this.thread = new Thread(this);
            this.thread.setPriority(Thread.NORM_PRIORITY);
            this.running = true;
            this.thread.start();
        }
    }

    int pitchHistoryIndex = -1;
    int pitchMidiHistoryIndex = -1;
    int midiNoteNumHistoryIndex = -1;
    float maxPitchHistory = 1500;
    float[] pitchHistory = new float[75];
    float[] pitchMidiHistory = new float[75];
    int[] midiNoteNumHistory = new int[75];
    void drawPitch(Canvas canvas) {
        float[] data = getDspOutput();
        for( int n=0; n < data.length; ++n ) {
            float absSample = Math.abs(data[n]);
            if( absSample > maxDataAutocorr ) {
                maxDataAutocorr = absSample;
            }
        }

        float pitch = getPitch();
        if( ++pitchHistoryIndex >= pitchHistory.length ) {
            for( int n=1; n < pitchHistory.length; ++n ) {
                pitchHistory[n-1] = pitchHistory[n];
            }
            pitchHistoryIndex = pitchHistory.length - 2;
        }
        pitchHistory[pitchHistoryIndex] = pitch;
//        for( int n=0; n < this.pitchHistory.length; ++n ) {
//            float absPitch = Math.abs(this.pitchHistory[n]);
//            if( absPitch > maxPitchHistory ) {
//                maxPitchHistory = absPitch;
//            }
//        }

        float pitchMidi = getPitchMidi();
        if( ++pitchMidiHistoryIndex >= pitchMidiHistory.length ) {
            for( int n=1; n < pitchMidiHistory.length; ++n ) {
                pitchMidiHistory[n-1] = pitchMidiHistory[n];
            }
            pitchMidiHistoryIndex = pitchMidiHistory.length - 2;
        }
        pitchMidiHistory[pitchMidiHistoryIndex] = pitchMidi;

        int midiNoteNum = getMidiNoteNumber();
        if( ++midiNoteNumHistoryIndex >= midiNoteNumHistory.length ) {
            for( int n=1; n < midiNoteNumHistory.length; ++n ) {
                midiNoteNumHistory[n-1] = midiNoteNumHistory[n];
            }
            midiNoteNumHistoryIndex = midiNoteNumHistory.length - 2;
        }
        midiNoteNumHistory[midiNoteNumHistoryIndex] = midiNoteNum;

        canvas.drawRect(0, 0, this.pxViewWidth, this.pxViewHeight, paintBg);

        int L = data.length/2;
        float pyHeight = (float) this.pxViewHeight/2;
        float pxStride = (float) this.pxViewWidth / (float)L;
        for( int xd=0, pxLast=0, pyLast=0; xd < L; ++xd ){
            int px = (int)(pxStride * (float)xd);
//            int py = this.pxViewHeight - (int)((data[xd] / maxDataAutocorr) * pyHeight);
            int py = (int)((data[xd] / maxDataAutocorr) * pyHeight + pyHeight);
            canvas.drawLine(pxLast, pyLast, px, py, paintBlue);
            pxLast = px;
            pyLast = py;
        }

        canvas.drawText( String.valueOf(pitch), this.pxViewWidth/2, this.pxViewHeight/8, paintTxtWhite);
        canvas.drawText( String.valueOf(pitchMidi) + " (" + String.valueOf(midiNoteNum) + ")", this.pxViewWidth/2, this.pxViewHeight/4, paintTxtWhite);

        L = pitchHistory.length;
        pyHeight = (float) this.pxViewHeight;
        pxStride = (float) this.pxViewWidth / (float)L;
        for( int xd=0, pxLast=0, pyLast=0; xd < L; ++xd ) {
            int px = (int)(pxStride * (float)xd);
            int py = this.pxViewHeight - (int)((this.pitchHistory[xd] / maxPitchHistory) * pyHeight);
            canvas.drawLine(pxLast, pyLast, px, py, paintGreen);
            pxLast = px;
            pyLast = py;
        }

        L = pitchMidiHistory.length;
        pyHeight = (float) this.pxViewHeight;
        pxStride = (float) this.pxViewWidth / (float)L;
        for( int xd=0, pxLast=0, pyLast=0; xd < L; ++xd ) {
            int px = (int)(pxStride * (float)xd);
            int py = this.pxViewHeight - (int)((this.pitchMidiHistory[xd] / maxPitchHistory) * pyHeight);
            canvas.drawLine(pxLast, pyLast, px, py, paintWhite);
            pxLast = px;
            pyLast = py;
        }
    }

    float maxDataAutocorr = 0;
    void drawAutocorr(Canvas canvas) {
        int L;
        float pxStride, pyHeight;

        float[] data = getDspOutput();

        for( int n=0; n < data.length; ++n ) {
            float absSample = Math.abs(data[n]);
            if( absSample > maxDataAutocorr ) {
                maxDataAutocorr = absSample;
            }
        }

        canvas.drawRect(0, 0, this.pxViewWidth, this.pxViewHeight, paintBg);

        L = data.length/2;
        float vertOffset = ((float)this.pxViewHeight * .3f);
        pyHeight = (float) this.pxViewHeight - vertOffset/2;
//        pyHeight = (float) this.pxViewHeight;
        pxStride = (float) this.pxViewWidth / (float)L;
        for( int xd=0, pxLast=0, pyLast=0; xd < L; ++xd ){
            int px = (int)(pxStride * (float)xd);
            int py = this.pxViewHeight - (int)((data[xd] / maxDataAutocorr) * pyHeight + (vertOffset / 2f));
//            int py = this.pxViewHeight - (int)((data[xd] / maxDataAutocorr) * pyHeight);
            canvas.drawLine(pxLast, pyLast, px, py, paintGreen);
            pxLast = px;
            pyLast = py;
        }
    }

    void drawMagSpecLines(Canvas canvas) {
        int L;
        float maxData, pxStride, pyHeight;

        float[] data = getDspOutput();

        maxData = 0;
        for( int n=0; n < data.length/2; ++n ) {
            float absSample = Math.abs(data[n]);
            if( absSample > maxData ) {
                maxData = absSample;
            }
        }

        canvas.drawRect(0, 0, this.pxViewWidth, this.pxViewHeight, paintBg);

        L = data.length/16;
        pyHeight = (float) this.pxViewHeight;
        pxStride = (float) this.pxViewWidth / (float)L;
        for( int xd=2, pxLast=0, pyLast=0; xd < L; ++xd ){
            int px = (int)(pxStride * (float)xd);
            int py = this.pxViewHeight - (int)((data[xd] / maxData) * pyHeight);
            canvas.drawLine(pxLast, pyLast, px, py, paintGreen);
            pxLast = px;
            pyLast = py;
        }
    }

    void drawRawAudioLines(Canvas canvas) {
        int L;
        float maxData, pxStride, pyHeight;

        float[] data = getDspOutput();

        maxData = 0;
        for( int n=0; n < data.length; ++n ) {
            float absSample = Math.abs(data[n]);
            if( absSample > maxData ) {
                maxData = absSample;
            }
        }

        canvas.drawRect(0, 0, this.pxViewWidth, this.pxViewHeight, paintBg);

        L = data.length;
        pyHeight = (float) this.pxViewHeight / 2;
        pxStride = (float) this.pxViewWidth / (float)L;
        for( int xd=0, pxLast=0, pyLast=0; xd < L; ++xd ){
            int px = (int)(pxStride * (float)xd);
            int py = this.pxViewHeight / 2 - (int)((data[xd] / maxData) * pyHeight);
            canvas.drawLine(pxLast, pyLast, px, py, paintGreen);
            pxLast = px;
            pyLast = py;
        }
    }

    static { System.loadLibrary("native-lib"); }
    native float[] getDspOutput();
    native void initAudioBridge();
    native void freeAudioBridge();
    native int getProcessingMode();
    native float getPitch();
    native float getPitchMidi();
    native int getMidiNoteNumber();
}
