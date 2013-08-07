// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.os.Looper;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.CheckBox;
import android.widget.TextView;
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

    /**
     * To be called once from the main Activity. Any subsequent calls will update the application
     * context, but not reload the library. This is useful e.g. when the activity is closed and the
     * user later wants to return to the application.
     */
    public static void loadLibrary(Activity context) {
        sContext = context;

        synchronized(JniInterface.class) {
            if (sLoaded) return;
        }

        System.loadLibrary("remoting_client_jni");
        loadNative(context);
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

    /** Callback to signal upon successful connection. */
    private static Runnable sSuccessCallback = null;

    /** Dialog for reporting connection progress. */
    private static ProgressDialog sProgressIndicator = null;

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
        SharedPreferences prefs = sContext.getPreferences(Activity.MODE_PRIVATE);
        connectNative(username, authToken, hostJid, hostId, hostPubkey,
                prefs.getString(hostId + "_id", ""), prefs.getString(hostId + "_secret", ""));
        sConnected = true;
    }

    /** Severs the connection and cleans up. */
    public static void disconnectFromHost() {
        synchronized(JniInterface.class) {
            if (!sLoaded || !sConnected) return;

            if (sProgressIndicator != null) {
                sProgressIndicator.dismiss();
                sProgressIndicator = null;
            }
        }

        disconnectNative();
        sSuccessCallback = null;
        sConnected = false;
    }

    /** Performs the native portion of the connection. */
    private static native void connectNative(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, String pairId, String pairSecret);

    /** Performs the native portion of the cleanup. */
    private static native void disconnectNative();

    /*
     * Entry points *from* the native code.
     */
    /** Callback to signal whenever we need to redraw. */
    private static Runnable sRedrawCallback = null;

    /** Screen width of the video feed. */
    private static int sWidth = 0;

    /** Screen height of the video feed. */
    private static int sHeight = 0;

    /** Buffer holding the video feed. */
    private static ByteBuffer sBuffer = null;

    /** Reports whenever the connection status changes. */
    private static void reportConnectionStatus(int state, int error) {
        if (state < SUCCESSFUL_CONNECTION && error == 0) {
            // The connection is still being established, so we'll report the current progress.
            synchronized (JniInterface.class) {
                if (sProgressIndicator == null) {
                    sProgressIndicator = ProgressDialog.show(sContext, sContext.
                            getString(R.string.progress_title), sContext.getResources().
                            getStringArray(R.array.protoc_states)[state], true, true,
                            new DialogInterface.OnCancelListener() {
                                @Override
                                public void onCancel(DialogInterface dialog) {
                                    Log.i("jniiface", "User canceled connection initiation");
                                    disconnectFromHost();
                                }
                            });
                }
                else {
                    sProgressIndicator.setMessage(
                            sContext.getResources().getStringArray(R.array.protoc_states)[state]);
                }
            }
        }
        else {
            // The connection is complete or has failed, so we can lose the progress indicator.
            synchronized (JniInterface.class) {
                if (sProgressIndicator != null) {
                    sProgressIndicator.dismiss();
                    sProgressIndicator = null;
                }
            }

            if (state == SUCCESSFUL_CONNECTION) {
                Toast.makeText(sContext, sContext.getResources().
                        getStringArray(R.array.protoc_states)[state], Toast.LENGTH_SHORT).show();

                // Actually display the remote desktop.
                sSuccessCallback.run();
            } else {
                Toast.makeText(sContext, sContext.getResources().getStringArray(
                        R.array.protoc_states)[state] + (error == 0 ? "" : ": " +
                        sContext.getResources().getStringArray(R.array.protoc_errors)[error]),
                        Toast.LENGTH_LONG).show();
            }
        }
    }

    /** Prompts the user to enter a PIN. */
    private static void displayAuthenticationPrompt() {
        AlertDialog.Builder pinPrompt = new AlertDialog.Builder(sContext);
        pinPrompt.setTitle(sContext.getString(R.string.pin_entry_title));
        pinPrompt.setMessage(sContext.getString(R.string.pin_entry_message));
        pinPrompt.setIcon(android.R.drawable.ic_lock_lock);

        final View pinEntry = sContext.getLayoutInflater().inflate(R.layout.pin_dialog, null);
        pinPrompt.setView(pinEntry);

        pinPrompt.setPositiveButton(
                R.string.pin_entry_connect, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Log.i("jniiface", "User provided a PIN code");
                        authenticationResponse(String.valueOf(
                                ((TextView)
                                        pinEntry.findViewById(R.id.pin_dialog_text)).getText()),
                                ((CheckBox)
                                        pinEntry.findViewById(R.id.pin_dialog_check)).isChecked());
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

        final AlertDialog pinDialog = pinPrompt.create();

        ((TextView)pinEntry.findViewById(R.id.pin_dialog_text)).setOnEditorActionListener(
                new TextView.OnEditorActionListener() {
                    @Override
                    public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                        // The user pressed enter on the keypad (equivalent to the connect button).
                        pinDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
                        pinDialog.dismiss();
                        return true;
                    }
                });

        pinDialog.setOnCancelListener(
                new DialogInterface.OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        // The user backed out of the dialog (equivalent to the cancel button).
                        pinDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
                    }
                });

        pinDialog.show();
    }

    /** Saves newly-received pairing credentials to permanent storage. */
    private static void commitPairingCredentials(String host, byte[] id, byte[] secret) {
        synchronized (sContext) {
            sContext.getPreferences(Activity.MODE_PRIVATE).edit().
                    putString(host + "_id", new String(id)).
                    putString(host + "_secret", new String(secret)).
                    apply();
        }
    }

    /**
     * Sets the redraw callback to the provided functor. Provide a value of null whenever the
     * window is no longer visible so that we don't continue to draw onto it.
     */
    public static void provideRedrawCallback(Runnable redrawCallback) {
        sRedrawCallback = redrawCallback;
    }

    /** Forces the native graphics thread to redraw to the canvas. */
    public static boolean redrawGraphics() {
        synchronized(JniInterface.class) {
            if (!sConnected || sRedrawCallback == null) return false;
        }

        scheduleRedrawNative();
        return true;
    }

    /** Performs the redrawing callback. This is a no-op if the window isn't visible. */
    private static void redrawGraphicsInternal() {
        if (sRedrawCallback != null)
            sRedrawCallback.run();
    }

    /**
     * Obtains the image buffer.
     * This should not be called from the UI thread. (We prefer the native graphics thread.)
     */
    public static Bitmap retrieveVideoFrame() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w("jniiface", "Canvas being redrawn on UI thread");
        }

        if (!sConnected) {
            return null;
        }

        int[] frame = new int[sWidth * sHeight];

        sBuffer.order(ByteOrder.LITTLE_ENDIAN);
        sBuffer.asIntBuffer().get(frame, 0, frame.length);

        return Bitmap.createBitmap(frame, 0, sWidth, sWidth, sHeight, Bitmap.Config.ARGB_8888);
    }

    /** Moves the mouse cursor, possibly while clicking the specified (nonnegative) button. */
    public static void mouseAction(int x, int y, int whichButton, boolean buttonDown) {
        if (!sConnected) {
            return;
        }

        mouseActionNative(x, y, whichButton, buttonDown);
    }

    /** Presses and releases the specified (nonnegative) key. */
    public static void keyboardAction(int keyCode, boolean keyDown) {
        if (!sConnected) {
            return;
        }

        keyboardActionNative(keyCode, keyDown);
    }

    /** Performs the native response to the user's PIN. */
    private static native void authenticationResponse(String pin, boolean createPair);

    /** Schedules a redraw on the native graphics thread. */
    private static native void scheduleRedrawNative();

    /** Passes mouse information to the native handling code. */
    private static native void mouseActionNative(int x, int y, int whichButton, boolean buttonDown);

    /** Passes key press information to the native handling code. */
    private static native void keyboardActionNative(int keyCode, boolean keyDown);
}
