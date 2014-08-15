// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.Build;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.Toast;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chromoting.CapabilityManager;
import org.chromium.chromoting.Chromoting;
import org.chromium.chromoting.R;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Initializes the Chromium remoting library, and provides JNI calls into it.
 * All interaction with the native code is centralized in this class.
 */
@JNINamespace("remoting")
public class JniInterface {
    /*
     * Library-loading state machine.
     */
    /** Whether the library has been loaded. Accessed on the UI thread. */
    private static boolean sLoaded = false;

    /** The application context. Accessed on the UI thread. */
    private static Activity sContext = null;

    /** Interface used for connection state notifications. */
    public interface ConnectionListener {
        /**
         * This enum must match the C++ enumeration remoting::protocol::ConnectionToHost::State.
         */
        public enum State {
            INITIALIZING(0),
            CONNECTING(1),
            AUTHENTICATED(2),
            CONNECTED(3),
            FAILED(4),
            CLOSED(5);

            private final int mValue;

            State(int value) {
                mValue = value;
            }

            public int value() {
                return mValue;
            }

            public static State fromValue(int value) {
                return values()[value];
            }
        }

        /**
         * This enum must match the C++ enumeration remoting::protocol::ErrorCode.
         */
        public enum Error {
            OK(0, 0),
            PEER_IS_OFFLINE(1, R.string.error_host_is_offline),
            SESSION_REJECTED(2, R.string.error_invalid_access_code),
            INCOMPATIBLE_PROTOCOL(3, R.string.error_incompatible_protocol),
            AUTHENTICATION_FAILED(4, R.string.error_invalid_access_code),
            CHANNEL_CONNECTION_ERROR(5, R.string.error_p2p_failure),
            SIGNALING_ERROR(6, R.string.error_p2p_failure),
            SIGNALING_TIMEOUT(7, R.string.error_p2p_failure),
            HOST_OVERLOAD(8, R.string.error_host_overload),
            UNKNOWN_ERROR(9, R.string.error_unexpected);

            private final int mValue;
            private final int mMessage;

            Error(int value, int message) {
                mValue = value;
                mMessage = message;
            }

            public int value() {
                return mValue;
            }

            public int message() {
                return mMessage;
            }

            public static Error fromValue(int value) {
                return values()[value];
            }
        }


        /**
         * Notified on connection state change.
         * @param state The new connection state.
         * @param error The error code, if state is STATE_FAILED.
         */
        void onConnectionState(State state, Error error);
    }

    /*
     * Connection-initiating state machine.
     */
    /** Whether the native code is attempting a connection. Accessed on the UI thread. */
    private static boolean sConnected = false;

    /** Notified upon successful connection or disconnection. Accessed on the UI thread. */
    private static ConnectionListener sConnectionListener = null;

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

    /** Capability Manager through which capabilities and extensions are handled. */
    private static CapabilityManager sCapabilityManager = CapabilityManager.getInstance();

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
            String hostJid, String hostId, String hostPubkey, ConnectionListener listener) {
        disconnectFromHost();

        sConnectionListener = listener;
        SharedPreferences prefs = sContext.getPreferences(Activity.MODE_PRIVATE);
        nativeConnect(username, authToken, hostJid, hostId, hostPubkey,
                prefs.getString(hostId + "_id", ""), prefs.getString(hostId + "_secret", ""),
                sCapabilityManager.getLocalCapabilities());
        sConnected = true;
    }

    /** Performs the native portion of the connection. */
    private static native void nativeConnect(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, String pairId, String pairSecret,
            String capabilities);

    /** Severs the connection and cleans up. Called on the UI thread. */
    public static void disconnectFromHost() {
        if (!sConnected) {
            return;
        }

        sConnectionListener.onConnectionState(ConnectionListener.State.CLOSED,
                ConnectionListener.Error.OK);

        disconnectFromHostWithoutNotification();
    }

    /** Same as disconnectFromHost() but without notifying the ConnectionListener. */
    private static void disconnectFromHostWithoutNotification() {
        if (!sConnected) {
            return;
        }

        nativeDisconnect();
        sConnectionListener = null;
        sConnected = false;

        // Drop the reference to free the Bitmap for GC.
        synchronized (sFrameLock) {
            sFrameBitmap = null;
        }
    }

    /** Performs the native portion of the cleanup. */
    private static native void nativeDisconnect();

    /** Called by native code whenever the connection status changes. Called on the UI thread. */
    @CalledByNative
    private static void onConnectionState(int stateCode, int errorCode) {
        ConnectionListener.State state = ConnectionListener.State.fromValue(stateCode);
        ConnectionListener.Error error = ConnectionListener.Error.fromValue(errorCode);
        sConnectionListener.onConnectionState(state, error);
        if (state == ConnectionListener.State.FAILED || state == ConnectionListener.State.CLOSED) {
            // Disconnect from the host here, otherwise the next time connectToHost() is called,
            // it will try to disconnect, triggering an incorrect status notification.
            disconnectFromHostWithoutNotification();
        }
    }

    /** Prompts the user to enter a PIN. Called on the UI thread. */
    @CalledByNative
    private static void displayAuthenticationPrompt(boolean pairingSupported) {
        AlertDialog.Builder pinPrompt = new AlertDialog.Builder(sContext);
        pinPrompt.setTitle(sContext.getString(R.string.title_authenticate));
        pinPrompt.setMessage(sContext.getString(R.string.pin_message_android));
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
                R.string.connect_button, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Log.i("jniiface", "User provided a PIN code");
                        if (sConnected) {
                            nativeAuthenticationResponse(String.valueOf(pinTextView.getText()),
                                    pinCheckBox.isChecked(), Build.MODEL);
                        } else {
                            String message = sContext.getString(R.string.error_network_error);
                            Toast.makeText(sContext, message, Toast.LENGTH_LONG).show();
                        }
                    }
                });

        pinPrompt.setNegativeButton(
                R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        Log.i("jniiface", "User canceled pin entry prompt");
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

    /**
     * Performs the native response to the user's PIN.
     * @param pin The entered PIN.
     * @param createPair Whether to create a new pairing for this client.
     * @param deviceName The device name to appear in the pairing registry. Only used if createPair
     *                   is true.
     */
    private static native void nativeAuthenticationResponse(String pin, boolean createPair,
            String deviceName);

    /** Saves newly-received pairing credentials to permanent storage. Called on the UI thread. */
    @CalledByNative
    private static void commitPairingCredentials(String host, String id, String secret) {
        // Empty |id| indicates that pairing needs to be removed.
        if (id.isEmpty()) {
            sContext.getPreferences(Activity.MODE_PRIVATE).edit().
                    remove(host + "_id").
                    remove(host + "_secret").
                    apply();
        } else {
            sContext.getPreferences(Activity.MODE_PRIVATE).edit().
                    putString(host + "_id", id).
                    putString(host + "_secret", secret).
                    apply();
        }
    }

    /**
     * Moves the mouse cursor, possibly while clicking the specified (nonnegative) button. Called
     * on the UI thread.
     */
    public static void sendMouseEvent(int x, int y, int whichButton, boolean buttonDown) {
        if (!sConnected) {
            return;
        }

        nativeSendMouseEvent(x, y, whichButton, buttonDown);
    }

    /** Passes mouse information to the native handling code. */
    private static native void nativeSendMouseEvent(int x, int y, int whichButton,
            boolean buttonDown);

    /** Injects a mouse-wheel event with delta values. Called on the UI thread. */
    public static void sendMouseWheelEvent(int deltaX, int deltaY) {
        if (!sConnected) {
            return;
        }

        nativeSendMouseWheelEvent(deltaX, deltaY);
    }

    /** Passes mouse-wheel information to the native handling code. */
    private static native void nativeSendMouseWheelEvent(int deltaX, int deltaY);

    /** Presses or releases the specified (nonnegative) key. Called on the UI thread. */
    public static boolean sendKeyEvent(int keyCode, boolean keyDown) {
        if (!sConnected) {
            return false;
        }

        return nativeSendKeyEvent(keyCode, keyDown);
    }

    /** Passes key press information to the native handling code. */
    private static native boolean nativeSendKeyEvent(int keyCode, boolean keyDown);

    /** Sends TextEvent to the host. Called on the UI thread. */
    public static void sendTextEvent(String text) {
        if (!sConnected) {
            return;
        }

        nativeSendTextEvent(text);
    }

    /** Passes text event information to the native handling code. */
    private static native void nativeSendTextEvent(String text);

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

    //
    // Third Party Authentication
    //

    /** Pops up a third party login page to fetch the token required for authentication. */
    @CalledByNative
    public static void fetchThirdPartyToken(String tokenUrl, String clientId, String scope) {
        Chromoting app = (Chromoting) sContext;
        app.fetchThirdPartyToken(tokenUrl, clientId, scope);
    }

    /**
     * Notify the native code to continue authentication with the |token| and the |sharedSecret|.
     */
    public static void onThirdPartyTokenFetched(String token, String sharedSecret) {
        if (!sConnected) {
            return;
        }

        nativeOnThirdPartyTokenFetched(token, sharedSecret);
    }

    /** Passes authentication data to the native handling code. */
    private static native void nativeOnThirdPartyTokenFetched(String token, String sharedSecret);

    //
    // Host and Client Capabilities
    //

    /** Set the list of negotiated capabilities between host and client. Called on the UI thread. */
    @CalledByNative
    public static void setCapabilities(String capabilities) {
        sCapabilityManager.setNegotiatedCapabilities(capabilities);
    }

    //
    // Extension Message Handling
    //

    /** Passes on the deconstructed ExtensionMessage to the app. Called on the UI thread. */
    @CalledByNative
    public static void handleExtensionMessage(String type, String data) {
        sCapabilityManager.onExtensionMessage(type, data);
    }

    /** Sends an extension message to the Chromoting host. Called on the UI thread. */
    public static void sendExtensionMessage(String type, String data) {
        if (!sConnected) {
            return;
        }

        nativeSendExtensionMessage(type, data);
    }

    private static native void nativeSendExtensionMessage(String type, String data);
}
