// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.Looper;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromoting.CapabilityManager;
import org.chromium.chromoting.SessionAuthenticator;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Initializes the Chromium remoting library, and provides JNI calls into it.
 * All interaction with the native code is centralized in this class.
 */
@JNINamespace("remoting")
public class JniInterface {
    private static final String TAG = "Chromoting";

    /*
     * Library-loading state machine.
     */
    /** Whether the library has been loaded. Accessed on the UI thread. */
    private static boolean sLoaded = false;

    /** Used for authentication-related UX during connection. Accessed on the UI thread. */
    private static SessionAuthenticator sAuthenticator;

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
     * To be called once from the main Activity. Loads and initializes the native code.
     * Called on the UI thread.
     */
    public static void loadLibrary(Context context) {
        if (sLoaded) return;

        System.loadLibrary("remoting_client_jni");

        ContextUtils.initApplicationContext(context.getApplicationContext());
        nativeLoadNative();
        sLoaded = true;
    }

    /** Performs the native portion of the initialization. */
    private static native void nativeLoadNative();

    /*
     * API/OAuth2 keys access.
     */
    public static native String nativeGetApiKey();
    public static native String nativeGetClientId();
    public static native String nativeGetClientSecret();

    /** Returns whether the client is connected. */
    public static boolean isConnected() {
        return sConnected;
    }

    /** Attempts to form a connection to the user-selected host. Called on the UI thread. */
    public static void connectToHost(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, ConnectionListener listener,
            SessionAuthenticator authenticator, String flags) {
        disconnectFromHost();

        sConnectionListener = listener;
        sAuthenticator = authenticator;
        nativeConnect(username, authToken, hostJid, hostId, hostPubkey,
                sAuthenticator.getPairingId(hostId), sAuthenticator.getPairingSecret(hostId),
                sCapabilityManager.getLocalCapabilities(), flags);
        sConnected = true;
    }

    /** Performs the native portion of the connection. */
    private static native void nativeConnect(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, String pairId, String pairSecret,
            String capabilities, String flags);

    /** Severs the connection and cleans up. Called on the UI thread. */
    public static void disconnectFromHost() {
        if (!sConnected) {
            return;
        }

        sConnectionListener.onConnectionState(
                ConnectionListener.State.CLOSED, ConnectionListener.Error.OK);

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
        sCapabilityManager.onHostDisconnect();

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
        sAuthenticator.displayAuthenticationPrompt(pairingSupported);
    }

    /**
     * Performs the native response to the user's PIN.
     * @param pin The entered PIN.
     * @param createPair Whether to create a new pairing for this client.
     * @param deviceName The device name to appear in the pairing registry. Only used if createPair
     *                   is true.
     */
    public static void handleAuthenticationResponse(
            String pin, boolean createPair, String deviceName) {
        assert sConnected;
        nativeAuthenticationResponse(pin, createPair, deviceName);
    }

    /** Native implementation of handleAuthenticationResponse(). */
    private static native void nativeAuthenticationResponse(
            String pin, boolean createPair, String deviceName);

    /** Saves newly-received pairing credentials to permanent storage. Called on the UI thread. */
    @CalledByNative
    private static void commitPairingCredentials(String host, String id, String secret) {
        sAuthenticator.commitPairingCredentials(host, id, secret);
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
    private static native void nativeSendMouseEvent(
            int x, int y, int whichButton, boolean buttonDown);

    /** Injects a mouse-wheel event with delta values. Called on the UI thread. */
    public static void sendMouseWheelEvent(int deltaX, int deltaY) {
        if (!sConnected) {
            return;
        }

        nativeSendMouseWheelEvent(deltaX, deltaY);
    }

    /** Passes mouse-wheel information to the native handling code. */
    private static native void nativeSendMouseWheelEvent(int deltaX, int deltaY);

    /**
     * Presses or releases the specified (nonnegative) key. Called on the UI thread. If scanCode
     * is not zero then keyCode is ignored.
     */
    public static boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown) {
        if (!sConnected) {
            return false;
        }

        return nativeSendKeyEvent(scanCode, keyCode, keyDown);
    }

    /**
     * Passes key press information to the native handling code.
     */
    private static native boolean nativeSendKeyEvent(int scanCode, int keyCode, boolean keyDown);

    /** Sends TextEvent to the host. Called on the UI thread. */
    public static void sendTextEvent(String text) {
        if (!sConnected) {
            return;
        }

        nativeSendTextEvent(text);
    }

    /** Passes text event information to the native handling code. */
    private static native void nativeSendTextEvent(String text);

    /** Sends an array of TouchEvents to the host. Called on the UI thread. */
    public static void sendTouchEvent(TouchEventData.EventType eventType, TouchEventData[] data) {
        nativeSendTouchEvent(eventType.value(), data);
    }

    /** Passes touch event information to the native handling code. */
    private static native void nativeSendTouchEvent(int eventType, TouchEventData[] data);

    /**
     * Enables or disables the video channel. Called on the UI thread in response to Activity
     * lifecycle events.
     */
    public static void enableVideoChannel(boolean enable) {
        if (!sConnected) {
            return;
        }

        nativeEnableVideoChannel(enable);
    }

    /** Native implementation of enableVideoChannel() */
    private static native void nativeEnableVideoChannel(boolean enable);

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
            Log.w(TAG, "Canvas being redrawn on UI thread");
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
            Log.w(TAG, "Video frame updated on UI thread");
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
    public static void updateCursorShape(
            int width, int height, int hotspotX, int hotspotY, ByteBuffer buffer) {
        sCursorHotspot = new Point(hotspotX, hotspotY);

        int[] data = new int[width * height];
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.asIntBuffer().get(data, 0, data.length);
        sCursorBitmap = Bitmap.createBitmap(data, width, height, Bitmap.Config.ARGB_8888);
    }

    /** Position of cursor hotspot within cursor image. Called on the graphics thread. */
    public static Point getCursorHotspot() {
        return sCursorHotspot;
    }

    /** Returns the current cursor shape. Called on the graphics thread. */
    public static Bitmap getCursorBitmap() {
        return sCursorBitmap;
    }

    //
    // Third Party Authentication
    //

    /** Pops up a third party login page to fetch the token required for authentication. */
    @CalledByNative
    public static void fetchThirdPartyToken(String tokenUrl, String clientId, String scope) {
        sAuthenticator.fetchThirdPartyToken(tokenUrl, clientId, scope);
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
