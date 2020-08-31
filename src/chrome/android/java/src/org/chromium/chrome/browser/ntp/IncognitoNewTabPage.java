// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.graphics.Canvas;
import android.os.Build;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Provides functionality when the user interacts with the Incognito NTP.
 */
public class IncognitoNewTabPage
        extends BasicNativePage implements InvalidationAwareThumbnailProvider {
    private Activity mActivity;

    private String mTitle;
    protected IncognitoNewTabPageView mIncognitoNewTabPageView;

    private boolean mIsLoaded;

    private IncognitoNewTabPageManager mIncognitoNewTabPageManager;
    private IncognitoCookieControlsManager mCookieControlsManager;
    private IncognitoCookieControlsManager.Observer mCookieControlsObserver;

    private final int mIncognitoNTPBackgroundColor;

    private void showIncognitoLearnMore() {
        HelpAndFeedback.getInstance().show(mActivity,
                mActivity.getString(R.string.help_context_incognito_learn_more),
                Profile.getLastUsedRegularProfile().getOffTheRecordProfile(), null);
    }

    /**
     * Constructs an Incognito NewTabPage.
     * @param activity The activity used to create the new tab page's View.
     */
    public IncognitoNewTabPage(Activity activity, NativePageHost host) {
        super(host);

        mActivity = activity;

        mIncognitoNTPBackgroundColor = ApiCompatibilityUtils.getColor(
                host.getContext().getResources(), R.color.ntp_bg_incognito);

        mIncognitoNewTabPageManager = new IncognitoNewTabPageManager() {
            @Override
            public void loadIncognitoLearnMore() {
                // Incognito 'Learn More' either opens a new activity or a new non-incognito tab.
                // Both is not supported in VR. Request to exit VR and if we succeed show the 'Learn
                // More' page in 2D.
                if (VrModuleProvider.getDelegate().isInVr()) {
                    VrModuleProvider.getDelegate().requestToExitVrAndRunOnSuccess(
                            IncognitoNewTabPage.this ::showIncognitoLearnMore);
                    return;
                }
                showIncognitoLearnMore();
            }

            @Override
            public void initCookieControlsManager() {
                mCookieControlsManager = new IncognitoCookieControlsManager();
                mCookieControlsManager.initialize();
                mIncognitoNewTabPageView.setIncognitoCookieControlsCardVisibility(
                        mCookieControlsManager.shouldShowCookieControlsCard());
                mCookieControlsObserver = new IncognitoCookieControlsManager.Observer() {
                    @Override
                    public void onUpdate(
                            boolean checked, @CookieControlsEnforcement int enforcement) {
                        mIncognitoNewTabPageView.setIncognitoCookieControlsToggleEnforcement(
                                enforcement);
                        mIncognitoNewTabPageView.setIncognitoCookieControlsToggleChecked(checked);
                    }
                };
                mCookieControlsManager.addObserver(mCookieControlsObserver);
                mIncognitoNewTabPageView.setIncognitoCookieControlsToggleCheckedListener(
                        mCookieControlsManager);
                mIncognitoNewTabPageView.setIncognitoCookieControlsIconOnclickListener(
                        mCookieControlsManager);
                mCookieControlsManager.updateIfNecessary();
            }

            @Override
            public boolean shouldCaptureThumbnail() {
                return mCookieControlsManager.shouldCaptureThumbnail();
            }

            @Override
            public void destroy() {
                if (mCookieControlsManager != null) {
                    mCookieControlsManager.removeObserver(mCookieControlsObserver);
                }
            }

            @Override
            public void onLoadingComplete() {
                mIsLoaded = true;
            }
        };

        mTitle = host.getContext().getResources().getString(R.string.button_new_tab);

        LayoutInflater inflater = LayoutInflater.from(host.getContext());
        mIncognitoNewTabPageView =
                (IncognitoNewTabPageView) inflater.inflate(R.layout.new_tab_page_incognito, null);
        mIncognitoNewTabPageView.initialize(mIncognitoNewTabPageManager);

        TextView newTabIncognitoHeader =
                mIncognitoNewTabPageView.findViewById(R.id.new_tab_incognito_title);
        newTabIncognitoHeader.setText(R.string.new_tab_otr_title);

        // Work around https://crbug.com/943873 and https://crbug.com/963385 where default focus
        // highlight shows up after toggling dark mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mIncognitoNewTabPageView.setDefaultFocusHighlightEnabled(false);
        }

        initWithView(mIncognitoNewTabPageView);
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    @VisibleForTesting
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert !ViewCompat
                .isAttachedToWindow(getView()) : "Destroy called before removed from window";
        mIncognitoNewTabPageManager.destroy();
        super.destroy();
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public int getBackgroundColor() {
        return mIncognitoNTPBackgroundColor;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mIncognitoNewTabPageView.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mIncognitoNewTabPageView.captureThumbnail(canvas);
    }
}
