// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.Manifest.permission;
import android.content.pm.PackageManager;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Owns the c++ DownloadCallbackProxy class, which is responsible for forwarding all
 * DownloadDelegate delegate calls to this class, which in turn forwards to the
 * DownloadCallbackClient.
 */
@JNINamespace("weblayer")
public final class DownloadCallbackProxy {
    private long mNativeDownloadCallbackProxy;
    private String mProfileName;
    private IDownloadCallbackClient mClient;

    DownloadCallbackProxy(String profileName, long profile) {
        mProfileName = profileName;
        mNativeDownloadCallbackProxy =
                DownloadCallbackProxyJni.get().createDownloadCallbackProxy(this, profile);
    }

    public void setClient(IDownloadCallbackClient client) {
        mClient = client;
    }

    public void destroy() {
        DownloadCallbackProxyJni.get().deleteDownloadCallbackProxy(mNativeDownloadCallbackProxy);
        mNativeDownloadCallbackProxy = 0;
    }

    @CalledByNative
    private boolean interceptDownload(String url, String userAgent, String contentDisposition,
            String mimetype, long contentLength) throws RemoteException {
        if (mClient == null) {
            return false;
        }

        return mClient.interceptDownload(
                url, userAgent, contentDisposition, mimetype, contentLength);
    }

    @CalledByNative
    private void allowDownload(TabImpl tab, String url, String requestMethod,
            String requestInitiator, long callbackId) throws RemoteException {
        WindowAndroid window = tab.getBrowser().getWindowAndroid();
        if (window.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            continueAllowDownload(url, requestMethod, requestInitiator, callbackId);
            return;
        }

        String[] requestPermissions = new String[] {permission.WRITE_EXTERNAL_STORAGE};
        window.requestPermissions(requestPermissions, (permissions, grantResults) -> {
            if (grantResults[0] == PackageManager.PERMISSION_DENIED) {
                DownloadCallbackProxyJni.get().allowDownload(callbackId, false);
                return;
            }

            try {
                continueAllowDownload(url, requestMethod, requestInitiator, callbackId);
            } catch (RemoteException e) {
            }
        });
    }

    private void continueAllowDownload(String url, String requestMethod, String requestInitiator,
            long callbackId) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 81) {
            DownloadCallbackProxyJni.get().allowDownload(callbackId, true);
            return;
        }

        if (mClient == null) {
            DownloadCallbackProxyJni.get().allowDownload(callbackId, true);
            return;
        }

        ValueCallback<Boolean> callback = new ValueCallback<Boolean>() {
            @Override
            public void onReceiveValue(Boolean result) {
                if (mNativeDownloadCallbackProxy == 0) {
                    throw new IllegalStateException("Called after destroy()");
                }
                DownloadCallbackProxyJni.get().allowDownload(callbackId, result);
            }
        };

        mClient.allowDownload(url, requestMethod, requestInitiator, ObjectWrapper.wrap(callback));
    }

    @CalledByNative
    private DownloadImpl createDownload(long nativeDownloadImpl, int id) {
        return new DownloadImpl(mProfileName, mClient, nativeDownloadImpl, id);
    }

    @CalledByNative
    private void downloadStarted(DownloadImpl download) throws RemoteException {
        if (mClient != null) {
            mClient.downloadStarted(download.getClientDownload());
        }
        download.downloadStarted();
    }

    @CalledByNative
    private void downloadProgressChanged(DownloadImpl download) throws RemoteException {
        if (mClient != null) {
            mClient.downloadProgressChanged(download.getClientDownload());
        }
        download.downloadProgressChanged();
    }

    @CalledByNative
    private void downloadCompleted(DownloadImpl download) throws RemoteException {
        if (mClient != null) {
            mClient.downloadCompleted(download.getClientDownload());
        }
        download.downloadCompleted();
    }

    @CalledByNative
    private void downloadFailed(DownloadImpl download) throws RemoteException {
        if (mClient != null) {
            mClient.downloadFailed(download.getClientDownload());
        }
        download.downloadFailed();
    }

    @NativeMethods
    interface Natives {
        long createDownloadCallbackProxy(DownloadCallbackProxy proxy, long tab);
        void deleteDownloadCallbackProxy(long proxy);
        void allowDownload(long callbackId, boolean allow);
    }
}
