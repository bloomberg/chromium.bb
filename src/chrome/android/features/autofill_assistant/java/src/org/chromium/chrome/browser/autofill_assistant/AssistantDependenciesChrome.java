// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

/**
 * Implementation of {@link AssistantDependencies} for Chrome.
 */
public class AssistantDependenciesChrome
        implements AssistantDependencies, AssistantStaticDependenciesChrome {
    private final WebContents mWebContents;

    // Dependencies tied to the activity.
    private Context mContext;
    private BottomSheetController mBottomSheetController;
    private BrowserControlsStateProvider mBrowserControls;
    private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private ApplicationViewportInsetSupplier mBottomInsetProvider;
    private ActivityTabProvider mActivityTabProvider;
    private View mRootView;
    private AssistantSnackbarFactory mSnackbarFactory;

    public AssistantDependenciesChrome(@NonNull WebContents webContents) {
        mWebContents = webContents;
        onActivityAttachmentChanged();
    }

    public boolean onActivityAttachmentChanged() {
        Activity activity = ActivityUtils.getActivityFromWebContents(mWebContents);
        if (!(activity instanceof ChromeActivity)) return false;
        ChromeActivity chromeActivity = (ChromeActivity) activity;

        Supplier<View> rootView = chromeActivity.getCompositorViewHolderSupplier();

        mContext = chromeActivity;
        mBottomSheetController =
                BottomSheetControllerProvider.from(chromeActivity.getWindowAndroid());
        mBrowserControls = chromeActivity.getBrowserControlsManager();
        mKeyboardVisibilityDelegate = chromeActivity.getWindowAndroid().getKeyboardDelegate();
        mBottomInsetProvider =
                chromeActivity.getWindowAndroid().getApplicationBottomInsetProvider();
        mActivityTabProvider = chromeActivity.getActivityTabProvider();
        mRootView = rootView.get();
        mSnackbarFactory =
                new AssistantSnackbarFactoryChrome(mContext, chromeActivity.getSnackbarManager());
        return true;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public BottomSheetController getBottomSheetController() {
        return mBottomSheetController;
    }

    @Override
    public BrowserControlsStateProvider getBrowserControls() {
        return mBrowserControls;
    }

    @Override
    public KeyboardVisibilityDelegate getKeyboardVisibilityDelegate() {
        return mKeyboardVisibilityDelegate;
    }

    @Override
    public ApplicationViewportInsetSupplier getBottomInsetProvider() {
        return mBottomInsetProvider;
    }

    @Override
    public ActivityTabProvider getActivityTabProvider() {
        return mActivityTabProvider;
    }

    @Override
    public View getRootView() {
        return mRootView;
    }

    @Override
    public AssistantSnackbarFactory getSnackbarFactory() {
        return mSnackbarFactory;
    }
}
