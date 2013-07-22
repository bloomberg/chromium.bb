// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Bitmap;
import android.os.Looper;
import android.text.InputType;
import android.util.Log;
import android.widget.EditText;
import android.widget.Toast;

import org.chromium.chromoting.R;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Initializes the Chromium remoting library, and provides JNI calls into it.
 * All interaction with the native code is centralized in this class.
 */
public class JniInterface {
    /** The status code indicating successful connection. */
    private static final int SUCCESSFUL_CONNECTION = 3;

    /** The application context. */
    private static Activity sContext = null;

    /*
     * Library-loading state machine.
     */
    /** Whether we've already loaded the library. */
    private static boolean sLoaded = false;

    /** To be called once from the main Activity. */
    public static void loadLibrary(Activity context) {
        synchronized(JniInterface.class) {
            if (sLoaded) return;
        }

        System.loadLibrary("remoting_client_jni");
        loadNative(context);
        sContext = context;
        sLoaded = true;
    }

    /** Performs the native portion of the initialization. */
    private static native void loadNative(Context context);

    /*
     * API/OAuth2 keys access.
     */
    public static native String getApiKey();
    public static native String getClientId();
    public static native String getClientSecret();

    /*
     * Connection-initiating state machine.
     */
    /** Whether the native code is attempting a connection. */
    private static boolean sConnected = false;

    /** The callback to signal upon successful connection. */
    private static Runnable sSuccessCallback = null;

    /** Attempts to form a connection to the user-selected host. */
    public static void connectToHost(String username, String authToken,
            String hostJid, String hostId, String hostPubkey, Runnable successCallback) {
        synchronized(JniInterface.class) {
            if (!sLoaded) return;
            if (sConnected) {
                disconnectFromHost();
            }
        }

        sSuccessCallback = successCallback;
        connectNative(username, authToken, hostJid, hostId, hostPubkey);
        sConnected = true;
    }

    /** Severs the connection and cleans up. */
    public static void disconnectFromHost() {
        synchronized(JniInterface.class) {
            if (!sLoaded || !sConnected) return;
        }

        disconnectNative();
        sSuccessCallback = null;
        sConnected = false;
    }

    /** Performs the native portion of the connection. */
    private static native void connectNative(
            String username, String authToken, String hostJid, String hostId, String hostPubkey);

    /** Performs the native portion of the cleanup. */
    private static native void disconnectNative();

    /*
     * Entry points *from* the native code.
     */
    /** The callback to signal whenever we need to redraw. */
    private static Runnable sRedrawCallback = null;

    /** Screen width of the video feed. */
    private static int sWidth = 0;

    /** Screen height of the video feed. */
    private static int sHeight = 0;

    /** Buffer holding the video feed. */
    private static ByteBuffer sBuffer = null;

    /** Reports whenever the connection status changes. */
    private static void reportConnectionStatus(int state, int error) {
        if (state == SUCCESSFUL_CONNECTION) {
            sSuccessCallback.run();
        }

        Toast.makeText(sContext, sContext.getResources().getStringArray(
                R.array.protoc_states)[state] + (error != 0 ? ": " +
                        sContext.getResources().getStringArray(R.array.protoc_errors)[error] : ""),
                Toast.LENGTH_SHORT).show();
    }

    /** Prompts the user to enter a PIN. */
    private static void displayAuthenticationPrompt() {
        AlertDialog.Builder pinPrompt = new AlertDialog.Builder(sContext);
        pinPrompt.setTitle(sContext.getString(R.string.pin_entry_title));
        pinPrompt.setMessage(sContext.getString(R.string.pin_entry_message));

        final EditText pinEntry = new EditText(sContext);
        pinEntry.setInputType(
                InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        pinPrompt.setView(pinEntry);

        pinPrompt.setPositiveButton(
                R.string.pin_entry_connect, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    Log.i("jniiface", "User provided a PIN code");
                    authenticationResponse(String.valueOf(pinEntry.getText()));
                }
            });

        pinPrompt.setNegativeButton(
                R.string.pin_entry_cancel, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    Log.i("jniiface", "User canceled pin entry prompt");
                    Toast.makeText(sContext,
                            sContext.getString(R.string.msg_pin_canceled),
                            Toast.LENGTH_LONG).show();
                    disconnectFromHost();
                }
            });

        pinPrompt.show();
    }

    /** Sets the redraw callback to the provided functor. */
    public static void provideRedrawCallback(Runnable redrawCallback) {
        sRedrawCallback = redrawCallback;
    }

    /** Forces the native graphics thread to redraw to the canvas. */
    public static boolean redrawGraphics() {
        synchronized(JniInterface.class) {
            if (!sConnected) return false;
        }

        scheduleRedrawNative();
        return true;
    }

    /** Performs the redrawing callback. */
    private static void redrawGraphicsInternal() {
        sRedrawCallback.run();
    }

    /**
     * Obtains the image buffer.
     * This should not be called from the UI thread. (We prefer the native graphics thread.)
     */
    public static Bitmap retrieveVideoFrame() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w("deskview", "Canvas being redrawn on UI thread");
        }

        if (!sConnected) {
            return null;
        }

        int[] frame = new int[sWidth * sHeight];

        sBuffer.order(ByteOrder.LITTLE_ENDIAN);
        sBuffer.asIntBuffer().get(frame, 0, frame.length);

        return Bitmap.createBitmap(frame, 0, sWidth, sWidth, sHeight, Bitmap.Config.ARGB_8888);
    }

    /** Performs the native response to the user's PIN. */
    private static native void authenticationResponse(String pin);

    /** Schedules a redraw on the native graphics thread. */
    private static native void scheduleRedrawNative();
}
