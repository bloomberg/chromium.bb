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
import android.graphics.Point;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.Toast;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chromoting.R;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Initializes the Chromium remoting library, and provides JNI calls into it.
 * All interaction with the native code is centralized in this class.
 */
@JNINamespace("remoting")
public class JniInterface {
    /** The status code indicating successful connection. */
    private static final int SUCCESSFUL_CONNECTION = 3;

    /*
     * Library-loading state machine.
     */
    /** Whether the library has been loaded. Accessed on the UI thread. */
    private static boolean sLoaded = false;

    /** The application context. Accessed on the UI thread. */
    private static Activity sContext = null;

    /*
     * Connection-initiating state machine.
     */
    /** Whether the native code is attempting a connection. Accessed on the UI thread. */
    private static boolean sConnected = false;

    /** Callback to signal upon successful connection. Accessed on the UI thread. */
    private static Runnable sSuccessCallback = null;

    /** Dialog for reporting connection progress. Accessed on the UI thread. */
    private static ProgressDialog sProgressIndicator = null;

    // Protects access to |sProgressIndicator|. Used only to silence FindBugs warnings - the
    // variable it protects is only accessed on a single thread.
    // TODO(lambroslambrou): Refactor the ProgressIndicator into a separate class.
    private static Object sProgressIndicatorLock = new Object();

    /**
     * Callback invoked on the graphics thread to repaint the desktop. Accessed on the UI and
     * graphics threads.
     */
    private static Runnable sRedrawCallback = null;

    /** Bitmap holding a copy of the latest video frame. Accessed on the UI and graphics threads. */
    private static Bitmap sFrameBitmap = null;

    /** Protects access to sFrameBitmap. */
    private static final Object sFrameLock = new Object();

    /** Position of cursor hot-spot. Accessed on the graphics thread. */
    private static Point sCursorHotspot = new Point();

    /** Bitmap holding the cursor shape. Accessed on the graphics thread. */
    private static Bitmap sCursorBitmap = null;

    /**
     * To be called once from the main Activity. Any subsequent calls will update the application
     * context, but not reload the library. This is useful e.g. when the activity is closed and the
     * user later wants to return to the application. Called on the UI thread.
     */
    public static void loadLibrary(Activity context) {
        sContext = context;

        if (sLoaded) return;

        System.loadLibrary("remoting_client_jni");

        nativeLoadNative(context);
        sLoaded = true;
    }

    /** Performs the native portion of the initialization. */
    private static native void nativeLoadNative(Context context);

    /*
     * API/OAuth2 keys access.
     */
    public static native String nativeGetApiKey();
    public static native String nativeGetClientId();
    public static native String nativeGetClientSecret();

    /** Attempts to form a connection to the user-selected host. Called on the UI thread. */
    public static void connectToHost(String username, String authToken,
            String hostJid, String hostId, String hostPubkey, Runnable successCallback) {
        disconnectFromHost();

        sSuccessCallback = successCallback;
        SharedPreferences prefs = sContext.getPreferences(Activity.MODE_PRIVATE);
        nativeConnect(username, authToken, hostJid, hostId, hostPubkey,
                prefs.getString(hostId + "_id", ""), prefs.getString(hostId + "_secret", ""));
        sConnected = true;
    }

    /** Performs the native portion of the connection. */
    private static native void nativeConnect(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, String pairId, String pairSecret);

    /** Severs the connection and cleans up. Called on the UI thread. */
    public static void disconnectFromHost() {
        if (!sConnected) return;

        synchronized (sProgressIndicatorLock) {
            if (sProgressIndicator != null) {
                sProgressIndicator.dismiss();
                sProgressIndicator = null;
            }
        }

        nativeDisconnect();
        sSuccessCallback = null;
        sConnected = false;

        // Drop the reference to free the Bitmap for GC.
        synchronized (sFrameLock) {
            sFrameBitmap = null;
        }
    }

    /** Performs the native portion of the cleanup. */
    private static native void nativeDisconnect();

    /** Reports whenever the connection status changes. Called on the UI thread. */
    @CalledByNative
    private static void reportConnectionStatus(int state, int error) {
        if (state < SUCCESSFUL_CONNECTION && error == 0) {
            // The connection is still being established, so we'll report the current progress.
            synchronized (sProgressIndicatorLock) {
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
                } else {
                    sProgressIndicator.setMessage(
                            sContext.getResources().getStringArray(R.array.protoc_states)[state]);
                }
            }
        } else {
            // The connection is complete or has failed, so we can lose the progress indicator.
            synchronized (sProgressIndicatorLock) {
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

    /** Prompts the user to enter a PIN. Called on the UI thread. */
    @CalledByNative
    private static void displayAuthenticationPrompt(boolean pairingSupported) {
        AlertDialog.Builder pinPrompt = new AlertDialog.Builder(sContext);
        pinPrompt.setTitle(sContext.getString(R.string.pin_entry_title));
        pinPrompt.setMessage(sContext.getString(R.string.pin_entry_message));
        pinPrompt.setIcon(android.R.drawable.ic_lock_lock);

        final View pinEntry = sContext.getLayoutInflater().inflate(R.layout.pin_dialog, null);
        pinPrompt.setView(pinEntry);

        final TextView pinTextView = (TextView)pinEntry.findViewById(R.id.pin_dialog_text);
        final CheckBox pinCheckBox = (CheckBox)pinEntry.findViewById(R.id.pin_dialog_check);

        if (!pairingSupported) {
            pinCheckBox.setChecked(false);
            pinCheckBox.setVisibility(View.GONE);
        }

        pinPrompt.setPositiveButton(
                R.string.pin_entry_connect, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Log.i("jniiface", "User provided a PIN code");
                        nativeAuthenticationResponse(String.valueOf(pinTextView.getText()),
                                                     pinCheckBox.isChecked());
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

        pinTextView.setOnEditorActionListener(
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

    /** Performs the native response to the user's PIN. */
    private static native void nativeAuthenticationResponse(String pin, boolean createPair);

    /** Saves newly-received pairing credentials to permanent storage. Called on the UI thread. */
    @CalledByNative
    private static void commitPairingCredentials(String host, byte[] id, byte[] secret) {
        sContext.getPreferences(Activity.MODE_PRIVATE).edit().
                putString(host + "_id", new String(id)).
                putString(host + "_secret", new String(secret)).
                apply();
    }

    /**
     * Moves the mouse cursor, possibly while clicking the specified (nonnegative) button. Called
     * on the UI thread.
     */
    public static void mouseAction(int x, int y, int whichButton, boolean buttonDown) {
        if (!sConnected) {
            return;
        }

        nativeMouseAction(x, y, whichButton, buttonDown);
    }

    /** Passes mouse information to the native handling code. */
    private static native void nativeMouseAction(int x, int y, int whichButton, boolean buttonDown);

    /** Injects a mouse-wheel event with delta values. Called on the UI thread. */
    public static void mouseWheelDeltaAction(int deltaX, int deltaY) {
        if (!sConnected) {
            return;
        }

        nativeMouseWheelDeltaAction(deltaX, deltaY);
    }

    /** Passes mouse-wheel information to the native handling code. */
    private static native void nativeMouseWheelDeltaAction(int deltaX, int deltaY);

    /** Presses and releases the specified (nonnegative) key. Called on the UI thread. */
    public static void keyboardAction(int keyCode, boolean keyDown) {
        if (!sConnected) {
            return;
        }

        nativeKeyboardAction(keyCode, keyDown);
    }

    /** Passes key press information to the native handling code. */
    private static native void nativeKeyboardAction(int keyCode, boolean keyDown);

    /**
     * Sets the redraw callback to the provided functor. Provide a value of null whenever the
     * window is no longer visible so that we don't continue to draw onto it. Called on the UI
     * thread.
     */
    public static void provideRedrawCallback(Runnable redrawCallback) {
        sRedrawCallback = redrawCallback;
    }

    /** Forces the native graphics thread to redraw to the canvas. Called on the UI thread. */
    public static boolean redrawGraphics() {
        if (!sConnected || sRedrawCallback == null) return false;

        nativeScheduleRedraw();
        return true;
    }

    /** Schedules a redraw on the native graphics thread. */
    private static native void nativeScheduleRedraw();

    /**
     * Performs the redrawing callback. This is a no-op if the window isn't visible. Called on the
     * graphics thread.
     */
    @CalledByNative
    private static void redrawGraphicsInternal() {
        Runnable callback = sRedrawCallback;
        if (callback != null) {
            callback.run();
        }
    }

    /**
     * Returns a bitmap of the latest video frame. Called on the native graphics thread when
     * DesktopView is repainted.
     */
    public static Bitmap getVideoFrame() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w("jniiface", "Canvas being redrawn on UI thread");
        }

        synchronized (sFrameLock) {
            return sFrameBitmap;
        }
    }

    /**
     * Sets a new video frame. Called on the native graphics thread when a new frame is allocated.
     */
    @CalledByNative
    private static void setVideoFrame(Bitmap bitmap) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w("jniiface", "Video frame updated on UI thread");
        }

        synchronized (sFrameLock) {
            sFrameBitmap = bitmap;
        }
    }

    /**
     * Creates a new Bitmap to hold video frame pixels. Called by native code which stores a global
     * reference to the Bitmap and writes the decoded frame pixels to it.
     */
    @CalledByNative
    private static Bitmap newBitmap(int width, int height) {
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    }

    /**
     * Updates the cursor shape. This is called on the graphics thread when receiving a new cursor
     * shape from the host.
     */
    @CalledByNative
    public static void updateCursorShape(int width, int height, int hotspotX, int hotspotY,
                                         ByteBuffer buffer) {
        sCursorHotspot = new Point(hotspotX, hotspotY);

        int[] data = new int[width * height];
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.asIntBuffer().get(data, 0, data.length);
        sCursorBitmap = Bitmap.createBitmap(data, width, height, Bitmap.Config.ARGB_8888);
    }

    /** Position of cursor hotspot within cursor image. Called on the graphics thread. */
    public static Point getCursorHotspot() { return sCursorHotspot; }

    /** Returns the current cursor shape. Called on the graphics thread. */
    public static Bitmap getCursorBitmap() { return sCursorBitmap; }
}
