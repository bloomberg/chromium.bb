// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Bundle;
import android.os.RemoteException;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserFragment;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

import java.util.concurrent.Future;

/**
 * WebLayer's fragment implementation.
 *
 * All the browser APIs, such as loading pages can be accessed via
 * {@link BrowserFragmentController}, which can be retrieved with {@link #getController} after
 * the fragment received onCreate the call.
 *
 * Attaching a BrowserFragment to an Activity requires WebLayer to be initialized, so
 * BrowserFragment will block the thread in onAttach until it's done. To prevent this,
 * asynchronously "pre-warm" WebLayer using {@link WebLayer#create} prior to using BrowserFragments.
 *
 * Unfortunately, when the system restores the BrowserFragment after killing the process, it
 * attaches the fragment immediately on activity's onCreate event, so there is currently no way to
 * asynchronously init WebLayer in that case.
 */
public final class BrowserFragment extends Fragment {
    private final IRemoteFragmentClient mClientImpl = new IRemoteFragmentClient.Stub() {
        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {
            BrowserFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnAttach(IObjectWrapper context) {
            BrowserFragment.super.onAttach(ObjectWrapper.unwrap(context, Context.class));
        }

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {
            BrowserFragment.super.onCreate(ObjectWrapper.unwrap(savedInstanceState, Bundle.class));
        }

        @Override
        public void superOnStart() {
            BrowserFragment.super.onStart();
        }

        @Override
        public void superOnResume() {
            BrowserFragment.super.onResume();
        }

        @Override
        public void superOnPause() {
            BrowserFragment.super.onPause();
        }

        @Override
        public void superOnStop() {
            BrowserFragment.super.onStop();
        }

        @Override
        public void superOnDestroyView() {
            BrowserFragment.super.onDestroyView();
        }

        @Override
        public void superOnDetach() {
            BrowserFragment.super.onDetach();
        }

        @Override
        public void superOnDestroy() {
            BrowserFragment.super.onDestroy();
        }

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {
            BrowserFragment.super.onSaveInstanceState(ObjectWrapper.unwrap(outState, Bundle.class));
        }

        @Override
        public IObjectWrapper getActivity() {
            return ObjectWrapper.wrap(BrowserFragment.this.getActivity());
        }
    };

    // Nonnull after first onAttach().
    private IBrowserFragment mImpl;
    private WebLayer mWebLayer;

    // Nonnull between onCreate() and onDestroy().
    private BrowserFragmentController mBrowserFragmentController;

    /**
     * This constructor is for the system FragmentManager only. Please use
     * {@link WebLayer#createBrowserFragment}.
     */
    public BrowserFragment() {
        super();
    }

    /**
     * Returns the {@link BrowserFragmentController} associated with this fragment.
     * The controller is available only between BrowserFragment's onCreate() and onDestroy().
     */
    public BrowserFragmentController getController() {
        if (mBrowserFragmentController == null) {
            throw new RuntimeException("BrowserFragmentController is available only between "
                    + "BrowserFragment's onCreate() and onDestroy().");
        }
        return mBrowserFragmentController;
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onAttach(Context context) {
        // This is the first lifecycle event and also the first time we can get app context (unless
        // the embedder has already called getController). So it's the latest and at the same time
        // the earliest moment when we can initialize WebLayer without missing any lifecycle events.
        ensureConnectedToWebLayer(context.getApplicationContext());
        try {
            mImpl.asRemoteFragment().handleOnAttach(ObjectWrapper.wrap(context));
            // handleOnAttach results in creating BrowserFragmentControllerImpl on the other side.
            // Now we retrieve it, and build BrowserFragmentController on this side.
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private void ensureConnectedToWebLayer(Context appContext) {
        if (mImpl != null) {
            return; // Already initialized.
        }
        Bundle args = getArguments();
        if (args == null) {
            throw new RuntimeException("BrowserFragment was created without arguments.");
        }
        try {
            Future<WebLayer> future = WebLayer.create(appContext);
            mWebLayer = future.get();
            mImpl = mWebLayer.connectFragment(mClientImpl, args);
        } catch (Exception e) {
            throw new RuntimeException("Failed to initialize WebLayer", e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        try {
            mImpl.asRemoteFragment().handleOnCreate(ObjectWrapper.wrap(savedInstanceState));
            mBrowserFragmentController = new BrowserFragmentController(mImpl.getController(),
                    mWebLayer.getProfileManager());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        try {
            return ObjectWrapper.unwrap(mImpl.asRemoteFragment().handleOnCreateView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        try {
            mImpl.asRemoteFragment().handleOnActivityCreated(
                    ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStart() {
        try {
            mImpl.asRemoteFragment().handleOnStart();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onResume() {
        try {
            mImpl.asRemoteFragment().handleOnResume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onSaveInstanceState(Bundle outState) {
        try {
            mImpl.asRemoteFragment().handleOnSaveInstanceState(ObjectWrapper.wrap(outState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onPause() {
        try {
            mImpl.asRemoteFragment().handleOnPause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStop() {
        try {
            mImpl.asRemoteFragment().handleOnStop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroyView() {
        try {
            mImpl.asRemoteFragment().handleOnDestroyView();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroy() {
        try {
            mImpl.asRemoteFragment().handleOnDestroy();
            // The other side does the clean up automatically in handleOnDestroy()
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        mBrowserFragmentController = null;
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDetach() {
        try {
            mImpl.asRemoteFragment().handleOnDetach();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
