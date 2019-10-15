// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.webkit.ValueCallback;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.ui.base.ResourceBundle;
import org.chromium.weblayer_private.aidl.IBrowserFragment;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
import org.chromium.weblayer_private.aidl.IWebLayer;
import org.chromium.weblayer_private.aidl.ObjectWrapper;
import org.chromium.weblayer_private.aidl.WebLayerVersion;

@UsedByReflection("WebLayer")
public final class WebLayerImpl extends IWebLayer.Stub {
    // TODO: should there be one tag for all this code?
    private static final String TAG = "WebLayer";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "weblayer";
    // TODO: Configure this from the client.
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/weblayer-command-line";

    private final ProfileManager mProfileManager = new ProfileManager();

    @UsedByReflection("WebLayer")
    public static IBinder create() {
        return new WebLayerImpl();
    }

    private WebLayerImpl() {}

    /**
     * Returns true if the client and implementation versions are compatible.
     */
    @UsedByReflection("WebLayer")
    public static boolean checkVersion(int clientVersion) {
        return clientVersion == WebLayerVersion.sVersionNumber;
    }

    @Override
    public void initAndLoadAsync(IObjectWrapper webLayerContextWrapper,
            IObjectWrapper loadedCallbackWrapper, int resourcesPackageId) {
        // TODO: The call to onResourcesLoaded() can be slow, we may need to parallelize this with
        // other expensive startup tasks.
        R.onResourcesLoaded(resourcesPackageId);

        Context context = ObjectWrapper.unwrap(webLayerContextWrapper, Context.class);
        ContextUtils.initApplicationContext(context);
        ResourceBundle.setNoAvailableLocalePaks();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);

        ChildProcessCreationParams.set(context.getPackageName(), false /* isExternalService */,
                LibraryProcessType.PROCESS_WEBLAYER_CHILD, true /* bindToCaller */,
                false /* ignoreVisibilityForImportance */,
                "org.chromium.weblayer.ChildProcessService$Privileged",
                "org.chromium.weblayer.ChildProcessService$Sandboxed");

        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();

        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_WEBLAYER);

        final ValueCallback<Boolean> loadedCallback = (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                loadedCallbackWrapper, ValueCallback.class);
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesAsync(/* startGpu */ false,
                        /* startServiceManagerOnly */ false,
                        new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess() {
                                loadedCallback.onReceiveValue(true);
                            }
                            @Override
                            public void onFailure() {
                                loadedCallback.onReceiveValue(false);
                            }
                        });
    }

    @Override
    public void loadSync() {
        BrowserStartupController.get(LibraryProcessType.PROCESS_WEBLAYER)
                .startBrowserProcessesSync(
                        /* singleProcess*/ false);
    }

    @Override
    public IBrowserFragment createBrowserFragmentImpl(IRemoteFragmentClient fragmentClient,
            IObjectWrapper fragmentArgs) {
        Bundle unwrappedArgs = ObjectWrapper.unwrap(fragmentArgs, Bundle.class);
        BrowserFragmentImpl fragment = new BrowserFragmentImpl(mProfileManager, fragmentClient,
                unwrappedArgs);
        return fragment.asIBrowserFragment();
    }
}
