package com.yourdomain.yourapp;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiReceiver;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.yourdomain.yourapp.databinding.ActivityMainBinding;

import com.example.simplefileexplorer.SimpleFileExplorerActivity;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity implements Runnable {
    static final String TAG = MainActivity.class.getName();

    static final int AUDIO_EFFECT_REQUEST = 0;

    private final String permission = Manifest.permission.WRITE_EXTERNAL_STORAGE;
    private final int file_explorer_request_result_code = 22;
    private final int permissions_request_code = 23;

    ActivityMainBinding binding;

    // Activity Controls (found in activity_main.xml)
    SurfaceViewDSP      surfaceViewDSP;
    Spinner             spinnerProcessingMode;
    Button              buttonStartStopRecording;
    TextView            textViewBottomInfo;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        this.surfaceViewDSP = this.findViewById(R.id.surfaceViewDSP);
        this.spinnerProcessingMode = this.findViewById(R.id.spinnerProcessingMode);
        this.buttonStartStopRecording = this.findViewById(R.id.buttonStartStopRecording);
        this.textViewBottomInfo = this.findViewById(R.id.textViewBottomInfo);

        List<String> listProcessingModes = new ArrayList<>();
        listProcessingModes.add(0, getString(R.string.processing_mode_0_raw_audio));
        listProcessingModes.add(1, getString(R.string.processing_mode_1_magnitude_spectrum));
        listProcessingModes.add(2, getString(R.string.processing_mode_2_autocorrelation));
        listProcessingModes.add(3, getString(R.string.processing_mode_2_pitch_estimate));
        ArrayAdapter<String> aaProcessingModes = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, listProcessingModes );
        aaProcessingModes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item); // The drop down view
        spinnerProcessingMode.setAdapter(aaProcessingModes);

        _spinInput = findViewById(R.id.spinnerAudioInput);
        _spinOutput = findViewById(R.id.spinnerMidiOutputDevice);
        _spinOutChannel = findViewById(R.id.spinnerMidiOutputChannel);
        _spinOutPatch = findViewById(R.id.spinnerMidiOutputPatch);

        setupMidiDevicesList();

        startup();
    }

    private void startFileExplorerLibrary(){
        Intent intent = new Intent(this, SimpleFileExplorerActivity.class);
        startActivityForResult(intent, file_explorer_request_result_code);
    }

    public void onClickStartStopButton(View view) {
        startStopRecording();
    }

    public void onClickOpenFile(View view) {
        if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{this.permission}, this.permissions_request_code);
        }
        else {
            this.startFileExplorerLibrary();
        }
    }

    /* recording permission / toast */
    boolean isRecordPermissionGranted() {
        return ( ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED );
    }

    void requestRecordPermission(){
        ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, AUDIO_EFFECT_REQUEST );
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode,
            @NonNull String[] permissions,
            @NonNull int[] grantResults
    )
    {
        if (AUDIO_EFFECT_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        } else if (AUDIO_EFFECT_REQUEST == requestCode) {
            if (grantResults.length != 1 || grantResults[0] != PackageManager.PERMISSION_GRANTED) {
                // User denied the permission, without this we cannot record audio
                // Show a toast and update the status accordingly
                buttonStartStopRecording.setText(getString(R.string.button_main_act_record_need_permissions));
                ///_tvNfo.setText(R.string.audio_need_rec_permission_denied);

                Toast.makeText(
                        getApplicationContext(),
                        getString(R.string.toast_record_permissions),
                        Toast.LENGTH_SHORT
                ).show();
            } else {
                startStopRecording();
            }
        } else if(requestCode == this.permissions_request_code) {
            if(grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED){
                this.startFileExplorerLibrary();
            }
            else {
                Toast.makeText(this, "Permission not granted.", Toast.LENGTH_LONG).show();
            }
        }
    }

    @Override protected void onActivityResult( int requestCode, int resultCode, @Nullable Intent data ) {
        super.onActivityResult( requestCode, resultCode, data );

        if(data != null) {
            String selectedAbsolutePath = data.getStringExtra(SimpleFileExplorerActivity.ON_ACTIVITY_RESULT_KEY);

            //Toast.makeText(this, selectedAbsolutePath, Toast.LENGTH_SHORT).show();

            this.openMediaFile( selectedAbsolutePath );
        }
    }

    void startStopRecording()
    {
        if( this.isRecording() ) {
            //this.stop();
            surfaceViewDSP.stop();
            this.stopRecording();
            buttonStartStopRecording.setText( R.string.button_main_act_start_recording );
        } else {
            if(isRecordPermissionGranted()) {
                int processingModeSelectionIndex = this.spinnerProcessingMode.getSelectedItemPosition();
                this.setProcessingMode( processingModeSelectionIndex );

                // midi device selected by user ?
                if( processingModeSelectionIndex == 3 && getSelectedMidiDeviceInfoIndex() > -1 ) {
                    _runMidi = true;
                    _awaitForMidiDeviceOpened = true;
                    _th = new Thread(new MidiWriter(this, 1));
                    _th.start();
                }

                this.startRecording();
                surfaceViewDSP.start();
                //this.start();
                buttonStartStopRecording.setText( R.string.button_main_act_stop_recording );
            }
            else {
                requestRecordPermission();
            }
        }
    }

    static { System.loadLibrary("native-lib"); }
    native int startRecording();
    native void stopRecording();
    native boolean isRecording();
    native void setProcessingMode( int mode );
    native boolean openMediaFile(String filename);
    native void startup();
    public native float getPitchEstimate();
    public native int getMidiNoteNumber();

    boolean running = false;
    Thread thread = null;
    public void stop() {
        this.running = false;

        try {
            if( this._runMidi ) {
                this._runMidi = false;
                _th.join();
            }

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
    @Override
    public void run() {
//        this.textViewBottomInfo.setText(String.valueOf(this.surfaceViewDSP.getFramesPerSecond()));
        long msTargetRate = 16;
        while( this.running ) {
            long start = System.currentTimeMillis();
            this.textViewBottomInfo.setText(String.valueOf(this.getPitchEstimate()));
            long end = System.currentTimeMillis();
            long ts = end - start;
            if( ts < msTargetRate ) {
                try {
                    Thread.sleep(msTargetRate - ts );
                } catch (InterruptedException e) {
                    Log.e(TAG,Log.getStackTraceString(e));
                }
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // MIDI
    ////////////////////////////////////////////////////////////////////////////////////////////////
    static boolean              _awaitForMidiDeviceOpened = false;
    static Thread               _th = null;

    ///TextView _tvNfo = null;
    ///TextView _tvDbg = null;

    Spinner _spinInput = null;
    Spinner _spinOutput = null;
    Spinner _spinOutChannel = null;
    Spinner _spinOutPatch = null;

    ///LinearLayout _canvasLayout = null;
    ///CustomSurfaceView _surfaceView = null;

    /* midi devices list */
    @RequiresApi(api = Build.VERSION_CODES.M)
    void setupMidiDevicesList()
    {
        // check for midi support...
        ///_tvNfo.setText(R.string.tvNfo_Status_TestForMidi);
        if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) {
            // MIDI capability exists, proceed...
            ///_tvNfo.setText(R.string.tvNfo_Status_HasMidiOuts);

            _mm = (MidiManager)getSystemService(Context.MIDI_SERVICE);
            _mdi = _mm.getDevices();

            List<String> midiOutDevicesList = new ArrayList<>();
            List<String> midiOutChannelsList = new ArrayList<>();
            List<String> midiOutPatchList = new ArrayList<>();
            if( _mdi != null && _mdi.length > 0 ){
                for( int mdi=0, n=1; mdi < _mdi.length; ++mdi ) {
                    int numInputs = _mdi[ mdi ].getInputPortCount();
                    if( numInputs > 0 ) {
                        Bundle properties = _mdi[ mdi ].getProperties();

                        // todo check that there is at least one, actual input port
                        //MidiDeviceInfo.PortInfo[] portInfos = _mdi[ mdi ].getPorts();
                        //String portName = portInfos[0].getName();
                        //if (portInfos[0].getType() == MidiDeviceInfo.PortInfo.TYPE_INPUT) {

                        String name = properties.getString(MidiDeviceInfo.PROPERTY_NAME);
                        String manu = properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER);
                        String prod = properties.getString(MidiDeviceInfo.PROPERTY_PRODUCT);

                        if( name == null ) name = getString(R.string.spinOutputNoName);
                        if( manu == null ) manu = getString(R.string.spinOutputNoManu);
                        if( prod == null ) prod = getString(R.string.spinOutputNoProd);
                        String devStr = n + ": " + name + " " + prod + " " + manu;
                        midiOutDevicesList.add(devStr);
                        ++n;
                    }
                }

                if( midiOutDevicesList.isEmpty() ){
                    midiOutDevicesList.add(getString(R.string.spinOutputNothingAvail));
                }
                else {
                    midiOutDevicesList.add(0, getString(R.string.spinOutputSelectNoMidiOut));

                    ArrayAdapter<String> saaMidiOut = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, midiOutDevicesList);
                    saaMidiOut.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item); // The drop down view
                    _spinOutput.setAdapter(saaMidiOut);

                    for( int ch=1; ch<=MidiConstants.MAX_CHANNELS; ++ch )
                        midiOutChannelsList.add( Integer.toString(ch) );

                    ArrayAdapter<String> saaMidiChan = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, midiOutChannelsList);
                    saaMidiChan.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item); // The drop down view
                    _spinOutChannel.setAdapter(saaMidiChan);

                    for( int pc=1; pc<=MidiConstants.MAX_PATCH; ++pc )
                        midiOutPatchList.add( Integer.toString(pc) );

                    ArrayAdapter<String> saaMidiPatch = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, midiOutPatchList);
                    saaMidiPatch.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item); // The drop down view
                    _spinOutPatch.setAdapter(saaMidiPatch);
                }
            }
            else
            {
                midiOutDevicesList.add(getString(R.string.spinOutputNothingAvail));
            }

            // MIDI ready!
            ///_tvNfo.setText(R.string.tvNfo_Status_MidiReady);
        }
    }

    /* special debug routines */
    ///public void dgb(String s) { _tvDbg.setText(s); }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // MIDI
    ////////////////////////////////////////////////////////////////////////////////////////////////


    /* MIDI */
    boolean _runMidi = false;

    MidiManager _mm = null;
    MidiDeviceInfo[] _mdi = null;
    MidiInputPort _mip = null;
    MidiDevice _md = null;

    public int getPatch() {
        return _spinOutPatch.getSelectedItemPosition();
    }

    public int getChannel() {
        return _spinOutChannel.getSelectedItemPosition();
    }

    public void setMidiDeviceOpened() {
        _awaitForMidiDeviceOpened = false;
    }

    public boolean awaitingMidiDeviceOpen() {
        return _awaitForMidiDeviceOpened;
    }

    public boolean runMidi() {
        return _runMidi;
    }

    public MidiManager getMidiManager() {
        return _mm;
    }

    public MidiDeviceInfo getSelectedMidiDeviceInfo() {
        return _mdi[ getSelectedMidiDeviceInfoIndex() ];
    }

    public MidiInputPort getMidiInputPort() {
        return _mip;
    }

    public MidiReceiver getMidiReceiver() {
        return _mip;
    }

    public int getSelectedMidiDeviceInfoIndex() {
        return _spinOutput.getSelectedItemPosition() - 1;
    }

    public void setMidiDevice( MidiDevice md ) {
        _md = md;
    }

    public void setMidiInputPort( MidiInputPort port ) {
        _mip = port;
    }
}