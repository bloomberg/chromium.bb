// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.Looper;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chromoting.CapabilityManager;
import org.chromium.chromoting.SessionAuthenticator;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Class to manage a client connection to the host. This class controls the lifetime of the
 * corresponding C++ object which implements the connection. A new object should be created for
 * each connection to the host, so that notifications from a connection are always sent to the
 * right object.
 */
@JNINamespace("remoting")
public class Client {
    private static final String TAG = "Chromoting";

    // Pointer to the C++ object, cast to a |long|.
    private long mNativeJniClient;

    // The global Client instance (may be null). This needs to be a global singleton so that the
    // Client can be passed between Activities.
    private static Client sClient;

    // Called on the UI thread.
    public Client() {
        if (sClient != null) {
            throw new RuntimeException("Client instance already created.");
        }

        sClient = this;
        mNativeJniClient = nativeInit();
    }

    // Called on the UI thread. Suppress FindBugs warning, since |sClient| is only used on the
    // UI thread.
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC")
    public void destroy() {
        if (sClient != null) {
            disconnectFromHost();
            nativeDestroy(mNativeJniClient);
            sClient = null;
        }
    }

    /** Returns the current Client instance, or null. */
    public static Client getInstance() {
        return sClient;
    }

    /** Used for authentication-related UX during connection. Accessed on the UI thread. */
    private SessionAuthenticator mAuthenticator;

    /** Whether the native code is attempting a connection. Accessed on the UI thread. */
    private boolean mConnected;

    /** Notified upon successful connection or disconnection. Accessed on the UI thread. */
    private ConnectionListener mConnectionListener;

    /**
     * Callback invoked on the graphics thread to repaint the desktop. Accessed on the UI and
     * graphics threads.
     */
    private Runnable mRedrawCallback;

    /** Bitmap holding a copy of the latest video frame. Accessed on the UI and graphics threads. */
    private Bitmap mFrameBitmap;

    /** Protects access to {@link mFrameBitmap}. */
    private final Object mFrameLock = new Object();

    /** Position of cursor hot-spot. Accessed on the graphics thread. */
    private Point mCursorHotspot = new Point();

    /** Bitmap holding the cursor shape. Accessed on the graphics thread. */
    private Bitmap mCursorBitmap;

    /** Capability Manager through which capabilities and extensions are handled. */
    private CapabilityManager mCapabilityManager = new CapabilityManager();

    public CapabilityManager getCapabilityManager() {
        return mCapabilityManager;
    }

    /** Returns whether the client is connected. */
    public boolean isConnected() {
        return mConnected;
    }

    /** Attempts to form a connection to the user-selected host. Called on the UI thread. */
    public void connectToHost(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, SessionAuthenticator authenticator, String flags,
            ConnectionListener listener) {
        disconnectFromHost();

        mConnectionListener = listener;
        mAuthenticator = authenticator;
        nativeConnect(mNativeJniClient, username, authToken, hostJid, hostId, hostPubkey,
                mAuthenticator.getPairingId(hostId), mAuthenticator.getPairingSecret(hostId),
                mCapabilityManager.getLocalCapabilities(), flags);
        mConnected = true;
    }

    /** Severs the connection and cleans up. Called on the UI thread. */
    public void disconnectFromHost() {
        if (!mConnected) {
            return;
        }

        mConnectionListener.onConnectionState(
                ConnectionListener.State.CLOSED, ConnectionListener.Error.OK);

        disconnectFromHostWithoutNotification();
    }

    /** Same as disconnectFromHost() but without notifying the ConnectionListener. */
    private void disconnectFromHostWithoutNotification() {
        if (!mConnected) {
            return;
        }

        nativeDisconnect(mNativeJniClient);
        mConnectionListener = null;
        mConnected = false;
        mCapabilityManager.onHostDisconnect();

        // Drop the reference to free the Bitmap for GC.
        synchronized (mFrameLock) {
            mFrameBitmap = null;
        }
    }

    /** Called on the UI thread whenever the connection status changes. */
    @CalledByNative
    void onConnectionState(int stateCode, int errorCode) {
        ConnectionListener.State state = ConnectionListener.State.fromValue(stateCode);
        ConnectionListener.Error error = ConnectionListener.Error.fromValue(errorCode);
        mConnectionListener.onConnectionState(state, error);
        if (state == ConnectionListener.State.FAILED || state == ConnectionListener.State.CLOSED) {
            // Disconnect from the host here, otherwise the next time connectToHost() is called,
            // it will try to disconnect, triggering an incorrect status notification.

            // TODO(lambroslambrou): Connection state notifications for separate sessions should
            // go to separate Client instances. Once this is true, we can remove this line and
            // simplify the disconnectFromHost() code.
            disconnectFromHostWithoutNotification();
        }
    }

    /**
     * Called on the UI thread to prompt the user to enter a PIN.
     */
    @CalledByNative
    void displayAuthenticationPrompt(boolean pairingSupported) {
        mAuthenticator.displayAuthenticationPrompt(pairingSupported);
    }

    /**
     * Called by the SessionAuthenticator after the user enters a PIN.
     * @param pin The entered PIN.
     * @param createPair Whether to create a new pairing for this client.
     * @param deviceName The device name to appear in the pairing registry. Only used if createPair
     *                   is true.
     */
    public void handleAuthenticationResponse(
            String pin, boolean createPair, String deviceName) {
        assert mConnected;
        nativeAuthenticationResponse(mNativeJniClient, pin, createPair, deviceName);
    }

    /**
     * Called on the UI thread to save newly-received pairing credentials to permanent storage.
     */
    @CalledByNative
    void commitPairingCredentials(String host, String id, String secret) {
        mAuthenticator.commitPairingCredentials(host, id, secret);
    }

    /**
     * Moves the mouse cursor, possibly while clicking the specified (nonnegative) button. Called
     * on the UI thread.
     */
    public void sendMouseEvent(int x, int y, int whichButton, boolean buttonDown) {
        if (!mConnected) {
            return;
        }

        nativeSendMouseEvent(mNativeJniClient, x, y, whichButton, buttonDown);
    }

    /** Injects a mouse-wheel event with delta values. Called on the UI thread. */
    public void sendMouseWheelEvent(int deltaX, int deltaY) {
        if (!mConnected) {
            return;
        }

        nativeSendMouseWheelEvent(mNativeJniClient, deltaX, deltaY);
    }

    /**
     * Presses or releases the specified key. Called on the UI thread. If scanCode is not zero then
     * keyCode is ignored.
     */
    public boolean sendKeyEvent(int scanCode, int keyCode, boolean keyDown) {
        if (!mConnected) {
            return false;
        }

        return nativeSendKeyEvent(mNativeJniClient, scanCode, keyCode, keyDown);
    }

    /** Sends TextEvent to the host. Called on the UI thread. */
    public void sendTextEvent(String text) {
        if (!mConnected) {
            return;
        }

        nativeSendTextEvent(mNativeJniClient, text);
    }

    /** Sends an array of TouchEvents to the host. Called on the UI thread. */
    public void sendTouchEvent(TouchEventData.EventType eventType, TouchEventData[] data) {
        if (!mConnected) {
            return;
        }

        nativeSendTouchEvent(mNativeJniClient, eventType.value(), data);
    }

    /**
     * Enables or disables the video channel. Called on the UI thread in response to Activity
     * lifecycle events.
     */
    public void enableVideoChannel(boolean enable) {
        if (!mConnected) {
            return;
        }

        nativeEnableVideoChannel(mNativeJniClient, enable);
    }

    /**
     * Sets the redraw callback to the provided functor. Provide a value of null whenever the
     * window is no longer visible so that we don't continue to draw onto it. Called on the UI
     * thread.
     */
    public void provideRedrawCallback(Runnable redrawCallback) {
        mRedrawCallback = redrawCallback;
    }

    /** Forces the native graphics thread to redraw to the canvas. Called on the UI thread. */
    public boolean redrawGraphics() {
        if (!mConnected || mRedrawCallback == null) return false;

        nativeScheduleRedraw(mNativeJniClient);
        return true;
    }

    /**
     * Called on the graphics thread to perform the redrawing callback requested by
     * {@link #redrawGraphics}. This is a no-op if the window isn't visible (the callback is null).
     */
    @CalledByNative
    void redrawGraphicsInternal() {
        Runnable callback = mRedrawCallback;
        if (callback != null) {
            callback.run();
        }
    }

    /**
     * Returns a bitmap of the latest video frame. Called on the native graphics thread when
     * DesktopView is repainted.
     */
    public Bitmap getVideoFrame() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w(TAG, "Canvas being redrawn on UI thread");
        }

        synchronized (mFrameLock) {
            return mFrameBitmap;
        }
    }

    /**
     * Set a new video frame. Called on the native graphics thread when a new frame is allocated.
     */
    @CalledByNative
    void setVideoFrame(Bitmap bitmap) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w(TAG, "Video frame updated on UI thread");
        }

        synchronized (mFrameLock) {
            mFrameBitmap = bitmap;
        }
    }

    /**
     * Creates a new Bitmap to hold video frame pixels. The returned Bitmap is referenced by native
     * code which writes the decoded frame pixels to it.
     */
    @CalledByNative
    static Bitmap newBitmap(int width, int height) {
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    }

    /**
     * Updates the cursor shape. This is called on the graphics thread when receiving a new cursor
     * shape from the host.
     */
    @CalledByNative
    void updateCursorShape(int width, int height, int hotspotX, int hotspotY, ByteBuffer buffer) {
        mCursorHotspot = new Point(hotspotX, hotspotY);

        int[] data = new int[width * height];
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.asIntBuffer().get(data, 0, data.length);
        mCursorBitmap = Bitmap.createBitmap(data, width, height, Bitmap.Config.ARGB_8888);
    }

    /** Position of cursor hotspot within cursor image. Called on the graphics thread. */
    public Point getCursorHotspot() {
        return mCursorHotspot;
    }

    /** Returns the current cursor shape. Called on the graphics thread. */
    public Bitmap getCursorBitmap() {
        return mCursorBitmap;
    }

    //
    // Third Party Authentication
    //

    /**
     * Pops up a third party login page to fetch the token required for authentication.
     */
    @CalledByNative
    void fetchThirdPartyToken(String tokenUrl, String clientId, String scope) {
        mAuthenticator.fetchThirdPartyToken(tokenUrl, clientId, scope);
    }

    /**
     * Called by the SessionAuthenticator to pass the |token| and |sharedSecret| to native code to
     * continue authentication.
     */
    public void onThirdPartyTokenFetched(String token, String sharedSecret) {
        if (!mConnected) {
            return;
        }

        nativeOnThirdPartyTokenFetched(mNativeJniClient, token, sharedSecret);
    }

    //
    // Host and Client Capabilities
    //

    /**
     * Sets the list of negotiated capabilities between host and client. Called on the UI thread.
     */
    @CalledByNative
    void setCapabilities(String capabilities) {
        mCapabilityManager.setNegotiatedCapabilities(capabilities);
    }

    //
    // Extension Message Handling
    //

    /**
     * Passes on the deconstructed ExtensionMessage to the app. Called on the UI thread.
     */
    @CalledByNative
    void handleExtensionMessage(String type, String data) {
        mCapabilityManager.onExtensionMessage(type, data);
    }

    /** Sends an extension message to the Chromoting host. Called on the UI thread. */
    public void sendExtensionMessage(String type, String data) {
        if (!mConnected) {
            return;
        }

        nativeSendExtensionMessage(mNativeJniClient, type, data);
    }

    private native long nativeInit();

    private native void nativeDestroy(long nativeJniClient);

    /** Performs the native portion of the connection. */
    private native void nativeConnect(long nativeJniClient, String username, String authToken,
            String hostJid, String hostId, String hostPubkey, String pairId, String pairSecret,
            String capabilities, String flags);

    /** Native implementation of Client.handleAuthenticationResponse(). */
    private native void nativeAuthenticationResponse(
            long nativeJniClient, String pin, boolean createPair, String deviceName);

    /** Performs the native portion of the cleanup. */
    private native void nativeDisconnect(long nativeJniClient);

    /** Schedules a redraw on the native graphics thread. */
    private native void nativeScheduleRedraw(long nativeJniClient);

    /** Passes authentication data to the native handling code. */
    private native void nativeOnThirdPartyTokenFetched(
            long nativeJniClient, String token, String sharedSecret);

    /** Passes mouse information to the native handling code. */
    private native void nativeSendMouseEvent(
            long nativeJniClient, int x, int y, int whichButton, boolean buttonDown);

    /** Passes mouse-wheel information to the native handling code. */
    private native void nativeSendMouseWheelEvent(long nativeJniClient, int deltaX, int deltaY);

    /** Passes key press information to the native handling code. */
    private native boolean nativeSendKeyEvent(
            long nativeJniClient, int scanCode, int keyCode, boolean keyDown);

    /** Passes text event information to the native handling code. */
    private native void nativeSendTextEvent(long nativeJniClient, String text);

    /** Passes touch event information to the native handling code. */
    private native void nativeSendTouchEvent(
            long nativeJniClient, int eventType, TouchEventData[] data);

    /** Native implementation of Client.enableVideoChannel() */
    private native void nativeEnableVideoChannel(long nativeJniClient, boolean enable);

    /** Passes extension message to the native code. */
    private native void nativeSendExtensionMessage(long nativeJniClient, String type, String data);
}
