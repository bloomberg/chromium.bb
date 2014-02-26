// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import org.chromium.chromoting.jni.JniInterface;

/**
 * This class manages making a connection to a host, with logic for reloading the host list and
 * retrying the connection in the case of a stale host JID.
 */
public class SessionConnector implements JniInterface.ConnectionListener,
        HostListLoader.Callback {
    private JniInterface.ConnectionListener mConnectionCallback;
    private HostListLoader.Callback mHostListCallback;
    private HostListLoader mHostListLoader;

    private String mAccountName;
    private String mAuthToken;

    /** Used to find the HostInfo from the returned array after the host list is reloaded. */
    private String mHostId;

    private String mHostJabberId;

    /**
     * @param connectionCallback Object to be notified on connection success/failure.
     * @param hostListCallback Object to be notified whenever the host list is reloaded.
     * @param hostListLoader The object used for reloading the host list.
     */
    public SessionConnector(JniInterface.ConnectionListener connectionCallback,
            HostListLoader.Callback hostListCallback, HostListLoader hostListLoader) {
        mConnectionCallback = connectionCallback;
        mHostListCallback = hostListCallback;
        mHostListLoader = hostListLoader;
    }

    /** Initiates a connection to the host. */
    public void connectToHost(String accountName, String authToken, HostInfo host) {
        mAccountName = accountName;
        mAuthToken = authToken;
        mHostId = host.id;
        mHostJabberId = host.jabberId;

        if (hostIncomplete(host)) {
            // These keys might not be present in a newly-registered host, so treat this as a
            // connection failure and reload the host list.
            reloadHostListAndConnect();
            return;
        }

        JniInterface.connectToHost(accountName, authToken, host.jabberId, host.id, host.publicKey,
                this);
    }

    private static boolean hostIncomplete(HostInfo host) {
        return host.jabberId.isEmpty() || host.publicKey.isEmpty();
    }

    private void reloadHostListAndConnect() {
        mHostListLoader.retrieveHostList(mAuthToken, this);
    }

    @Override
    public void onConnectionState(JniInterface.ConnectionListener.State state,
            JniInterface.ConnectionListener.Error error) {
        if (state == JniInterface.ConnectionListener.State.FAILED &&
                error == JniInterface.ConnectionListener.Error.PEER_IS_OFFLINE) {
            // The host is offline, which may mean the JID is out of date, so refresh the host list
            // and try to connect again.
            reloadHostListAndConnect();
        } else {
            // Pass the state/error back to the caller.
            mConnectionCallback.onConnectionState(state, error);
        }
    }

    @Override
    public void onHostListReceived(HostInfo[] hosts) {
        // Notify the caller, so the UI is updated.
        mHostListCallback.onHostListReceived(hosts);

        HostInfo foundHost = null;
        for (HostInfo host : hosts) {
            if (host.id.equals(mHostId)) {
                foundHost = host;
                break;
            }
        }

        if (foundHost == null || foundHost.jabberId.equals(mHostJabberId)
                || hostIncomplete(foundHost)) {
            // Cannot reconnect to this host, or there's no point in trying because the JID is
            // unchanged, so report the original failure to the client.
            mConnectionCallback.onConnectionState(JniInterface.ConnectionListener.State.FAILED,
                    JniInterface.ConnectionListener.Error.PEER_IS_OFFLINE);
        } else {
            // Reconnect to the host, but use the original callback directly, instead of this
            // wrapper object, so the host list is not loaded again.
            JniInterface.connectToHost(mAccountName, mAuthToken, foundHost.jabberId,
                    foundHost.id, foundHost.publicKey, mConnectionCallback);
        }
    }

    @Override
    public void onError(HostListLoader.Error error) {
        // Connection failed and reloading the host list also failed, so report the connection
        // error.
        mConnectionCallback.onConnectionState(JniInterface.ConnectionListener.State.FAILED,
                JniInterface.ConnectionListener.Error.PEER_IS_OFFLINE);

        // Notify the caller that the host list failed to load, so the UI is updated accordingly.
        // The currently-displayed host list is not likely to be valid any more.
        mHostListCallback.onError(error);
    }
}
