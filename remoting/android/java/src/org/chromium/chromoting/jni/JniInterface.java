// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.nio.ByteBuffer;

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

    /** Performs the native portion of the connection. */
    static native void nativeConnect(String username, String authToken, String hostJid,
            String hostId, String hostPubkey, String pairId, String pairSecret,
            String capabilities, String flags);

    /** Performs the native portion of the cleanup. */
    static native void nativeDisconnect();

    /** Called by native code whenever the connection status changes. Called on the UI thread. */
    @CalledByNative
    private static void onConnectionState(int stateCode, int errorCode) {
        if (Client.getInstance() != null) {
            Client.getInstance().onConnectionState(stateCode, errorCode);
        }
    }

    /** Prompts the user to enter a PIN. Called on the UI thread. */
    @CalledByNative
    private static void displayAuthenticationPrompt(boolean pairingSupported) {
        if (Client.getInstance() != null) {
            Client.getInstance().displayAuthenticationPrompt(pairingSupported);
        }
    }

    /** Native implementation of Client.handleAuthenticationResponse(). */
    static native void nativeAuthenticationResponse(
            String pin, boolean createPair, String deviceName);

    /** Saves newly-received pairing credentials to permanent storage. Called on the UI thread. */
    @CalledByNative
    private static void commitPairingCredentials(String host, String id, String secret) {
        if (Client.getInstance() != null) {
            Client.getInstance().commitPairingCredentials(host, id, secret);
        }
    }

    /** Passes mouse information to the native handling code. */
    static native void nativeSendMouseEvent(
            int x, int y, int whichButton, boolean buttonDown);

    /** Passes mouse-wheel information to the native handling code. */
    static native void nativeSendMouseWheelEvent(int deltaX, int deltaY);

    /**
     * Passes key press information to the native handling code.
     */
    static native boolean nativeSendKeyEvent(int scanCode, int keyCode, boolean keyDown);

    /** Passes text event information to the native handling code. */
    static native void nativeSendTextEvent(String text);

    /** Passes touch event information to the native handling code. */
    static native void nativeSendTouchEvent(int eventType, TouchEventData[] data);

    /** Native implementation of Client.enableVideoChannel() */
    static native void nativeEnableVideoChannel(boolean enable);

    /** Schedules a redraw on the native graphics thread. */
    static native void nativeScheduleRedraw();

    /**
     * Performs the redrawing callback. This is a no-op if the window isn't visible. Called on the
     * graphics thread.
     */
    @CalledByNative
    private static void redrawGraphicsInternal() {
        Client client = Client.getInstance();
        if (client != null) {
            client.redrawGraphicsInternal();
        }
    }

    /**
     * Sets a new video frame. Called on the native graphics thread when a new frame is allocated.
     */
    @CalledByNative
    private static void setVideoFrame(Bitmap bitmap) {
        Client client = Client.getInstance();
        if (client != null) {
            client.setVideoFrame(bitmap);
        }
    }

    /**
     * Creates a new Bitmap to hold video frame pixels. Called by native code which stores a global
     * reference to the Bitmap and writes the decoded frame pixels to it.
     */
    @CalledByNative
    private static Bitmap newBitmap(int width, int height) {
        return Client.newBitmap(width, height);
    }

    /**
     * Updates the cursor shape. This is called on the graphics thread when receiving a new cursor
     * shape from the host.
     */
    @CalledByNative
    private static void updateCursorShape(
            int width, int height, int hotspotX, int hotspotY, ByteBuffer buffer) {
        Client client = Client.getInstance();
        if (client != null) {
            client.updateCursorShape(width, height, hotspotX, hotspotY, buffer);
        }
    }

    //
    // Third Party Authentication
    //

    /** Pops up a third party login page to fetch the token required for authentication. */
    @CalledByNative
    private static void fetchThirdPartyToken(String tokenUrl, String clientId, String scope) {
        if (Client.getInstance() != null) {
            Client.getInstance().fetchThirdPartyToken(tokenUrl, clientId, scope);
        }
    }

    /** Passes authentication data to the native handling code. */
    static native void nativeOnThirdPartyTokenFetched(String token, String sharedSecret);

    //
    // Host and Client Capabilities
    //

    /** Set the list of negotiated capabilities between host and client. Called on the UI thread. */
    @CalledByNative
    private static void setCapabilities(String capabilities) {
        if (Client.getInstance() != null) {
            Client.getInstance().setCapabilities(capabilities);
        }
    }

    //
    // Extension Message Handling
    //

    /** Passes on the deconstructed ExtensionMessage to the app. Called on the UI thread. */
    @CalledByNative
    private static void handleExtensionMessage(String type, String data) {
        if (Client.getInstance() != null) {
            Client.getInstance().handleExtensionMessage(type, data);
        }
    }

    /** Passes extension message to the native code. */
    static native void nativeSendExtensionMessage(String type, String data);
}
