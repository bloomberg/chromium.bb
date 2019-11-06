// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.INavigation;

import java.util.ArrayList;
import java.util.List;

/**
 * Information about a navigation.
 */
public final class Navigation extends IClientNavigation.Stub {
    private final INavigation mNavigationImpl;
    // TODO(sky): investigate using java_cpp_enum.
    public enum State {
        WAITING_RESPONSE,
        RECEIVING_BYTES,
        COMPLETE,
        FAILED,
    }

    private static State ipcStateToState(int state) {
        switch (state) {
            case 0:
                return State.WAITING_RESPONSE;
            case 1:
                return State.RECEIVING_BYTES;
            case 2:
                return State.COMPLETE;
            case 3:
                return State.FAILED;
            default:
                throw new IllegalArgumentException("Unexpected state " + state);
        }
    }

    Navigation(INavigation impl) {
        mNavigationImpl = impl;
    }

    public State getState() {
        ThreadCheck.ensureOnUiThread();
        try {
            return ipcStateToState(mNavigationImpl.getState());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * The uri the main frame is navigating to. This may change during the navigation when
     * encountering a server redirect.
     */
    @NonNull
    public Uri getUri() {
        ThreadCheck.ensureOnUiThread();
        try {
            return Uri.parse(mNavigationImpl.getUri());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the redirects that occurred on the way to the current page. The current page is the
     * last one in the list (so even when there's no redirect, there will be one entry in the list).
     */
    @NonNull
    public List<Uri> getRedirectChain() {
        ThreadCheck.ensureOnUiThread();
        try {
            List<Uri> redirects = new ArrayList<Uri>();
            for (String r : mNavigationImpl.getRedirectChain()) redirects.add(Uri.parse(r));
            return redirects;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Whether the navigation happened without changing document. Examples of same document
     * navigations are:
     *  - reference fragment navigations
     *  - pushState/replaceState
     *  - same page history navigation
     */
    public boolean isSameDocument() {
        try {
            return mNavigationImpl.isSameDocument();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
