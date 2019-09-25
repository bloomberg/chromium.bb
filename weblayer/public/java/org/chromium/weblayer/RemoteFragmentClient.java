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
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IRemoteFragment;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Hosts a "remote fragment" (represented by {@link IRemoteFragment}) that lives in another
 * ClassLoader. The remote fragment has all the actual logic (e.g. handling lifecycle events), while
 * this class actually extends {@link Fragment}, forwarding the calls to and from the remote
 * fragment. Thus it is "hosting" the fragment implemented elsewhere.
 */
public final class RemoteFragmentClient extends Fragment {
    private final IRemoteFragmentClient mClientImpl = new IRemoteFragmentClient.Stub() {
        @Override
        public void superOnCreate(IObjectWrapper savedInstanceState) {
            RemoteFragmentClient.super.onCreate(ObjectWrapper.unwrap(savedInstanceState,
                    Bundle.class));
        }

        @Override
        public void superOnAttach(IObjectWrapper context) {
            RemoteFragmentClient.super.onAttach(ObjectWrapper.unwrap(context, Context.class));
        }

        @Override
        public void superOnActivityCreated(IObjectWrapper savedInstanceState) {
            RemoteFragmentClient.super.onCreate(ObjectWrapper.unwrap(savedInstanceState,
                    Bundle.class));
        }

        @Override
        public void superOnStart() {
            RemoteFragmentClient.super.onStart();
        }

        @Override
        public void superOnResume() {
            RemoteFragmentClient.super.onResume();
        }

        @Override
        public void superOnPause() {
            RemoteFragmentClient.super.onPause();
        }

        @Override
        public void superOnStop() {
            RemoteFragmentClient.super.onStop();
        }

        @Override
        public void superOnDestroyView() {
            RemoteFragmentClient.super.onDestroyView();
        }

        @Override
        public void superOnDetach() {
            RemoteFragmentClient.super.onDetach();
        }

        @Override
        public void superOnDestroy() {
            RemoteFragmentClient.super.onDestroy();
        }

        @Override
        public void superOnSaveInstanceState(IObjectWrapper outState) {
            RemoteFragmentClient.super.onSaveInstanceState(ObjectWrapper.unwrap(outState,
                    Bundle.class));
        }

        @Override
        public IObjectWrapper getActivity() {
            return ObjectWrapper.wrap(RemoteFragmentClient.this.getActivity());
        }
    };

    private IRemoteFragment mRemoteFragment;

    // TODO(pshmakov): how do we deal with FragmentManager restoring this Fragment on its own?
    /* package */ void setRemoteFragment(IRemoteFragment remoteFragment) {
        mRemoteFragment = remoteFragment;
    }

    /* package */ IRemoteFragmentClient asIRemoteFragmentClient() {
        return mClientImpl;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        try {
            return ObjectWrapper.unwrap(mRemoteFragment.handleOnCreateView(), View.class);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onAttach(Context context) {
        try {
            mRemoteFragment.handleOnAttach(ObjectWrapper.wrap(context));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        try {
            mRemoteFragment.handleOnCreate(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        try {
            mRemoteFragment.handleOnActivityCreated(ObjectWrapper.wrap(savedInstanceState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStart() {
        try {
            mRemoteFragment.handleOnStart();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onResume() {
        try {
            mRemoteFragment.handleOnResume();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onSaveInstanceState(Bundle outState) {
        try {
            mRemoteFragment.handleOnSaveInstanceState(ObjectWrapper.wrap(outState));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onPause() {
        try {
            mRemoteFragment.handleOnPause();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onStop() {
        try {
            mRemoteFragment.handleOnStop();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroyView() {
        try {
            mRemoteFragment.handleOnDestroyView();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDestroy() {
        try {
            mRemoteFragment.handleOnDestroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @SuppressWarnings("MissingSuperCall")
    @Override
    public void onDetach() {
        try {
            mRemoteFragment.handleOnDetach();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }
}
