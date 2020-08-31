// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.webapps.WebappExtras;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.util.ColorUtils;

import javax.inject.Inject;

/**
 * Helper that updates the Android task description given the state of the current tab.
 *
 * <p>
 * The task description is what is shown in Android's Overview/Recents screen for each entry.
 */
@ActivityScope
public class CustomTabTaskDescriptionHelper implements NativeInitObserver, Destroyable {
    private final ChromeActivity<?> mActivity;
    private final CustomTabActivityTabProvider mTabProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    @Nullable
    private CustomTabTaskDescriptionIconGenerator mIconGenerator;
    @Nullable
    private FaviconHelper mFaviconHelper;

    @Nullable
    private CustomTabTabObserver mTabObserver;
    @Nullable
    private CustomTabTabObserver mIconTabObserver;
    @Nullable
    private CustomTabActivityTabProvider.Observer mActivityTabObserver;

    private int mDefaultThemeColor;
    @Nullable
    private String mForceTitle;
    @Nullable
    private Bitmap mForceIcon;

    @Nullable
    private Bitmap mLargestFavicon;

    @Inject
    public CustomTabTaskDescriptionHelper(ChromeActivity<?> activity,
            CustomTabActivityTabProvider tabProvider, TabObserverRegistrar tabObserverRegistrar,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
        mIntentDataProvider = intentDataProvider;

        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();
        boolean canUpdate = (webappExtras != null || usesSeparateTask());
        if (!canUpdate) return;

        mDefaultThemeColor = ApiCompatibilityUtils.getColor(
                mActivity.getResources(), R.color.default_primary_color);
        if (webappExtras != null) {
            if (mIntentDataProvider.hasCustomToolbarColor()) {
                mDefaultThemeColor = mIntentDataProvider.getToolbarColor();
            }
            mForceIcon = webappExtras.icon.bitmap();
            mForceTitle = webappExtras.shortName;
        }

        mIconGenerator = new CustomTabTaskDescriptionIconGenerator(mActivity);
        mFaviconHelper = new FaviconHelper();

        startObserving();
    }

    private void startObserving() {
        mTabObserver = new CustomTabTabObserver() {
            @Override
            public void onUrlUpdated(Tab tab) {
                updateTaskDescription();
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTaskDescription();
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()
                        && !navigation.isSameDocument()) {
                    mLargestFavicon = null;
                    updateTaskDescription();
                }
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                updateTaskDescription();
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updateTaskDescription();
            }

            @Override
            public void onAttachedToInitialTab(@NonNull Tab tab) {
                onActiveTabChanged();
            }

            @Override
            public void onObservingDifferentTab(@NonNull Tab tab) {
                onActiveTabChanged();
            }
        };
        mTabObserverRegistrar.registerActivityTabObserver(mTabObserver);

        if (mForceIcon != null) {
            mIconTabObserver = new CustomTabTabObserver() {
                @Override
                public void onWebContentsSwapped(
                        Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                    if (!didStartLoad) return;
                    resetIcon();
                }

                @Override
                public void onFaviconUpdated(Tab tab, Bitmap icon) {
                    if (icon == null) return;
                    updateFavicon(icon);
                }

                @Override
                public void onSSLStateUpdated(Tab tab) {
                    if (hasSecurityWarningOrError(tab)) resetIcon();
                }

                @Override
                public void onDidAttachInterstitialPage(Tab tab) {
                    resetIcon();
                }

                @Override
                public void onDidDetachInterstitialPage(Tab tab) {
                    resetIcon();
                }

                private boolean hasSecurityWarningOrError(Tab tab) {
                    boolean isContentDangerous =
                            SecurityStateModel.isContentDangerous(tab.getWebContents());
                    return isContentDangerous;
                }
            };
            mTabObserverRegistrar.registerActivityTabObserver(mIconTabObserver);
        }
    }

    private void stopObserving() {
        mTabObserverRegistrar.unregisterActivityTabObserver(mTabObserver);
        mTabObserverRegistrar.unregisterActivityTabObserver(mIconTabObserver);
    }

    private void onActiveTabChanged() {
        updateTaskDescription();
        if (mForceIcon == null) {
            fetchIcon();
        }
    }

    private void resetIcon() {
        mLargestFavicon = null;
        updateTaskDescription();
    }

    private void updateFavicon(Bitmap favicon) {
        if (favicon == null) return;
        if (mLargestFavicon == null || favicon.getWidth() > mLargestFavicon.getWidth()
                || favicon.getHeight() > mLargestFavicon.getHeight()) {
            mLargestFavicon = favicon;
            updateTaskDescription();
        }
    }

    private void updateTaskDescription() {
        ApiCompatibilityUtils.setTaskDescription(
                mActivity, computeTitle(), computeIcon(), computeThemeColor());
    }

    /**
     * Computes the title for the task description.
     */
    private String computeTitle() {
        if (!TextUtils.isEmpty(mForceTitle)) return mForceTitle;

        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return null;

        String label = currentTab.getTitle();
        String domain = UrlUtilities.getDomainAndRegistry(currentTab.getUrlString(), false);
        if (TextUtils.isEmpty(label)) {
            label = domain;
        }
        return label;
    }

    /**
     * Computes the icon for the task description.
     */
    private Bitmap computeIcon() {
        if (mForceIcon != null) return mForceIcon;

        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return null;

        Bitmap bitmap = null;
        if (!currentTab.isIncognito()) {
            bitmap = mIconGenerator.getBitmap(currentTab.getUrlString(), mLargestFavicon);
        }
        return bitmap;
    }

    /**
     * Computes the theme color for the task description.
     */
    private int computeThemeColor() {
        Tab currentTab = mTabProvider.getTab();
        int themeColor = (currentTab == null || TabThemeColorHelper.isDefaultColorUsed(currentTab))
                ? mDefaultThemeColor
                : TabThemeColorHelper.getColor(currentTab);
        return ColorUtils.getOpaqueColor(themeColor);
    }

    private void fetchIcon() {
        Tab currentTab = mTabProvider.getTab();
        if (currentTab == null) return;

        final String currentUrl = currentTab.getUrlString();
        mFaviconHelper.getLocalFaviconImageForURL(
                Profile.fromWebContents(currentTab.getWebContents()), currentTab.getUrlString(), 0,
                (image, iconUrl) -> {
                    if (mTabProvider.getTab() == null
                            || !TextUtils.equals(
                                    currentUrl, mTabProvider.getTab().getUrlString())) {
                        return;
                    }

                    updateFavicon(image);
                });
    }

    /**
     * Returns true when the activity has been launched in a separate task.
     */
    private boolean usesSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (mActivity.getIntent().getFlags() & separateTaskFlags) != 0;
    }

    /**
     * Destroys all dependent components of the task description helper.
     */
    @Override
    public void destroy() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
        }
        stopObserving();
    }
}
