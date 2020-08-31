// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.content.res.Resources;
import android.graphics.Color;
import android.os.Build;
import android.view.View;
import android.view.Window;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.UiUtils;
import org.chromium.ui.util.ColorUtils;

/**
 * Maintains the status bar color for a {@link ChromeActivity}.
 */
public class StatusBarColorController
        implements Destroyable, TopToolbarCoordinator.UrlExpansionObserver,
                   StatusIndicatorCoordinator.StatusIndicatorObserver {
    public static final @ColorInt int UNDEFINED_STATUS_BAR_COLOR = Color.TRANSPARENT;
    public static final @ColorInt int DEFAULT_STATUS_BAR_COLOR = Color.argb(0x01, 0, 0, 0);

    /**
     * Provides the base status bar color.
     */
    public interface StatusBarColorProvider {
        /**
         * @return The base status bar color to override default colors used in the
         *         {@link StatusBarColorController}. If this returns
         *         {@link #DEFAULT_STATUS_BAR_COLOR}, {@link StatusBarColorController} will use the
         *         default status bar color.
         *         If this returns a color other than {@link #UNDEFINED_STATUS_BAR_COLOR} and
         *         {@link #DEFAULT_STATUS_BAR_COLOR}, the {@link StatusBarColorController} will
         *         always use the color provided by this method to adjust the status bar color.
         *         This color may be used as-is or adjusted due to a scrim overlay or Android
         *         version.
         */
        @ColorInt
        int getBaseStatusBarColor(Tab tab);
    }

    private final Window mWindow;
    private final boolean mIsTablet;
    private final @Nullable OverviewModeBehavior mOverviewModeBehavior;
    private final StatusBarColorProvider mStatusBarColorProvider;
    private final ScrimView.StatusBarScrimDelegate mStatusBarScrimDelegate;
    private final ActivityTabProvider.ActivityTabTabObserver mStatusBarColorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    private final @ColorInt int mStandardPrimaryBgColor;
    private final @ColorInt int mIncognitoPrimaryBgColor;
    private final @ColorInt int mStandardDefaultThemeColor;
    private final @ColorInt int mIncognitoDefaultThemeColor;

    private @Nullable TabModelSelector mTabModelSelector;
    private @Nullable OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private @Nullable Tab mCurrentTab;
    private boolean mIsInOverviewMode;
    private boolean mIsIncognito;

    private @ColorInt int mScrimColor;
    private float mStatusBarScrimFraction;

    private float mToolbarUrlExpansionPercentage;
    private boolean mShouldUpdateStatusBarColorForNTP;
    private @ColorInt int mStatusIndicatorColor;
    private @ColorInt int mStatusBarColorWithoutStatusIndicator;

    /**
     * @param chromeActivity The {@link ChromeActivity} that this class is attached to.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     */
    public StatusBarColorController(
            ChromeActivity chromeActivity, ActivityTabProvider tabProvider) {
        mWindow = chromeActivity.getWindow();
        mIsTablet = chromeActivity.isTablet();
        mOverviewModeBehavior = chromeActivity.getOverviewModeBehavior();
        mStatusBarColorProvider = chromeActivity;
        mStatusBarScrimDelegate = (fraction) -> {
            mStatusBarScrimFraction = fraction;
            updateStatusBarColor();
        };

        Resources resources = chromeActivity.getResources();
        mStandardPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(resources, false);
        mIncognitoPrimaryBgColor = ChromeColors.getPrimaryBackgroundColor(resources, true);
        mStandardDefaultThemeColor = ChromeColors.getDefaultThemeColor(resources, false);
        mIncognitoDefaultThemeColor = ChromeColors.getDefaultThemeColor(resources, true);

        mStatusIndicatorColor = UNDEFINED_STATUS_BAR_COLOR;

        mStatusBarColorTabObserver = new ActivityTabProvider.ActivityTabTabObserver(tabProvider) {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                updateStatusBarColor();
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updateStatusBarColor();
            }

            @Override
            public void onContentChanged(Tab tab) {
                final boolean newShouldUpdateStatusBarColorForNTP = isStandardNTP();
                // Also update the status bar color if the content was previously an NTP, because an
                // NTP can use a different status bar color than the default theme color. In this
                // case, the theme color might not change, and thus #onDidChangeThemeColor might
                // not get called.
                if (mShouldUpdateStatusBarColorForNTP || newShouldUpdateStatusBarColorForNTP) {
                    updateStatusBarColor();
                }
                mShouldUpdateStatusBarColorForNTP = newShouldUpdateStatusBarColorForNTP;
            }

            @Override
            public void onDestroyed(Tab tab) {
                // Make sure that #mCurrentTab is cleared because #onObservingDifferentTab() might
                // not be notified early enough when #onUrlExpansionPercentageChanged() is called.
                mCurrentTab = null;
                mShouldUpdateStatusBarColorForNTP = false;
            }

            @Override
            protected void onObservingDifferentTab(Tab tab) {
                mCurrentTab = tab;
                mShouldUpdateStatusBarColorForNTP = isStandardNTP();

                // |tab == null| means we're switching tabs - by the tab switcher or by swiping
                // on the omnibox. These cases are dealt with differently, elsewhere.
                if (tab == null) return;
                updateStatusBarColor();
            }
        };

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mIsIncognito = newModel.isIncognito();

                // When opening a new Incognito Tab from a normal Tab (or vice versa), the
                // status bar color is updated. However, this update is triggered after the
                // animation, so we update here for the duration of the new Tab animation.
                // See https://crbug.com/917689.
                updateStatusBarColor();
            }
        };

        if (mOverviewModeBehavior != null) {
            mOverviewModeObserver = new EmptyOverviewModeObserver() {
                @Override
                public void onOverviewModeStartedShowing(boolean showToolbar) {
                    mIsInOverviewMode = true;
                    updateStatusBarColor();
                }

                @Override
                public void onOverviewModeFinishedHiding() {
                    mIsInOverviewMode = false;
                    updateStatusBarColor();
                }
            };
            mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);
        }

        chromeActivity.getLifecycleDispatcher().register(this);
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        mStatusBarColorTabObserver.destroy();
        if (mOverviewModeBehavior != null) {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
    }

    // TopToolbarCoordinator.UrlExpansionObserver implementation.
    @Override
    public void onUrlExpansionPercentageChanged(float percentage) {
        mToolbarUrlExpansionPercentage = percentage;
        if (mShouldUpdateStatusBarColorForNTP) updateStatusBarColor();
    }

    // StatusIndicatorCoordinator.StatusIndicatorObserver implementation.

    @Override
    public void onStatusIndicatorColorChanged(@ColorInt int newColor) {
        mStatusIndicatorColor = newColor;
        updateStatusBarColor();
    }

    /**
     * @param tabModelSelector The {@link TabModelSelector} to check whether incognito model is
     *                         selected.
     */
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        assert mTabModelSelector == null : "mTabModelSelector should only be set once.";
        mTabModelSelector = tabModelSelector;
        if (mTabModelSelector != null) mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    /**
     * @return The {@link ScrimView.StatusBarScrimDelegate} that adjusts the status bar color based
     *         on the scrim offset.
     */
    public ScrimView.StatusBarScrimDelegate getStatusBarScrimDelegate() {
        return mStatusBarScrimDelegate;
    }

    public void updateStatusBarColor() {
        mStatusBarColorWithoutStatusIndicator = calculateBaseStatusBarColor();
        if (shouldDarkenStatusBarColor()) {
            mStatusBarColorWithoutStatusIndicator =
                    ColorUtils.getDarkenedColorForStatusBar(mStatusBarColorWithoutStatusIndicator);
        }

        int statusBarColor = applyStatusBarIndicatorColor(mStatusBarColorWithoutStatusIndicator);
        statusBarColor = applyCurrentScrimToColor(statusBarColor);
        setStatusBarColor(statusBarColor);
    }

    // TODO(sinansahin): Confirm pre-M expectations with UX and update as needed.
    /**
     * @return The status bar color without the status indicator's color taken into consideration.
     *         Color returned from this method includes darkening if the OS version doesn't support
     *         light status bar icons (pre-M). However, scrimming isn't included since it's managed
     *         completely by this class.
     */
    public @ColorInt int getStatusBarColorWithoutStatusIndicator() {
        return mStatusBarColorWithoutStatusIndicator;
    }

    private @ColorInt int calculateBaseStatusBarColor() {
        // Return overridden status bar color from StatusBarColorProvider if specified.
        final int baseStatusBarColor = mStatusBarColorProvider.getBaseStatusBarColor(mCurrentTab);
        if (baseStatusBarColor == DEFAULT_STATUS_BAR_COLOR) {
            return calculateDefaultStatusBarColor();
        }
        if (baseStatusBarColor != UNDEFINED_STATUS_BAR_COLOR) {
            return baseStatusBarColor;
        }

        // We don't adjust status bar color for tablet when status bar color is not overridden by
        // StatusBarColorProvider.
        if (mIsTablet) return Color.BLACK;

        // Return status bar color in overview mode.
        if (mIsInOverviewMode) {
            if (shouldDarkenStatusBarColor()) return Color.BLACK;

            return (mIsIncognito && ToolbarColors.canUseIncognitoToolbarThemeColorInOverview())
                    ? mIncognitoPrimaryBgColor
                    : mStandardPrimaryBgColor;
        }

        // Return status bar color in standard NewTabPage. If location bar is not shown in NTP, we
        // use the tab theme color regardless of the URL expansion percentage.
        if (isLocationBarShownInNTP()) {
            if (shouldDarkenStatusBarColor()) return Color.BLACK;

            return ColorUtils.getColorWithOverlay(
                    TabThemeColorHelper.getBackgroundColor(mCurrentTab),
                    TabThemeColorHelper.getColor(mCurrentTab), mToolbarUrlExpansionPercentage);
        }

        // Return status bar color to match the toolbar.
        if (mCurrentTab != null && !TabThemeColorHelper.isDefaultColorUsed(mCurrentTab)) {
            return TabThemeColorHelper.getColor(mCurrentTab);
        }

        return calculateDefaultStatusBarColor();
    }

    /**
     * Calculates the default status bar color based on the Android version and incognito state.
     */
    private @ColorInt int calculateDefaultStatusBarColor() {
        if (shouldDarkenStatusBarColor()) {
            return Color.BLACK;
        }
        return mIsIncognito ? mIncognitoDefaultThemeColor : mStandardDefaultThemeColor;
    }

    /**
     * Set device status bar to a given color. Also, set the status bar icons to a dark color if
     * needed.
     * @param color The color that the status bar should be set to.
     */
    private void setStatusBarColor(@ColorInt int color) {
        if (UiUtils.isSystemUiThemingDisabled()) return;

        final View root = mWindow.getDecorView().getRootView();
        boolean needsDarkStatusBarIcons = !ColorUtils.shouldUseLightForegroundOnBackground(color);
        ApiCompatibilityUtils.setStatusBarIconColor(root, needsDarkStatusBarIcons);
        ApiCompatibilityUtils.setStatusBarColor(mWindow, color);
    }

    /**
     * Takes status bar indicator into account in status bar color computation.
     * @param color The status bar color without the status indicator's color taken into
     *              consideration. (as specified in
     *              {@link getStatusBarColorWithoutStatusIndicator()}).
     * @return The resulting color.
     */
    private @ColorInt int applyStatusBarIndicatorColor(@ColorInt int darkenedBaseColor) {
        if (mStatusIndicatorColor == UNDEFINED_STATUS_BAR_COLOR) return darkenedBaseColor;

        return shouldDarkenStatusBarColor()
                ? ColorUtils.getDarkenedColorForStatusBar(mStatusIndicatorColor)
                : mStatusIndicatorColor;
    }

    /**
     * Get the scrim applied color if the scrim is showing. Otherwise, return the original color.
     * @param color Color to maybe apply scrim to.
     * @return The resulting color.
     */
    private @ColorInt int applyCurrentScrimToColor(@ColorInt int color) {
        if (shouldDarkenStatusBarColor()) return color;

        if (mScrimColor == 0) {
            final View root = mWindow.getDecorView().getRootView();
            final Resources resources = root.getResources();
            mScrimColor = ApiCompatibilityUtils.getColor(resources, R.color.black_alpha_65);
        }
        // Apply a color overlay if the scrim is showing.
        float scrimColorAlpha = (mScrimColor >>> 24) / 255f;
        int scrimColorOpaque = mScrimColor & 0xFF000000;
        return ColorUtils.getColorWithOverlay(
                color, scrimColorOpaque, mStatusBarScrimFraction * scrimColorAlpha);
    }

    /**
     * @return Whether to to darken the status bar color because the OS version does not support
     * light status bar icons.
     */
    private boolean shouldDarkenStatusBarColor() {
        return (Build.VERSION.SDK_INT < Build.VERSION_CODES.M);
    }

    /**
     * @return Whether or not the current tab is a new tab page in standard mode.
     */
    private boolean isStandardNTP() {
        return mCurrentTab != null && mCurrentTab.getNativePage() instanceof NewTabPage;
    }

    /**
     * @return Whether or not the fake location bar is shown on the current NTP.
     */
    private boolean isLocationBarShownInNTP() {
        if (!isStandardNTP()) return false;
        final NewTabPage newTabPage = (NewTabPage) mCurrentTab.getNativePage();
        return newTabPage != null && newTabPage.isLocationBarShownInNTP();
    }
}
