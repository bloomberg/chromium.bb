// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A manager for the capabilities of the Android client. Based on the negotiated set of
 * capabilities, it creates the associated ClientExtensions, and enables their communication with
 * the Chromoting host by dispatching extension messages appropriately.
 *
 * The CapabilityManager mirrors how the Chromoting host handles extension messages. For each
 * incoming extension message, runs through a list of HostExtensionSession objects, giving each one
 * a chance to handle the message.
 *
 * The CapabilityManager is a singleton class so we can manage client extensions on an application
 * level. The singleton object may be used from multiple Activities, thus allowing it to support
 * different capabilities at different stages of the application.
 */
public class CapabilityManager {

    /** Lazily-initialized singleton object that can be used from different Activities. */
    private static CapabilityManager sInstance;

    /** Protects access to |sInstance|. */
    private static final Object sInstanceLock = new Object();

    /** List of all capabilities that are supported by the application. */
    private List<String> mLocalCapabilities;

    /** List of negotiated capabilities received from the host. */
    private List<String> mNegotiatedCapabilities;

    /** List of extensions to the client based on capabilities negotiated with the host. */
    private List<ClientExtension> mClientExtensions;

    private CapabilityManager() {
        mLocalCapabilities = new ArrayList<String>();
        mClientExtensions = new ArrayList<ClientExtension>();

        mLocalCapabilities.add(Capabilities.CAST_CAPABILITY);
    }

    /**
     * Returns the singleton object. Thread-safe.
     */
    public static CapabilityManager getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance == null) {
                sInstance = new CapabilityManager();
            }
            return sInstance;
        }
    }

    /**
     * Returns a space-separated list (required by host) of the capabilities supported by
     * this client.
     */
    public String getLocalCapabilities() {
        return TextUtils.join(" ", mLocalCapabilities);
    }

    /**
     * Returns the ActivityLifecycleListener associated with the specified capability, if
     * |capability| is enabled and such a listener exists.
     *
     * Activities that call this method agree to appropriately notify the listener of lifecycle
     * events., thus supporting |capability|. This allows extensions like the CastExtensionHandler
     * to hook into an existing activity's lifecycle.
     */
    public ActivityLifecycleListener onActivityAcceptingListener(
            Activity activity, String capability) {

        ActivityLifecycleListener listener;

        if (isCapabilityEnabled(capability)) {
            for (ClientExtension ext : mClientExtensions) {
                if (ext.getCapability().equals(capability)) {
                    listener = ext.onActivityAcceptingListener(activity);
                    if (listener != null)
                        return listener;
                }
            }
        }

        return new DummyActivityLifecycleListener();
    }

    /**
     * Receives the capabilities negotiated between client and host and creates the appropriate
     * extension handlers.
     *
     * Currently only the CAST_CAPABILITY exists, so that is the only extension constructed.
     */
    public void setNegotiatedCapabilities(String capabilities) {
        mNegotiatedCapabilities = Arrays.asList(capabilities.split(" "));
        mClientExtensions.clear();
        if (isCapabilityEnabled(Capabilities.CAST_CAPABILITY)) {
            mClientExtensions.add(maybeCreateCastExtensionHandler());
        }
    }

    /**
     * Passes the deconstructed extension message to each ClientExtension in turn until the message
     * is handled or none remain. Returns true if the message was handled.
     */
    public boolean onExtensionMessage(String type, String data) {
        if (type == null || type.isEmpty()) {
            return false;
        }

        for (ClientExtension ext : mClientExtensions) {
            if (ext.onExtensionMessage(type, data)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Return true if the capability is enabled for this connection with the host.
     */
    private boolean isCapabilityEnabled(String capability) {
        return (mNegotiatedCapabilities != null && mNegotiatedCapabilities.contains(capability));
    }

    /**
     * Tries to reflectively instantiate a CastExtensionHandler object.
     *
     * Note: The ONLY reason this is done is that by default, the regular android application
     * will be built, without this experimental extension.
     */
    private ClientExtension maybeCreateCastExtensionHandler() {
        try {
            Class<?> cls = Class.forName("org.chromium.chromoting.CastExtensionHandler");
            return (ClientExtension) cls.newInstance();
        } catch (ClassNotFoundException e) {
            Log.w("CapabilityManager", "Failed to create CastExtensionHandler.");
            return new DummyClientExtension();
        } catch (InstantiationException e) {
            Log.w("CapabilityManager", "Failed to create CastExtensionHandler.");
            return new DummyClientExtension();
        } catch (IllegalAccessException e) {
            Log.w("CapabilityManager", "Failed to create CastExtensionHandler.");
            return new DummyClientExtension();
        }
    }
}
