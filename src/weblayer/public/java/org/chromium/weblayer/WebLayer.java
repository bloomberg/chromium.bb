// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.support.v4.app.Fragment;
import android.util.AndroidRuntimeException;
import android.util.Log;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.BrowserFragmentArgs;
import org.chromium.weblayer_private.interfaces.IBrowserFragment;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.IRemoteFragmentClient;
import org.chromium.weblayer_private.interfaces.IWebLayer;
import org.chromium.weblayer_private.interfaces.IWebLayerFactory;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

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
    private static ClassLoader sRemoteClassLoader;

    @Nullable
    private static WebLayerLoader sLoader;

    @NonNull
    private final IWebLayer mImpl;

    /**
     * Returns true if WebLayer is available. This tries to load WebLayer, but does no
     * initialization. This function may be called by code that uses WebView.
     * <p>
     * NOTE: it's possible for this to return true, yet loading to still fail. This happens if there
     * is an error during loading.
     *
     * @return true Returns true if WebLayer is available.
     */
    public static boolean isAvailable(Context context) {
        ThreadCheck.ensureOnUiThread();
        context = context.getApplicationContext();
        return getWebLayerLoader(context).isAvailable();
    }

    private static void checkAvailable(Context context) {
        if (!isAvailable(context)) {
            throw new UnsupportedVersionException(sLoader.getVersion());
        }
    }

    /**
     * Asynchronously creates and initializes WebLayer. Calling this more than once returns the same
     * object. Both this method and {@link #loadSync} yield the same instance of {@link WebLayer}.
     * <p>
     * {@link callback} is supplied null if unable to load WebLayer. In general, the only time null
     * is supplied is if there is an unexpected error.
     *
     * @param appContext The hosting application's Context.
     * @param callback {@link Callback} which will receive the WebLayer instance.
     * @throws UnsupportedVersionException If {@link #isAvailable} returns false. See
     * {@link #isAvailable} for details.
     */
    public static void loadAsync(@NonNull Context appContext, @NonNull Callback<WebLayer> callback)
            throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        checkAvailable(appContext);
        appContext = appContext.getApplicationContext();
        getWebLayerLoader(appContext).loadAsync(appContext, callback);
    }

    /**
     * Synchronously creates and initializes WebLayer.
     * Both this method and {@link #loadAsync} yield the same instance of {@link WebLayer}.
     * It is safe to call this method after {@link #loadAsync} to block until the ongoing load
     * finishes (or immediately return its result if already finished).
     * <p>
     * This returns null if unable to load WebLayer. In general, the only time null
     * is returns is if there is an unexpected error loading.
     *
     * @param appContext The hosting application's Context.
     * @return a {@link WebLayer} instance, or null if unable to load WebLayer.
     *
     * @throws UnsupportedVersionException If {@link #isAvailable} returns false. See
     * {@link #isAvailable} for details.
     */
    @Nullable
    public static WebLayer loadSync(@NonNull Context appContext)
            throws UnsupportedVersionException {
        ThreadCheck.ensureOnUiThread();
        appContext = appContext.getApplicationContext();
        checkAvailable(appContext);
        return getWebLayerLoader(appContext).loadSync(appContext);
    }

    private static WebLayerLoader getWebLayerLoader(Context appContext) {
        if (sLoader == null) sLoader = new WebLayerLoader(appContext);
        return sLoader;
    }

    /**
     * Returns the supported version. Using any functions defined in a newer version than
     * returned by {@link getSupportedMajorVersion} result in throwing an
     * UnsupportedOperationException.
     * <p> For example, consider the function {@link setBottomBar}, and further assume
     * {@link setBottomBar} was added in version 11. If {@link getSupportedMajorVersion}
     * returns 10, then calling {@link setBottomBar} returns in an UnsupportedOperationException.
     * OTOH, if {@link getSupportedMajorVersion} returns 12, then {@link setBottomBar} works as
     * expected
     *
     * @return the supported version, or -1 if WebLayer is not available.
     */
    public static int getSupportedMajorVersion(Context context) {
        ThreadCheck.ensureOnUiThread();
        context = context.getApplicationContext();
        return getWebLayerLoader(context).getMajorVersion();
    }

    /**
     * Encapsulates the state of WebLayer loading and initialization.
     */
    private static final class WebLayerLoader {
        @NonNull
        private final List<Callback<WebLayer>> mCallbacks = new ArrayList<>();
        @Nullable
        private IWebLayerFactory mFactory;
        @Nullable
        private IWebLayer mIWebLayer;
        @Nullable
        private WebLayer mWebLayer;
        // True if WebLayer is available and compatible with this client.
        private final boolean mAvailable;
        private final int mMajorVersion;
        private final String mVersion;
        private boolean mIsLoadingAsync;

        /**
         * Creates WebLayerLoader. This does a minimal amount of loading
         */
        public WebLayerLoader(@NonNull Context appContext) {
            ClassLoader remoteClassLoader;
            boolean available = false;
            int majorVersion = -1;
            String version = "<unavailable>";
            try {
                remoteClassLoader = getOrCreateRemoteClassLoader(appContext);
                Class factoryClass = remoteClassLoader.loadClass(
                        "org.chromium.weblayer_private.WebLayerFactoryImpl");
                // NOTE: the 20 comes from the previous scheme of incrementing versioning. It must
                // remain at 20 for Chrome version 79.
                // TODO(https://crbug.com/1031830): change 20 to -1 when tip of tree is at 83.
                mFactory = IWebLayerFactory.Stub.asInterface(
                        (IBinder) factoryClass
                                .getMethod("create", String.class, int.class, int.class)
                                .invoke(null, WebLayerClientVersionConstants.PRODUCT_VERSION,
                                        WebLayerClientVersionConstants.PRODUCT_MAJOR_VERSION, 20));
                available = mFactory.isClientSupported();
                majorVersion = mFactory.getImplementationMajorVersion();
                version = mFactory.getImplementationVersion();
            } catch (PackageManager.NameNotFoundException | ReflectiveOperationException
                    | RemoteException e) {
                Log.e("WebLayer", "Unable to create WebLayerFactory", e);
            }
            mAvailable = available;
            mMajorVersion = majorVersion;
            mVersion = version;
        }

        public boolean isAvailable() {
            return mAvailable;
        }

        public int getMajorVersion() {
            return mMajorVersion;
        }

        public String getVersion() {
            return mVersion;
        }

        public void loadAsync(@NonNull Context appContext, @NonNull Callback<WebLayer> callback) {
            if (mWebLayer != null) {
                callback.onResult(mWebLayer);
                return;
            }
            mCallbacks.add(callback);
            if (mIsLoadingAsync) {
                return; // Already loading.
            }
            mIsLoadingAsync = true;
            if (getIWebLayer(appContext) == null) {
                // Unable to create WebLayer. This generally shouldn't happen.
                onWebLayerReady();
                return;
            }
            try {
                getIWebLayer(appContext)
                        .loadAsync(ObjectWrapper.wrap(appContext),
                                ObjectWrapper.wrap((ValueCallback<Boolean>) result -> {
                                    onWebLayerReady();
                                }));
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        public WebLayer loadSync(@NonNull Context appContext) {
            if (mWebLayer != null) {
                return mWebLayer;
            }
            if (getIWebLayer(appContext) == null) {
                // Error in creating WebLayer. This generally shouldn't happen.
                onWebLayerReady();
                return null;
            }
            try {
                getIWebLayer(appContext).loadSync(ObjectWrapper.wrap(appContext));
                onWebLayerReady();
                return mWebLayer;
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }

        @Nullable
        private IWebLayer getIWebLayer(@NonNull Context appContext) {
            if (mIWebLayer != null) return mIWebLayer;
            if (!mAvailable) return null;
            try {
                mIWebLayer = mFactory.createWebLayer();
            } catch (RemoteException e) {
                // If |mAvailable| returns true, then create() should always succeed.
                throw new AndroidRuntimeException(e);
            }
            return mIWebLayer;
        }

        private void onWebLayerReady() {
            if (mWebLayer != null) {
                return;
            }
            if (mIWebLayer != null) mWebLayer = new WebLayer(mIWebLayer);
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
     * Get or create the profile for profileName.
     * @param profileName Null to indicate in-memory profile. Otherwise, name cannot be empty
     * and should contain only alphanumeric and underscore characters since it will be used as
     * a directory name in the file system.
     */
    @NonNull
    public Profile getProfile(@Nullable String profileName) {
        ThreadCheck.ensureOnUiThread();
        IProfile iprofile;
        try {
            iprofile = mImpl.getProfile(sanitizeProfileName(profileName));
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

    /**
     * Create a new WebLayer Fragment.
     * @param profileName Null to indicate in-memory profile. Otherwise, name cannot be empty
     * and should contain only alphanumeric and underscore characters since it will be used as
     * a directory name in the file system.
     */
    @NonNull
    public static Fragment createBrowserFragment(@Nullable String profileName) {
        ThreadCheck.ensureOnUiThread();
        // TODO: use a profile id instead of the path to the actual file.
        Bundle args = new Bundle();
        args.putString(BrowserFragmentArgs.PROFILE_NAME, sanitizeProfileName(profileName));
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

    /* package */ static IWebLayer getIWebLayer(Context appContext) {
        return getWebLayerLoader(appContext).getIWebLayer(appContext);
    }

    @SuppressWarnings("NewApi")
    static ClassLoader getOrCreateRemoteClassLoaderForChildProcess(Context appContext)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        if (sRemoteClassLoader != null) {
            return sRemoteClassLoader;
        }
        if (getImplPackageName(appContext) == null && Process.isIsolated()
                && Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            // In <= M, the WebView update service is not available in isolated processes. This
            // causes a crash when trying to initialize WebView through the normal machinery, so we
            // need to directly make the remote context here.
            String packageName = (String) Class.forName("android.webkit.WebViewFactory")
                                         .getMethod("getWebViewPackageName")
                                         .invoke(null);
            sRemoteClassLoader =
                    appContext
                            .createPackageContext(packageName,
                                    Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE)
                            .getClassLoader();
            return sRemoteClassLoader;
        }
        return getOrCreateRemoteClassLoader(appContext);
    }

    /**
     * Creates a ClassLoader for the remote (weblayer implementation) side.
     */
    static ClassLoader getOrCreateRemoteClassLoader(Context appContext)
            throws PackageManager.NameNotFoundException, ReflectiveOperationException {
        if (sRemoteClassLoader != null) {
            return sRemoteClassLoader;
        }
        String implPackageName = getImplPackageName(appContext);
        if (implPackageName == null) {
            sRemoteClassLoader = createRemoteClassLoaderFromWebViewFactory(appContext);
        } else {
            sRemoteClassLoader = createRemoteClassLoaderFromPackage(appContext, implPackageName);
        }
        return sRemoteClassLoader;
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

    private static String sanitizeProfileName(String profileName) {
        if ("".equals(profileName)) {
            throw new AndroidRuntimeException("Profile path cannot be empty");
        }
        return profileName == null ? "" : profileName;
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
