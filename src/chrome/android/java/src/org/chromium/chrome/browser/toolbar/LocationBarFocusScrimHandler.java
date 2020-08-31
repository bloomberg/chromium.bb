// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

/**
 * Handles showing and hiding a scrim when url bar focus changes.
 */
public class LocationBarFocusScrimHandler implements UrlFocusChangeListener {
    /** The params used to control how the scrim behaves when shown for the omnibox. */
    private PropertyModel mScrimModel;
    private final TabObscuringHandler mTabObscuringHandler;
    private final ScrimCoordinator mScrimCoordinator;
    /** A token held while the toolbar/omnibox is obscuring all visible tabs. */
    private int mTabObscuringToken = TokenHolder.INVALID_TOKEN;

    /** Whether the scrim was shown on focus. */
    private boolean mScrimShown;

    /** The light color to use for the scrim on the NTP. */
    private int mLightScrimColor;
    private final ToolbarDataProvider mToolbarDataProvider;
    private final Runnable mClickDelegate;
    private final Context mContext;
    private NightModeStateProvider mNightModeStateProvider;

    /**
     *
     * @param scrimCoordinator Coordinator responsible for showing and hiding the scrim view.
     * @param tabObscuringHandler Handler used to obscure/unobscure tabs when the scrim is
     *         shown/hidden.
     * @param context Context for retrieving resources.
     * @param nightModeStateProvider Provider of state about night mode color settings.
     * @param toolbarDataProvider Provider of toolbar data, e.g. the NTP state.
     * @param clickDelegate Click handler for the scrim
     * @param scrimTarget View that the scrim should be anchored to.
     */
    public LocationBarFocusScrimHandler(ScrimCoordinator scrimCoordinator,
            TabObscuringHandler tabObscuringHandler, Context context,
            NightModeStateProvider nightModeStateProvider, ToolbarDataProvider toolbarDataProvider,
            Runnable clickDelegate, View scrimTarget) {
        mScrimCoordinator = scrimCoordinator;
        mTabObscuringHandler = tabObscuringHandler;
        mToolbarDataProvider = toolbarDataProvider;
        mClickDelegate = clickDelegate;
        mContext = context;
        mNightModeStateProvider = nightModeStateProvider;

        Resources resources = context.getResources();
        int topMargin =
                resources.getDimensionPixelSize(org.chromium.chrome.R.dimen.tab_strip_height);
        mLightScrimColor = ApiCompatibilityUtils.getColor(resources,
                org.chromium.chrome.R.color.omnibox_focused_fading_background_color_light);

        Callback<Boolean> visibilityChangeCallback = (visible) -> {
            if (visible) {
                // It's possible for the scrim to unfocus and refocus without the
                // visibility actually changing. In this case we have to make sure we
                // unregister the previous token before acquiring a new one.
                int oldToken = mTabObscuringToken;
                mTabObscuringToken = mTabObscuringHandler.obscureAllTabs();
                if (oldToken != TokenHolder.INVALID_TOKEN) {
                    mTabObscuringHandler.unobscureAllTabs(oldToken);
                }
            } else {
                mTabObscuringHandler.unobscureAllTabs(mTabObscuringToken);
                mTabObscuringToken = TokenHolder.INVALID_TOKEN;
            }
        };

        mScrimModel = new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                              .with(ScrimProperties.ANCHOR_VIEW, scrimTarget)
                              .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                              .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                              .with(ScrimProperties.TOP_MARGIN, topMargin)
                              .with(ScrimProperties.CLICK_DELEGATE, mClickDelegate)
                              .with(ScrimProperties.VISIBILITY_CALLBACK, visibilityChangeCallback)
                              .with(ScrimProperties.BACKGROUND_COLOR, ScrimProperties.INVALID_COLOR)
                              .build();
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        boolean useLightColor = !isTablet && !mToolbarDataProvider.isIncognito()
                && !mNightModeStateProvider.isInNightMode();
        mScrimModel.set(ScrimProperties.BACKGROUND_COLOR,
                useLightColor ? mLightScrimColor : ScrimProperties.INVALID_COLOR);

        if (hasFocus && !showScrimAfterAnimationCompletes()) {
            mScrimCoordinator.showScrim(mScrimModel);
            mScrimShown = true;
        } else if (!hasFocus && mScrimShown) {
            mScrimCoordinator.hideScrim(true);
            mScrimShown = false;
        }
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        if (hasFocus && showScrimAfterAnimationCompletes()) {
            mScrimCoordinator.showScrim(mScrimModel);
            mScrimShown = true;
        }
    }

    /**
     * @return Whether the scrim should wait to be shown until after the omnibox is done
     *         animating.
     */
    private boolean showScrimAfterAnimationCompletes() {
        if (mToolbarDataProvider.getNewTabPageForCurrentTab() == null) return false;
        return mToolbarDataProvider.getNewTabPageForCurrentTab().isLocationBarShownInNTP();
    }
}
