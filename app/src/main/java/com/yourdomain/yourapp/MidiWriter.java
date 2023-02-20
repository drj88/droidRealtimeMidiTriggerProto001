package com.yourdomain.yourapp;

import android.os.Build;
import android.os.Process;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiReceiver;
import android.util.Log;
import java.io.IOException;

import static android.os.Process.THREAD_PRIORITY_AUDIO;

import androidx.annotation.RequiresApi;
//import static android.os.Process.THREAD_PRIORITY_URGENT_AUDIO;

class MidiWriter implements Runnable
{
    static final String TAG = MidiWriter.class.getName();

    MainActivity    _act;

    long            _ms;
    byte[]          _bytes = new byte[3];

    MidiWriter(MainActivity act, long msThrottle, double thres)
    {
        _act = act;
        _ms = msThrottle;
    }

    MidiWriter(MainActivity act, long msThrottle)
    {
        _act = act;
        _ms = msThrottle;
    }

    boolean _running = false;
    @RequiresApi(api = Build.VERSION_CODES.M)
    public void run()
    {
        _running = true;

        MidiDeviceInfo mdi = _act.getSelectedMidiDeviceInfo();
        _act.getMidiManager().openDevice(
                mdi,
                new MidiManager.OnDeviceOpenedListener() {
                    @Override
                    public void onDeviceOpened(MidiDevice device) {
                        if (device == null) {
                            Log.e(MidiConstants.TAG, "could not open Midi Device");
                        } else {
                            int channel = _act.getChannel();

                            _act.setMidiDevice( device );
                            _act.setMidiInputPort(device.openInputPort(channel));
                            if (_act.getMidiInputPort() == null) {
                                Log.e(MidiConstants.TAG, "could not open input port on Midi Device");
                            }
                            else {
                                changeProgram(_act.getPatch(), channel);
                                _act.setMidiDeviceOpened();
                            }
                        }
                    }
                },
                null
        );
        // Don't run the callback on the UI thread because openInputPort might take a while.

        // sleep until the midi device actually opens...
        while( _act.awaitingMidiDeviceOpen() ) {
            try {
                Thread.sleep( _ms );
            } catch (InterruptedException e) {
                //e.printStackTrace(); // todo
            }
        }

        // prepare
        boolean playing = false;
        int lastMidiNoteVelocity = 0, lastMidiNote = 0;
        int patch = _act.getPatch();
        int channel = _act.getChannel();
        long accum, acc_st = System.currentTimeMillis();

        // poll each ms for changes to pitch estimate & midi note
        Process.setThreadPriority(THREAD_PRIORITY_AUDIO);

        // play!
        while( _act.runMidi() )
        {
            long start = System.currentTimeMillis();
            if (_act.getPitchEstimate() > 0) {
                int
                    vel = 100,
                    note = _act.getMidiNoteNumber();
                if( playing ) {
                    if( lastMidiNote != note) {
                        noteOff(channel, lastMidiNote, lastMidiNoteVelocity);
                        noteOn(channel, note, vel);

                        lastMidiNoteVelocity = vel;
                        lastMidiNote = note;
                    }
                    else {
                        // todo adjust volume || detect vibrato - pitch wheel, etc
                    }
                }
                else {
                    playing = true;
                    noteOn(channel, note, vel);

                    lastMidiNoteVelocity = vel;
                    lastMidiNote = note;
                }
            } else {
                if (playing) {
                    playing = false;
                    noteOff(channel, lastMidiNote, lastMidiNoteVelocity);
                }
            }

            try {
                Thread.sleep( _ms );
            } catch (InterruptedException e) {
                //e.printStackTrace(); // todo
            }
        } // while( _act.runMidi() ) {

        // in lieu of this...
        noteOff(channel, lastMidiNote, lastMidiNoteVelocity );

        // ensure ALL possble notes are off (todo b8r sol)...
        for( int nMidiNoteNumber=MidiConstants.MIDI_NOTE_NUMBER_MIN; nMidiNoteNumber<=MidiConstants.MIDI_NOTE_NUMBER_MAX; ++nMidiNoteNumber ) {
            noteOff(channel, nMidiNoteNumber, lastMidiNoteVelocity);
        }

        if( _act.getMidiInputPort() != null ) {
            MidiInputPort port = _act.getMidiInputPort();

            try {
                port.flush();
            } catch (IOException e) {
                //e.printStackTrace(); //todo
            }

            try {
                port.close();
            } catch (IOException e) {
                //e.printStackTrace();  //todo
            }
        }
    }

    /*
    public void onProgramSend(View view) {
        midiCommand(MidiConstants.STATUS_PROGRAM_CHANGE + _channel, _programs[_channel]);
    }

    public void onProgramDelta(View view) {
        Button button = (Button) view;
        //int delta = Integer.parseInt(button.getText().toString());
        int delta = 0;
        changeProgram(delta);
    }
    */

    void changeProgram(int patchNum, int channel) {
        /*int program = _programs[_channel];
        program += delta;
        if (program < 0) {
            program = 0;
        } else if (program > 127) {
            program = 127;
        }*/
        midiCommand(
            MidiConstants.STATUS_PROGRAM_CHANGE + channel,
                patchNum
        );
        //_programs[_channel] = program;
        //updateProgramText();
    }

    void noteOff(int channel, int pitch, int velocity) {
        midiCommand(MidiConstants.STATUS_NOTE_OFF + channel, pitch, velocity);
    }

    void noteOn(int channel, int pitch, int velocity) {
        midiCommand(MidiConstants.STATUS_NOTE_ON + channel, pitch, velocity);
    }

    void midiCommand(int status, int data1, int data2) {
        _bytes[0] = (byte) status;
        _bytes[1] = (byte) data1;
        _bytes[2] = (byte) data2;
        long now = System.nanoTime();
        midiSend(_bytes, 3, now);
    }

    void midiCommand(int status, int data1) {
        _bytes[0] = (byte) status;
        _bytes[1] = (byte) data1;
        long now = System.nanoTime();
        midiSend(_bytes, 2, now);
    }

    void midiSend(byte[] buffer, int count, long timestamp) {
        try {
            // send event immediately
            MidiReceiver receiver = _act.getMidiReceiver();
            if (receiver != null) {
                receiver.send(buffer, 0, count, timestamp);
            }
        } catch (IOException e) {
            Log.e(TAG, "_act.getMidiPortSelector().send() failed " + e);
        }
    }
}