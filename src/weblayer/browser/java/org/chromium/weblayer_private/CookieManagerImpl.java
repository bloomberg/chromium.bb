// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.CookieChangeCause;
import org.chromium.weblayer_private.interfaces.ICookieChangedCallbackClient;
import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.lang.ref.WeakReference;

/**
 * Implementation of ICookieManager.
 */
@JNINamespace("weblayer")
public final class CookieManagerImpl extends ICookieManager.Stub {
    private long mNativeCookieManager;

    CookieManagerImpl(long nativeCookieManager) {
        mNativeCookieManager = nativeCookieManager;
    }

    public void destroy() {
        mNativeCookieManager = 0;
    }

    @Override
    public boolean setCookie(String url, String value, IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        ValueCallback<Boolean> valueCallback =
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<Boolean> baseCallback = (Boolean result) -> valueCallback.onReceiveValue(result);
        return CookieManagerImplJni.get().setCookie(mNativeCookieManager, url, value, baseCallback);
    }

    @Override
    public void getCookie(String url, IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        ValueCallback<String> valueCallback =
                (ValueCallback<String>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<String> baseCallback = (String result) -> valueCallback.onReceiveValue(result);
        CookieManagerImplJni.get().getCookie(mNativeCookieManager, url, baseCallback);
    }

    @Override
    public IObjectWrapper addCookieChangedCallback(
            String url, String name, ICookieChangedCallbackClient callback) {
        StrictModeWorkaround.apply();
        int id = CookieManagerImplJni.get().addCookieChangedCallback(
                mNativeCookieManager, url, name, callback);
        // Use a weak reference to make sure we don't keep |this| alive in the closure.
        WeakReference<CookieManagerImpl> weakSelf = new WeakReference<>(this);
        Runnable close = () -> {
            CookieManagerImpl impl = weakSelf.get();
            if (impl != null && impl.mNativeCookieManager != 0) {
                CookieManagerImplJni.get().removeCookieChangedCallback(
                        impl.mNativeCookieManager, id);
            }
        };
        return ObjectWrapper.wrap(close);
    }

    @CalledByNative
    private static void onCookieChange(
            ICookieChangedCallbackClient callback, String cookie, int cause) {
        try {
            callback.onCookieChanged(cookie, mojoCauseToJavaType(cause));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CookieChangeCause
    private static int mojoCauseToJavaType(int cause) {
        assert org.chromium.network.mojom.CookieChangeCause.isKnownValue(cause);
        switch (cause) {
            case org.chromium.network.mojom.CookieChangeCause.INSERTED:
                return CookieChangeCause.INSERTED;
            case org.chromium.network.mojom.CookieChangeCause.EXPLICIT:
                return CookieChangeCause.EXPLICIT;
            case org.chromium.network.mojom.CookieChangeCause.UNKNOWN_DELETION:
                return CookieChangeCause.UNKNOWN_DELETION;
            case org.chromium.network.mojom.CookieChangeCause.OVERWRITE:
                return CookieChangeCause.OVERWRITE;
            case org.chromium.network.mojom.CookieChangeCause.EXPIRED:
                return CookieChangeCause.EXPIRED;
            case org.chromium.network.mojom.CookieChangeCause.EVICTED:
                return CookieChangeCause.EVICTED;
            case org.chromium.network.mojom.CookieChangeCause.EXPIRED_OVERWRITE:
                return CookieChangeCause.EXPIRED_OVERWRITE;
        }
        assert false;
        return CookieChangeCause.EXPLICIT;
    }

    @NativeMethods
    interface Natives {
        boolean setCookie(
                long nativeCookieManagerImpl, String url, String value, Callback<Boolean> callback);
        void getCookie(long nativeCookieManagerImpl, String url, Callback<String> callback);
        int addCookieChangedCallback(long nativeCookieManagerImpl, String url, String name,
                ICookieChangedCallbackClient callback);
        void removeCookieChangedCallback(long nativeCookieManagerImpl, int id);
    }
}
