// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.host.jni;

/** Interface for receiving events from the C++ implementation of It2MeHost. */
public interface It2MeHostObserver {
    /** This corresponds to the C++ enumeration remoting::It2MeHostState. */
    public enum State {
        DISCONNECTED,
        STARTING,
        REQUESTED_ACCESS_CODE,
        RECEIVED_ACCESS_CODE,
        CONNECTED,
        ERROR,
        INVALID_DOMAIN_ERROR;
    }

    /**
     * Called when a new It2Me access-code is received.
     * @param accessCode The access code for the It2Me session.
     * @param lifetimeSeconds Duration for which the access-code remains valid.
     */
    void onAccessCodeReceived(String accessCode, int lifetimeSeconds);

    /**
     * Called when the state has changed.
     * @param state The new state.
     * @param errorMessage Used only for the ERROR state. The string is not localized and not
     *     intended to be shown to a user.
     */
    void onStateChanged(State state, String errorMessage);
}
