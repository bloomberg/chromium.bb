// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientNavigation;
import org.chromium.weblayer_private.interfaces.INavigation;
import org.chromium.weblayer_private.interfaces.INavigationController;
import org.chromium.weblayer_private.interfaces.INavigationControllerClient;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Provides methods to control navigation, along with maintaining the current list of navigations.
 */
public class NavigationController {
    private INavigationController mNavigationController;
    private final ObserverList<NavigationCallback> mCallbacks;

    static NavigationController create(ITab tab) {
        NavigationController navigationController = new NavigationController();
        try {
            navigationController.mNavigationController = tab.createNavigationController(
                    navigationController.new NavigationControllerClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return navigationController;
    }

    // Constructor protected for test mocking.
    protected NavigationController() {
        mCallbacks = new ObserverList<NavigationCallback>();
    }

    public void navigate(@NonNull Uri uri) {
        navigate(uri, null);
    }

    /**
     * Navigates to the given URI, with optional settings.
     *
     * @param uri the destination URI.
     * @param params extra parameters for the navigation.
     *
     * @since 83
     */
    public void navigate(@NonNull Uri uri, @Nullable NavigateParams params) {
        ThreadCheck.ensureOnUiThread();
        if ((WebLayer.getSupportedMajorVersionInternal() < 83) && (params != null)) {
            throw new UnsupportedOperationException();
        }
        try {
            mNavigationController.navigate(
                    uri.toString(), params == null ? null : params.toInterfaceParams());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Navigates to the previous navigation.
     *
     * @throws IndexOutOfBoundsException If {@link #canGoBack} returns false.
     */
    public void goBack() throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        if (!canGoBack()) {
            throw new IndexOutOfBoundsException();
        }
        try {
            mNavigationController.goBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Navigates to the next navigation.
     *
     * @throws IndexOutOfBoundsException If {@link #canGoForward} returns false.
     */
    public void goForward() throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        if (!canGoForward()) {
            throw new IndexOutOfBoundsException();
        }
        try {
            mNavigationController.goForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if there is a navigation before the current one.
     *
     * @return Whether there is a navigation before the current one.
     */
    public boolean canGoBack() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.canGoBack();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns true if there is a navigation after the current one.
     *
     * @return Whether there is a navigation after the current one.
     */
    public boolean canGoForward() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.canGoForward();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Navigates to the entry at {@link index}.
     *
     * @throws IndexOutOfBoundsException If index is not between 0 and {@link
     *         getNavigationListCurrentIndex}.
     * @throws IndexOutOfBoundsException
     *
     * @since 81
     */
    public void goToIndex(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 81) {
            throw new UnsupportedOperationException();
        }
        checkNavigationIndex(index);
        try {
            mNavigationController.goToIndex(index);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Reloads the current entry. Does nothing if there are no navigations.
     */
    public void reload() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.reload();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Stops in progress loading. Does nothing if not in the process of loading.
     */
    public void stop() {
        ThreadCheck.ensureOnUiThread();
        try {
            mNavigationController.stop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the number of navigations entries.
     *
     * @return The number of navigation entries, 0 if empty.
     */
    public int getNavigationListSize() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.getNavigationListSize();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the index of the current navigation, -1 if there are no navigations.
     *
     * @return The index of the current navigation.
     */
    public int getNavigationListCurrentIndex() {
        ThreadCheck.ensureOnUiThread();
        try {
            return mNavigationController.getNavigationListCurrentIndex();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the uri to display for the navigation at index.
     *
     * @param index The index of the navigation.
     * @throws IndexOutOfBoundsException If index is not between 0 and {@link
     *         getNavigationListCurrentIndex}.
     */
    @NonNull
    public Uri getNavigationEntryDisplayUri(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        try {
            return Uri.parse(mNavigationController.getNavigationEntryDisplayUri(index));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void checkNavigationIndex(int index) throws IndexOutOfBoundsException {
        if (index < 0 || index >= getNavigationListSize()) {
            throw new IndexOutOfBoundsException();
        }
    }

    /**
     * Returns the title of the navigation entry at the supplied index.
     *
     * @throws IndexOutOfBoundsException If index is not between 0 and {@link
     *         getNavigationListCurrentIndex}.
     * @since 81
     */
    @NonNull
    public String getNavigationEntryTitle(int index) throws IndexOutOfBoundsException {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 81) {
            throw new UnsupportedOperationException();
        }
        checkNavigationIndex(index);
        try {
            return mNavigationController.getNavigationEntryTitle(index);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void registerNavigationCallback(@NonNull NavigationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    public void unregisterNavigationCallback(@NonNull NavigationCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    private final class NavigationControllerClientImpl extends INavigationControllerClient.Stub {
        @Override
        public IClientNavigation createClientNavigation(INavigation navigationImpl) {
            StrictModeWorkaround.apply();
            return new Navigation(navigationImpl);
        }

        @Override
        public void navigationStarted(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationStarted((Navigation) navigation);
            }
        }

        @Override
        public void navigationRedirected(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationRedirected((Navigation) navigation);
            }
        }

        @Override
        public void readyToCommitNavigation(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onReadyToCommitNavigation((Navigation) navigation);
            }
        }

        @Override
        public void navigationCompleted(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationCompleted((Navigation) navigation);
            }
        }

        @Override
        public void navigationFailed(IClientNavigation navigation) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onNavigationFailed((Navigation) navigation);
            }
        }

        @Override
        public void loadStateChanged(boolean isLoading, boolean toDifferentDocument) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onLoadStateChanged(isLoading, toDifferentDocument);
            }
        }

        @Override
        public void loadProgressChanged(double progress) {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onLoadProgressChanged(progress);
            }
        }

        @Override
        public void onFirstContentfulPaint() {
            StrictModeWorkaround.apply();
            for (NavigationCallback callback : mCallbacks) {
                callback.onFirstContentfulPaint();
            }
        }
    }
}
