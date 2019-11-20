// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.support.v4.app.Fragment;
import android.util.AndroidRuntimeException;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.WebLayerVersion;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

/**
 * WebLayer is responsible for initializing state necessary to use any of the classes in web layer.
 */
public final class WebLayer {
    // This metadata key, if defined, overrides the default behaviour of loading WebLayer from the
    // current WebView implementation. This is only intended for testing, and does not enforce any
    // signature requirements on the implementation, nor does it use the production code path to
    // load the code. Do not set this in production APKs!
    private static final String PACKAGE_MANIFEST_KEY = "org.chromium.weblayer.WebLayerPackage";

    @Nullable
    private static WebLayerLoader sLoader;

    @NonNull
    private final IWebLayer mImpl;

    /**
     * Loads the WebLayer implementation and returns the IWebLayer. This does *not* trigger the
     * implementation to start.
     */
    private static IWebLayer connectToWebLayerImplementation(@NonNull Context appContext)
            throws UnsupportedVersionException {
        // Just in case the app passed an Activity context.
        appContext = appContext.getApplicationContext();
        ClassLoader remoteClassLoader;
        try {
            remoteClassLoader = createRemoteClassLoader(appContext);
        } catch (Exception e) {
            throw new AndroidRuntimeException(e);
        }
        try {
            Class webLayerClass =
                    remoteClassLoader.loadClass("org.chromium.weblayer_private.WebLayerImpl");

            // Check version before doing anything else on the implementation side.
            if (!(boolean) webLayerClass.getMethod("checkVersion", Integer.TYPE)
                            .invoke(null, WebLayerVersion.sVersionNumber)) {
                throw new UnsupportedVersionException(WebLayerVersion.sVersionNumber);
            }

            return IWebLayer.Stub.asInterface(
                    (IBinder) webLayerClass.getMethod("create").invoke(null));
        } catch (UnsupportedVersionException e) {
            throw e;
        } catch (Exception e) {
            throw new APICallException(e);
        }
    }

    /**
     * Asynchronously creates and initializes WebLayer. Calling this more than once returns the same
     * object. Both this method and {@link #loadSync} yield the same instance of {@link WebLayer}.
     *
     * @param appContext The hosting application's Context.
     * @param callback {@link Callback} which will receive the WebLayer instance.
     */
    public static void loadAsync(@NonNull Context appContext, @NonNull Callback<WebLayer> callback)
            throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        if (sLoader == null) sLoader = new WebLayerLoader();
        sLoader.loadAsync(appContext, callback);
    }

    /**
     * Synchronously creates and initializes WebLayer.
     * Both this method and {@link #loadAsync} yield the same instance of {@link WebLayer}.
     * It is safe to call this method after {@link #loadAsync} to block until the ongoing load
     * finishes (or immediately return its result if already finished).
     *
     * @param appContext The hosting application's Context.
     * @return a {@link WebLayer} instance.
     */
    @NonNull
    public static WebLayer loadSync(@NonNull Context appContext)
            throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        if (sLoader == null) sLoader = new WebLayerLoader();
        return sLoader.loadSync(appContext);
    }

    /**
     * Encapsulates the state of WebLayer loading and initialization.
     */
    private static final class WebLayerLoader {
        @NonNull
        private final List<Callback<WebLayer>> mCallbacks = new ArrayList<>();
        @Nullable
        private IWebLayer mIWebLayer;
        @Nullable
        private WebLayer mWebLayer;

        public void loadAsync(@NonNull Context appContext, @NonNull Callback<WebLayer> callback)
                throws UnsupportedVersionException {
            if (mWebLayer != null) {
                callback.onResult(mWebLayer);
                return;
            }
            mCallbacks.add(callback);
            if (mIWebLayer != null) {
                return; // Already loading.
            }
            try {
                getIWebLayer(appContext)
                        .loadAsync(ObjectWrapper.wrap(appContext),
                                ObjectWrapper.wrap((ValueCallback<Boolean>) result -> {
                                    // TODO: figure out when |result| is false and what to do in
                                    // such a scenario.
                                    assert result;
                                    onWebLayerReady();
                                }));
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        @NonNull
        public WebLayer loadSync(@NonNull Context appContext) throws UnsupportedVersionException {
            if (mWebLayer != null) {
                return mWebLayer;
            }
            try {
                getIWebLayer(appContext).loadSync(ObjectWrapper.wrap(appContext));
                onWebLayerReady();
                return mWebLayer;
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        @NonNull
        private IWebLayer getIWebLayer(@NonNull Context appContext)
                throws UnsupportedVersionException {
            if (mIWebLayer == null) {
                mIWebLayer = connectToWebLayerImplementation(appContext);
            }
            return mIWebLayer;
        }

        private void onWebLayerReady() {
            if (mWebLayer != null) {
                return;
            }
            assert mIWebLayer != null;
            mWebLayer = new WebLayer(mIWebLayer);
            for (Callback<WebLayer> callback : mCallbacks) {
                callback.onResult(mWebLayer);
            }
            mCallbacks.clear();
        }
    }

    private WebLayer(IWebLayer iWebLayer) {
        mImpl = iWebLayer;
    }

    /**
     * Get or create the profile for profilePath.
     */
    @NonNull
    public Profile getProfile(@Nullable String profilePath) {
        ThreadCheck.ensureOnUiThread();
        IProfile iprofile;
        try {
            iprofile = mImpl.getProfile(sanitizeProfilePath(profilePath));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        return Profile.of(iprofile);
    }

    /**
     * To enable or disable DevTools remote debugging.
     */
    public void setRemoteDebuggingEnabled(boolean enabled) {
        try {
            mImpl.setRemoteDebuggingEnabled(enabled);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * @return Whether or not DevTools remote debugging is enabled.
     */
    public boolean isRemoteDebuggingEnabled() {
        try {
            return mImpl.isRemoteDebuggingEnabled();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @NonNull
    public static Fragment createBrowserFragment(@Nullable String profilePath) {
        ThreadCheck.ensureOnUiThread();
        // TODO: use a profile id instead of the path to the actual file.
        Bundle args = new Bundle();
        args.putString(BrowserFragmentArgs.PROFILE_PATH, sanitizeProfilePath(profilePath));
        BrowserFragment fragment = new BrowserFragment();
        fragment.setArguments(args);
        return fragment;
    }

    /**
     * Returns remote counterpart for the BrowserFragment: an {@link IBrowserFragment}.
     */
    /* package */ IBrowserFragment connectFragment(
            IRemoteFragmentClient remoteFragmentClient, Bundle fragmentArgs) {
        try {
            return mImpl.createBrowserFragmentImpl(
                    remoteFragmentClient, ObjectWrapper.wrap(fragmentArgs));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Creates a ClassLoader for the remote (weblayer implementation) side.
     */
    static ClassLoader createRemoteClassLoader(Context appContext)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        String implPackageName = getImplPackageName(appContext);
        if (implPackageName == null) {
            return createRemoteClassLoaderFromWebViewFactory(appContext);
        } else {
            return createRemoteClassLoaderFromPackage(appContext, implPackageName);
        }
    }

    /**
     * Creates a ClassLoader for the remote (weblayer implementation) side
     * using a specified package name as the implementation. This is only
     * intended for testing, not production use.
     */
    private static ClassLoader createRemoteClassLoaderFromPackage(
            Context appContext, String implPackageName)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        // Load the code for the target package.
        Context remoteContext = appContext.createPackageContext(
                implPackageName, Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);

        // Get the package info for the target package.
        PackageInfo implPackageInfo = appContext.getPackageManager().getPackageInfo(implPackageName,
                PackageManager.GET_SHARED_LIBRARY_FILES | PackageManager.GET_META_DATA);

        // Store this package info in WebViewFactory as if it had been loaded as WebView,
        // because other parts of the implementation need to be able to fetch it from there.
        Class<?> webViewFactory = Class.forName("android.webkit.WebViewFactory");
        Field sPackageInfo = webViewFactory.getDeclaredField("sPackageInfo");
        sPackageInfo.setAccessible(true);
        sPackageInfo.set(null, implPackageInfo);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            // Load assets using the WebViewDelegate.
            Class<?> webViewDelegateClass = Class.forName("android.webkit.WebViewDelegate");
            Constructor constructor = webViewDelegateClass.getDeclaredConstructor();
            constructor.setAccessible(true);
            Method addWebViewAssetPath =
                    webViewDelegateClass.getDeclaredMethod("addWebViewAssetPath", Context.class);
            Object delegate = constructor.newInstance();
            addWebViewAssetPath.invoke(delegate, appContext);
        } else {
            // In L WebViewDelegate did not yet exist, so we have to poke AssetManager directly.
            // Note: like the implementation in WebView's Api21CompatibilityDelegate this does
            // not support split APKs.
            Method addAssetPath = AssetManager.class.getMethod("addAssetPath", String.class);
            addAssetPath.invoke(appContext.getResources().getAssets(),
                    implPackageInfo.applicationInfo.sourceDir);
        }

        return remoteContext.getClassLoader();
    }

    /**
     * Creates a ClassLoader for the remote (weblayer implementation) side
     * using WebViewFactory to load the current WebView implementation.
     */
    private static ClassLoader createRemoteClassLoaderFromWebViewFactory(Context appContext)
            throws ReflectiveOperationException {
        Class<?> webViewFactory = Class.forName("android.webkit.WebViewFactory");
        Class<?> providerClass;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // In M+ this method loads the native library and the Java code, and adds the assets
            // to the app.
            Method getProviderClass = webViewFactory.getDeclaredMethod("getProviderClass");
            getProviderClass.setAccessible(true);
            providerClass = (Class) getProviderClass.invoke(null);
        } else {
            // In L we have to load the native library separately first.
            Method loadNativeLibrary = webViewFactory.getDeclaredMethod("loadNativeLibrary");
            loadNativeLibrary.setAccessible(true);
            loadNativeLibrary.invoke(null);
            // In L the method had a different name but still adds the assets to the app.
            Method getFactoryClass = webViewFactory.getDeclaredMethod("getFactoryClass");
            getFactoryClass.setAccessible(true);
            providerClass = (Class) getFactoryClass.invoke(null);
        }
        return providerClass.getClassLoader();
    }

    private static String sanitizeProfilePath(String profilePath) {
        if ("".equals(profilePath)) {
            throw new AndroidRuntimeException("Profile path cannot be empty");
        }
        return profilePath == null ? "" : profilePath;
    }

    private static String getImplPackageName(Context appContext)
            throws PackageManager.NameNotFoundException {
        Bundle metaData = appContext.getPackageManager()
                                  .getApplicationInfo(
                                          appContext.getPackageName(), PackageManager.GET_META_DATA)
                                  .metaData;
        if (metaData != null) return metaData.getString(PACKAGE_MANIFEST_KEY);
        return null;
    }
}
