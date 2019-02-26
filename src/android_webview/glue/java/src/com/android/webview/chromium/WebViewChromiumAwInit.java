// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.Manifest;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.TokenBindingService;
import android.webkit.WebStorage;
import android.webkit.WebViewDatabase;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwNetworkChangeNotifierRegistrationPolicy;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwResource;
import org.chromium.android_webview.AwServiceWorkerController;
import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.android_webview.ScopedSysTraceEvent;
import org.chromium.android_webview.VariationsSeedLoader;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.FieldTrialList;
import org.chromium.base.PathService;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.DoNotInline;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.net.NetworkChangeNotifier;

/**
 * Class controlling the Chromium initialization for WebView.
 * We hold on to most static objects used by WebView here.
 * This class is shared between the webkit glue layer and the support library glue layer.
 */
public class WebViewChromiumAwInit {
    private static final String TAG = "WebViewChromiumAwInit";

    private static final String HTTP_AUTH_DATABASE_FILE = "http_auth.db";

    /**
     * This holds objects of classes that are defined in N and above to ensure that run-time class
     * verification does not occur until it is actually used for N and above.
     */
    @TargetApi(Build.VERSION_CODES.N)
    @DoNotInline
    private static class ObjectHolderForN {
        public TokenBindingService mTokenBindingService;
    }

    // TODO(gsennton): store aw-objects instead of adapters here
    // Initialization guarded by mLock.
    private AwBrowserContext mBrowserContext;
    private SharedStatics mSharedStatics;
    private GeolocationPermissionsAdapter mGeolocationPermissions;
    private CookieManagerAdapter mCookieManager;

    @TargetApi(Build.VERSION_CODES.N)
    private ObjectHolderForN mObjectHolderForN =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N ? new ObjectHolderForN() : null;

    private WebIconDatabaseAdapter mWebIconDatabase;
    private WebStorageAdapter mWebStorage;
    private WebViewDatabaseAdapter mWebViewDatabase;
    private AwServiceWorkerController mServiceWorkerController;
    private AwTracingController mAwTracingController;
    private VariationsSeedLoader mSeedLoader;
    private Thread mSetUpResourcesThread;

    // Guards accees to the other members, and is notifyAll() signalled on the UI thread
    // when the chromium process has been started.
    // This member is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ final Object mLock = new Object();

    // Read/write protected by mLock.
    private boolean mStarted;

    private final WebViewChromiumFactoryProvider mFactory;

    WebViewChromiumAwInit(WebViewChromiumFactoryProvider factory) {
        mFactory = factory;
        // Do not make calls into 'factory' in this ctor - this ctor is called from the
        // WebViewChromiumFactoryProvider ctor, so 'factory' is not properly initialized yet.
    }

    public AwTracingController getAwTracingController() {
        synchronized (mLock) {
            if (mAwTracingController == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mAwTracingController;
    }

    // TODO: DIR_RESOURCE_PAKS_ANDROID needs to live somewhere sensible,
    // inlined here for simplicity setting up the HTMLViewer demo. Unfortunately
    // it can't go into base.PathService, as the native constant it refers to
    // lives in the ui/ layer. See ui/base/ui_base_paths.h
    private static final int DIR_RESOURCE_PAKS_ANDROID = 3003;

    protected void startChromiumLocked() {
        try (ScopedSysTraceEvent event =
                        ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.startChromiumLocked")) {
            assert Thread.holdsLock(mLock) && ThreadUtils.runningOnUiThread();

            // The post-condition of this method is everything is ready, so notify now to cover all
            // return paths. (Other threads will not wake-up until we release |mLock|, whatever).
            mLock.notifyAll();

            if (mStarted) {
                return;
            }

            final Context context = ContextUtils.getApplicationContext();

            // We are rewriting Java resources in the background.
            // NOTE: Any reference to Java resources will cause a crash.

            try (ScopedSysTraceEvent e =
                            ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.LibraryLoader")) {
                LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_WEBVIEW);
            } catch (ProcessInitException e) {
                throw new RuntimeException("Error initializing WebView library", e);
            }

            PathService.override(PathService.DIR_MODULE, "/system/lib/");
            PathService.override(DIR_RESOURCE_PAKS_ANDROID, "/system/framework/webview/paks");

            initPlatSupportLibrary();
            doNetworkInitializations(context);

            waitUntilSetUpResources();

            // NOTE: Finished writing Java resources. From this point on, it's safe to use them.

            AwBrowserProcess.configureChildProcessLauncher();

            // finishVariationsInitLocked() must precede native initialization so the seed is
            // available when AwFeatureListCreator::SetUpFieldTrials() runs.
            finishVariationsInitLocked();

            AwBrowserProcess.start();
            AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(true /* updateMetricsConsent */);

            mSharedStatics = new SharedStatics();
            if (CommandLineUtil.isBuildDebuggable()) {
                mSharedStatics.setWebContentsDebuggingEnabledUnconditionally(true);
            }

            TraceEvent.setATraceEnabled(mFactory.getWebViewDelegate().isTraceTagEnabled());
            mFactory.getWebViewDelegate().setOnTraceEnabledChangeListener(
                    new WebViewDelegate.OnTraceEnabledChangeListener() {
                        @Override
                        public void onTraceEnabledChange(boolean enabled) {
                            TraceEvent.setATraceEnabled(enabled);
                        }
                    });

            mStarted = true;

            // Make sure to record any cached metrics, now that we know that the native
            // library has been loaded and initialized.
            CachedMetrics.commitCachedMetrics();

            RecordHistogram.recordSparseHistogram("Android.WebView.TargetSdkVersion",
                    context.getApplicationInfo().targetSdkVersion);

            try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                         "WebViewChromiumAwInit.initThreadUnsafeSingletons")) {
                // Initialize thread-unsafe singletons.
                AwBrowserContext awBrowserContext = getBrowserContextOnUiThread();
                mGeolocationPermissions = new GeolocationPermissionsAdapter(
                        mFactory, awBrowserContext.getGeolocationPermissions());
                mWebStorage = new WebStorageAdapter(mFactory, AwQuotaManagerBridge.getInstance());
                mAwTracingController = awBrowserContext.getTracingController();
                mServiceWorkerController = awBrowserContext.getServiceWorkerController();
            }

            mFactory.getRunQueue().drainQueue();

            maybeLogActiveTrials(context);
        }
    }

    /**
     * Set up resources on a background thread.
     * @param context The context.
     */
    public void setUpResourcesOnBackgroundThread(PackageInfo webViewPackageInfo, Context context) {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumAwInit.setUpResourcesOnBackgroundThread")) {
            assert mSetUpResourcesThread == null : "This method shouldn't be called twice.";

            // Make sure that ResourceProvider is initialized before starting the browser process.
            mSetUpResourcesThread = new Thread(new Runnable() {
                @Override
                public void run() {
                    // Run this in parallel as it takes some time.
                    setUpResources(webViewPackageInfo, context);
                }
            });
            mSetUpResourcesThread.start();
        }
    }

    private void waitUntilSetUpResources() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumAwInit.waitUntilSetUpResources")) {
            mSetUpResourcesThread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    private void setUpResources(PackageInfo webViewPackageInfo, Context context) {
        try (ScopedSysTraceEvent e =
                        ScopedSysTraceEvent.scoped("WebViewChromiumAwInit.setUpResources")) {
            String packageName = webViewPackageInfo.packageName;
            if (webViewPackageInfo.applicationInfo.metaData != null) {
                packageName = webViewPackageInfo.applicationInfo.metaData.getString(
                        "com.android.webview.WebViewDonorPackage", packageName);
            }
            ResourceRewriter.rewriteRValues(mFactory.getWebViewDelegate().getPackageId(
                    context.getResources(), packageName));

            AwResource.setResources(context.getResources());
            AwResource.setConfigKeySystemUuidMapping(android.R.array.config_keySystemUuidMapping);
        }
    }

    boolean hasStarted() {
        return mStarted;
    }

    void startYourEngines(boolean onMainThread) {
        synchronized (mLock) {
            ensureChromiumStartedLocked(onMainThread);
        }
    }

    // This method is not private only because the downstream subclass needs to access it,
    // it shouldn't be accessed from anywhere else.
    /* package */ void ensureChromiumStartedLocked(boolean onMainThread) {
        assert Thread.holdsLock(mLock);

        if (mStarted) { // Early-out for the common case.
            return;
        }

        Looper looper = !onMainThread ? Looper.myLooper() : Looper.getMainLooper();
        Log.v(TAG, "Binding Chromium to "
                        + (Looper.getMainLooper().equals(looper) ? "main" : "background")
                        + " looper " + looper);
        ThreadUtils.setUiThread(looper);

        if (ThreadUtils.runningOnUiThread()) {
            startChromiumLocked();
            return;
        }

        // We must post to the UI thread to cover the case that the user has invoked Chromium
        // startup by using the (thread-safe) CookieManager rather than creating a WebView.
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                synchronized (mLock) {
                    startChromiumLocked();
                }
            }
        });
        while (!mStarted) {
            try {
                // Important: wait() releases |mLock| the UI thread can take it :-)
                mLock.wait();
            } catch (InterruptedException e) {
                // Keep trying... eventually the UI thread will process the task we sent it.
            }
        }
    }

    private void initPlatSupportLibrary() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumAwInit.initPlatSupportLibrary")) {
            DrawGLFunctor.setChromiumAwDrawGLFunction(AwContents.getAwDrawGLFunction());
            AwContents.setAwDrawSWFunctionTable(GraphicsUtils.getDrawSWFunctionTable());
            AwContents.setAwDrawGLFunctionTable(GraphicsUtils.getDrawGLFunctionTable());
        }
    }

    private void doNetworkInitializations(Context applicationContext) {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumAwInit.doNetworkInitializations")) {
            if (applicationContext.checkPermission(
                        Manifest.permission.ACCESS_NETWORK_STATE, Process.myPid(), Process.myUid())
                    == PackageManager.PERMISSION_GRANTED) {
                NetworkChangeNotifier.init();
                NetworkChangeNotifier.setAutoDetectConnectivityState(
                        new AwNetworkChangeNotifierRegistrationPolicy());
            }

            AwContentsStatics.setCheckClearTextPermitted(
                    applicationContext.getApplicationInfo().targetSdkVersion
                    >= Build.VERSION_CODES.O);
        }
    }

    // Only on UI thread.
    AwBrowserContext getBrowserContextOnUiThread() {
        assert mStarted;

        if (BuildConfig.DCHECK_IS_ON && !ThreadUtils.runningOnUiThread()) {
            throw new RuntimeException(
                    "getBrowserContextOnUiThread called on " + Thread.currentThread());
        }

        if (mBrowserContext == null) {
            mBrowserContext = new AwBrowserContext(
                mFactory.getWebViewPrefs(), ContextUtils.getApplicationContext());
        }
        return mBrowserContext;
    }

    /**
     * Returns the lock used for guarding chromium initialization.
     * We make this public to let higher-level classes use this lock to guard variables
     * dependent on this class, to avoid introducing new locks (which can cause deadlocks).
     */
    public Object getLock() {
        return mLock;
    }

    public SharedStatics getStatics() {
        synchronized (mLock) {
            if (mSharedStatics == null) {
                // TODO: Optimization potential: most these methods only need the native library
                // loaded and initialized, not the entire browser process started.
                // See also http://b/7009882
                ensureChromiumStartedLocked(true);
            }
        }
        return mSharedStatics;
    }

    public GeolocationPermissions getGeolocationPermissions() {
        synchronized (mLock) {
            if (mGeolocationPermissions == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mGeolocationPermissions;
    }

    public CookieManager getCookieManager() {
        synchronized (mLock) {
            if (mCookieManager == null) {
                mCookieManager = new CookieManagerAdapter(new AwCookieManager());
            }
        }
        return mCookieManager;
    }

    public AwServiceWorkerController getServiceWorkerController() {
        synchronized (mLock) {
            if (mServiceWorkerController == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mServiceWorkerController;
    }

    @TargetApi(Build.VERSION_CODES.N)
    public TokenBindingService getTokenBindingService() {
        synchronized (mLock) {
            if (mObjectHolderForN.mTokenBindingService == null) {
                mObjectHolderForN.mTokenBindingService =
                        GlueApiHelperForN.createTokenBindingManagerAdapter(mFactory);
            }
        }
        return mObjectHolderForN.mTokenBindingService;
    }

    public android.webkit.WebIconDatabase getWebIconDatabase() {
        synchronized (mLock) {
            ensureChromiumStartedLocked(true);
            if (mWebIconDatabase == null) {
                mWebIconDatabase = new WebIconDatabaseAdapter();
            }
        }
        return mWebIconDatabase;
    }

    public WebStorage getWebStorage() {
        synchronized (mLock) {
            if (mWebStorage == null) {
                ensureChromiumStartedLocked(true);
            }
        }
        return mWebStorage;
    }

    public WebViewDatabase getWebViewDatabase(final Context context) {
        synchronized (mLock) {
            ensureChromiumStartedLocked(true);
            if (mWebViewDatabase == null) {
                mWebViewDatabase = new WebViewDatabaseAdapter(
                        mFactory, HttpAuthDatabase.newInstance(context, HTTP_AUTH_DATABASE_FILE));
            }
        }
        return mWebViewDatabase;
    }

    // See comments in VariationsSeedLoader.java on when it's safe to call this.
    public void startVariationsInit() {
        synchronized (mLock) {
            if (mSeedLoader == null) {
                mSeedLoader = new VariationsSeedLoader();
                mSeedLoader.startVariationsInit();
            }
        }
    }

    private void finishVariationsInitLocked() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumAwInit.finishVariationsInitLocked")) {
            assert Thread.holdsLock(mLock);
            if (mSeedLoader == null) {
                Log.e(TAG, "finishVariationsInitLocked() called before startVariationsInit()");
                startVariationsInit();
            }
            mSeedLoader.finishVariationsInit();
            mSeedLoader = null; // Allow this to be GC'd after its background thread finishes.
        }
    }

    // If a certain app is installed, log field trials as they become active, for debugging
    // purposes. Check for the app asyncronously because PackageManager is slow.
    private static void maybeLogActiveTrials(final Context ctx) {
        AsyncTask.THREAD_POOL_EXECUTOR.execute(() -> {
            try {
                // This must match the package name in:
                // android_webview/tools/webview_log_verbosifier/AndroidManifest.xml
                ctx.getPackageManager().getPackageInfo(
                        "org.chromium.webview_log_verbosifier", /*flags=*/0);
            } catch (PackageManager.NameNotFoundException e) {
                return;
            }

            ThreadUtils.postOnUiThread(() -> FieldTrialList.logActiveTrials());
        });
    }

    public WebViewChromiumRunQueue getRunQueue() {
        return mFactory.getRunQueue();
    }
}
