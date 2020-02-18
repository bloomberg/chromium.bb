// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.UserData;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;

/**
 * Represents the suspension page presented when a user tries to visit a site whose fully-qualified
 * domain name (FQDN) has been suspended via Digital Wellbeing.
 */
public class SuspendedTab extends EmptyTabObserver implements UserData {
    private static final String DIGITAL_WELLBEING_SITE_DETAILS_ACTION =
            "org.chromium.chrome.browser.usage_stats.action.SHOW_WEBSITE_DETAILS";
    private static final String EXTRA_FQDN_NAME =
            "org.chromium.chrome.browser.usage_stats.extra.FULLY_QUALIFIED_DOMAIN_NAME";
    private static final String TAG = "SuspendedTab";
    private static final Class<SuspendedTab> USER_DATA_KEY = SuspendedTab.class;

    public static boolean isShowing(Tab tab) {
        if (tab == null || !tab.isInitialized()) return false;
        SuspendedTab suspendedTab = get(tab);
        return suspendedTab != null && suspendedTab.isShowing();
    }

    public static SuspendedTab from(Tab tab) {
        SuspendedTab suspendedTab = get(tab);
        if (suspendedTab == null) {
            suspendedTab = tab.getUserDataHost().setUserData(USER_DATA_KEY, new SuspendedTab(tab));
        }
        return suspendedTab;
    }

    public static SuspendedTab get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private final Tab mTab;
    private View mView;
    private String mFqdn;

    private SuspendedTab(Tab tab) {
        mTab = tab;
    }

    /**
     * Show the suspended tab UI within the root view of the associated tab. This will stop loading
     * of mTab so that the page is not also rendered. If the suspended tab is already showing, this
     * will update its fqdn to the given one.
     */
    public void show(String fqdn) {
        mFqdn = fqdn;
        mTab.addObserver(this);
        mTab.stopLoading();

        WebContents webContents = mTab.getWebContents();
        if (webContents != null) {
            webContents.onHide();
            webContents.suspendAllMediaPlayers();
            webContents.setAudioMuted(true);
            WebContentsAccessibility.fromWebContents(webContents).setObscuredByAnotherView(true);
        }

        InfoBarContainer infoBarContainer = InfoBarContainer.get(mTab);
        if (infoBarContainer != null) {
            infoBarContainer.setHidden(true);
        }

        if (isViewAttached()) {
            updateFqdnText();
        } else {
            attachView();
        }

        TabContentManager tabContentManager = mTab.getActivity().getTabContentManager();
        if (tabContentManager != null) {
            // We have to wait for the view to layout to cache a new thumbnail for it; otherwise,
            // its width and height won't be available yet.
            mView.post(() -> {
                tabContentManager.removeTabThumbnail(mTab.getId());
                tabContentManager.cacheTabThumbnail(mTab);
            });
        }
    }

    /** Remove the suspended tab UI if it's currently being shown. */
    public void removeIfPresent() {
        removeViewIfPresent();

        WebContents webContents = mTab.getWebContents();
        if (webContents != null) {
            webContents.onShow();
            webContents.setAudioMuted(false);
            WebContentsAccessibility.fromWebContents(webContents).setObscuredByAnotherView(false);
        }

        mView = null;
        mFqdn = null;
    }

    /** @return the fqdn this SuspendedTab is currently showing for; null if not showing. */
    public String getFqdn() {
        return mFqdn;
    }

    /** @return Whether this SuspendedTab is currently showing. */
    public boolean isShowing() {
        return mFqdn != null;
    }

    @VisibleForTesting
    boolean isViewAttached() {
        return mView != null && mView.getParent() == mTab.getContentView();
    }

    private View createView() {
        Context context = mTab.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);

        View suspendedTabView = inflater.inflate(R.layout.suspended_tab, null);
        return suspendedTabView;
    }

    private void attachView() {
        assert mView == null;

        ViewGroup parent = mTab.getContentView();
        // getContentView() will return null if the tab doesn't have a WebContents, which is
        // possible in some situations, e.g. if the renderer crashes.
        if (parent == null) return;
        mView = createView();
        parent.addView(mView,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        updateFqdnText();
    }

    private void updateFqdnText() {
        Context context = mTab.getContext();
        TextView explanationText = (TextView) mView.findViewById(R.id.suspended_tab_explanation);
        explanationText.setText(
                context.getString(R.string.usage_stats_site_paused_explanation, mFqdn));
        setSettingsLinkClickListener();
    }

    private void setSettingsLinkClickListener() {
        Context context = mTab.getContext();
        View settingsLink = mView.findViewById(R.id.suspended_tab_settings_button);
        settingsLink.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(DIGITAL_WELLBEING_SITE_DETAILS_ACTION);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra(EXTRA_FQDN_NAME, mFqdn);
                intent.putExtra(Intent.EXTRA_PACKAGE_NAME,
                        ContextUtils.getApplicationContext().getPackageName());
                try {
                    context.startActivity(intent);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "No activity found for site details intent", e);
                }
            }
        });
    }

    private void removeViewIfPresent() {
        if (isViewAttached()) {
            mTab.getContentView().removeView(mView);
            mView = null;
        }
    }

    // TabObserver implementation.
    @Override
    public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
        if (!isAttached) {
            removeViewIfPresent();
        } else {
            attachView();
        }
    }

    // UserData implementation.
    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }
}
