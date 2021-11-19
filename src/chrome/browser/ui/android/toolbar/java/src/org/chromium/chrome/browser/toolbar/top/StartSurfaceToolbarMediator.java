// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.ACCESSIBILITY_ENABLED;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOMEPAGE_ENABLED_SUPPLIER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOMEPAGE_MANAGED_BY_POLICY_SUPPLIER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOME_BUTTON_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.HOME_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_STATE_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_COUNT_PROVIDER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_TAB_MODEL_SELECTOR;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IN_START_SURFACE_MODE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_HIGHLIGHT;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_VIEW_TEXT_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.SHOW_ANIMATION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TAB_SWITCHER_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.TRANSLATION_Y;

import android.graphics.Bitmap;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.ui.modelutil.PropertyModel;

/** The mediator implements interacts between the views and the caller. */
class StartSurfaceToolbarMediator {
    private final PropertyModel mPropertyModel;
    private final Callback<IPHCommandBuilder> mShowIdentityIPHCallback;
    private final boolean mHideIncognitoSwitchWhenNoTabs;
    private final boolean mShouldShowTabSwitcherButtonOnHomepage;
    private final Supplier<ButtonData> mIdentityDiscButtonSupplier;
    private final boolean mIsTabGroupsAndroidContinuationEnabled;
    private final UserEducationHelper mUserEducationHelper;
    private final BooleanSupplier mIsIncognitoModeEnabledSupplier;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final IncognitoTabModelObserver mIncognitoTabModelObserver;

    private TabModelSelector mTabModelSelector;
    private TabCountProvider mTabCountProvider;
    private LayoutStateProvider mLayoutStateProvider;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    @StartSurfaceState
    private int mStartSurfaceState;
    private boolean mIsAnimationEnabled;
    private boolean mDefaultSearchEngineHasLogo;
    private boolean mShouldShowStartSurfaceAsHomepage;
    private boolean mHomepageEnabled;

    private CallbackController mCallbackController = new CallbackController();
    private float mNonIncognitoHomepageTranslationY;

    private boolean mShowHomeButtonOnTabSwitcher;
    private StartSurfaceHomeButtonIPHController mStartSurfaceHomeButtonIPHController;
    private View mHomeButtonView;

    StartSurfaceToolbarMediator(PropertyModel model,
            Callback<IPHCommandBuilder> showIdentityIPHCallback,
            boolean hideIncognitoSwitchWhenNoTabs, boolean showHomeButtonOnTabSwitcher,
            MenuButtonCoordinator menuButtonCoordinator,
            ObservableSupplier<Boolean> identityDiscStateSupplier,
            Supplier<ButtonData> identityDiscButtonSupplier,
            ObservableSupplier<Boolean> homepageEnabledSupplier,
            ObservableSupplier<Boolean> startSurfaceAsHomepageSupplier,
            ObservableSupplier<Boolean> homepageManagedByPolicySupplier,
            OnClickListener homeButtonOnClickHandler, boolean shouldShowTabSwitcherButtonOnHomepage,
            boolean isTabGroupsAndroidContinuationEnabled, UserEducationHelper userEducationHelper,
            BooleanSupplier isIncognitoModeEnabledSupplier, boolean isAnimationEnabled) {
        mPropertyModel = model;
        mStartSurfaceState = StartSurfaceState.NOT_SHOWN;
        mShowIdentityIPHCallback = showIdentityIPHCallback;
        mHideIncognitoSwitchWhenNoTabs = hideIncognitoSwitchWhenNoTabs;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mIdentityDiscButtonSupplier = identityDiscButtonSupplier;
        mIsTabGroupsAndroidContinuationEnabled = isTabGroupsAndroidContinuationEnabled;
        mUserEducationHelper = userEducationHelper;
        mIsIncognitoModeEnabledSupplier = isIncognitoModeEnabledSupplier;
        identityDiscStateSupplier.addObserver((canShowHint) -> {
            // If the identity disc wants to be hidden and is hidden, there's nothing we need to do.
            if (!canShowHint && !mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE)) return;
            updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        });

        mShowHomeButtonOnTabSwitcher = showHomeButtonOnTabSwitcher;
        if (mShowHomeButtonOnTabSwitcher) {
            mPropertyModel.set(HOMEPAGE_ENABLED_SUPPLIER, homepageEnabledSupplier);
            homepageEnabledSupplier.addObserver(mCallbackController.makeCancelable((enabled) -> {
                mHomepageEnabled = enabled;
                updateHomeButtonVisibility();
            }));
            mPropertyModel.set(
                    HOMEPAGE_MANAGED_BY_POLICY_SUPPLIER, homepageManagedByPolicySupplier);

            View.OnClickListener homeButtonOnClickListener = v -> {
                if (homeButtonOnClickHandler != null) homeButtonOnClickHandler.onClick(v);

                if (mStartSurfaceHomeButtonIPHController != null) {
                    mStartSurfaceHomeButtonIPHController.onHomeButtonClicked();
                }
            };

            mPropertyModel.set(HOME_BUTTON_CLICK_HANDLER, homeButtonOnClickListener);
            startSurfaceAsHomepageSupplier.addObserver(
                    mCallbackController.makeCancelable((showStartSurfaceAsHomepage) -> {
                        mShouldShowStartSurfaceAsHomepage = showStartSurfaceAsHomepage;
                        updateHomeButtonVisibility();
                    }));
        }
        mShouldShowTabSwitcherButtonOnHomepage = shouldShowTabSwitcherButtonOnHomepage;
        mIsAnimationEnabled = isAnimationEnabled;

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
                updateIdentityDisc(mIdentityDiscButtonSupplier.get());
            }

            @Override
            public void onTabStateInitialized() {
                maybeInitializeIncognitoToggle();
            }
        };

        mIncognitoTabModelObserver = new IncognitoTabModelObserver() {
            @Override
            public void wasFirstTabCreated() {
                updateIncognitoToggleTabVisibility();
            }

            @Override
            public void didBecomeEmpty() {
                updateIncognitoToggleTabVisibility();
            }
        };
    }

    void destroy() {
        if (mTabModelSelector != null && mTabModelSelectorObserver != null) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        if (mTabModelSelector != null && mIncognitoTabModelObserver != null) {
            mTabModelSelector.removeIncognitoTabModelObserver(mIncognitoTabModelObserver);
        }
        if (mLayoutStateObserver != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    void onStartSurfaceStateChanged(
            @StartSurfaceState int newState, boolean shouldShowStartSurfaceToolbar) {
        updateShowAnimation(newState, shouldShowStartSurfaceToolbar);
        mStartSurfaceState = newState;
        updateLogoVisibility();
        updateTabSwitcherButtonVisibility();
        updateIncognitoToggleTabVisibility();
        updateNewTabViewVisibility();
        updateHomeButtonVisibility();
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        updateAppMenuUpdateBadgeSuppression();
        setStartSurfaceToolbarVisibility(shouldShowStartSurfaceToolbar);
        updateTranslationY(mNonIncognitoHomepageTranslationY);
    }

    void onStartSurfaceHeaderOffsetChanged(int verticalOffset) {
        updateTranslationY(verticalOffset);
    }

    /**
     * The real omnibox should be shown in three cases:
     * 1. It's on the homepage, the url is focused and start surface toolbar is not visible.
     * 2. It's on the homepage and the start surface toolbar is scrolled off.
     * 3. It's on a tab.
     *
     * In the other cases:
     * 1. It's on the homepage and start surface toolbar is at least partially shown -- the user
     *    sees the fake search box.
     * 2. It's on the tab switcher surface, there is no search box (fake or real).
     *
     * @param toolbarHeight The height of start surface toolbar.
     * @return Whether toolbar layout should be shown.
     */
    boolean shouldShowRealSearchBox(int toolbarHeight) {
        return isRealSearchBoxFocused() || isStartSurfaceToolbarScrolledOff(toolbarHeight)
                || isOnATab();
    }

    /** Returns whether it's on the start surface homepage. */
    boolean isOnHomepage() {
        return mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;
    }

    /** Returns whether it's on a normal tab. */
    private boolean isOnATab() {
        return mStartSurfaceState == StartSurfaceState.NOT_SHOWN;
    }

    /**
     * When mPropertyModel.get(IS_VISIBLE) is false on the homepage, start surface toolbar and fake
     * search box are not shown and the real search box is focused.
     * @return Whether the real search box is focused.
     */
    private boolean isRealSearchBoxFocused() {
        return isOnHomepage() && !mPropertyModel.get(IS_VISIBLE);
    }

    /**
     * Start surface toolbar is only scrolled on the homepage. When scrolling offset is larger than
     * toolbar height, start surface toolbar is scrolled out of the screen.
     *
     * @param toolbarHeight The height of start surface toolbar.
     * @return Whether the start surface toolbar is scrolled out of the screen.
     */
    private boolean isStartSurfaceToolbarScrolledOff(int toolbarHeight) {
        return mPropertyModel.get(IS_VISIBLE)
                && -mPropertyModel.get(TRANSLATION_Y) >= toolbarHeight;
    }

    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mPropertyModel.set(NEW_TAB_CLICK_HANDLER, listener);
    }

    void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
    }

    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;

        if (mTabModelSelector.isTabStateInitialized()) maybeInitializeIncognitoToggle();
        mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
        updateIdentityDisc(mIdentityDiscButtonSupplier.get());
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelSelector.addIncognitoTabModelObserver(mIncognitoTabModelObserver);
    }

    private void maybeInitializeIncognitoToggle() {
        if (mIsIncognitoModeEnabledSupplier.getAsBoolean()) {
            assert mTabCountProvider != null;
            mPropertyModel.set(INCOGNITO_TAB_COUNT_PROVIDER, mTabCountProvider);
            mPropertyModel.set(INCOGNITO_TAB_MODEL_SELECTOR, mTabModelSelector);
        }
    }

    private void updateIncognitoToggleTabVisibility() {
        if (mStartSurfaceState != StartSurfaceState.SHOWN_TABSWITCHER
                && mStartSurfaceState != StartSurfaceState.SHOWING_TABSWITCHER) {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, false);
            updateNewTabViewTextVisibility();
            return;
        }

        if (mHideIncognitoSwitchWhenNoTabs) {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, hasIncognitoTabs());
        } else {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, true);
        }
        updateNewTabViewTextVisibility();
    }

    // TODO(crbug.com/1042997): share with TabSwitcherModeTTPhone.
    private boolean hasIncognitoTabs() {
        if (mTabModelSelector == null) return false;

        // Check if there is no incognito tab, or all the incognito tabs are being closed.
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        for (int i = 0; i < incognitoTabModel.getCount(); i++) {
            if (!incognitoTabModel.getTabAt(i).isClosing()) return true;
        }
        return false;
    }

    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        mPropertyModel.set(IN_START_SURFACE_MODE, inStartSurfaceMode);
    }

    void setStartSurfaceToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        mPropertyModel.set(IS_VISIBLE, shouldShowStartSurfaceToolbar);
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mPropertyModel.set(INCOGNITO_STATE_PROVIDER, provider);
    }

    void onAccessibilityStatusChanged(boolean enabled) {
        mPropertyModel.set(ACCESSIBILITY_ENABLED, enabled);
        updateNewTabViewVisibility();
    }

    void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        assert mLayoutStateProvider == null : "the mLayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    updateIncognitoToggleTabVisibility();
                }
            }
            @Override
            public void onFinishedShowing(@LayoutType int layoutType) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    mPropertyModel.set(BUTTONS_CLICKABLE, true);
                    mMenuButtonCoordinator.setClickable(true);
                }
            }
            @Override
            public void onStartedHiding(
                    @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {
                if (layoutType == LayoutType.TAB_SWITCHER) {
                    mPropertyModel.set(BUTTONS_CLICKABLE, false);
                    mMenuButtonCoordinator.setClickable(false);
                }
            }
        };

        if (mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            new Handler().post(() -> {
                mLayoutStateObserver.onStartedShowing(LayoutType.TAB_SWITCHER, true);
                mLayoutStateObserver.onFinishedShowing(LayoutType.TAB_SWITCHER);
            });
        }

        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        mPropertyModel.set(NEW_TAB_BUTTON_HIGHLIGHT, highlight);
    }

    void setHomeButtonView(View homeButtonView) {
        mHomeButtonView = homeButtonView;
    }

    /**
     * This method should be called when there is a possibility that logo image became available or
     * was changed.
     * @param logoImage The logo image in bitmap format.
     * @param contentDescription The accessibility text describing the logo.
     */
    void onLogoImageAvailable(Bitmap logoImage, String contentDescription) {
        mDefaultSearchEngineHasLogo = logoImage != null;
        updateLogoVisibility();
        mPropertyModel.set(LOGO_IMAGE, logoImage);
        mPropertyModel.set(LOGO_CONTENT_DESCRIPTION, contentDescription);
    }

    private void updateLogoVisibility() {
        boolean shouldShowLogo = mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                && mDefaultSearchEngineHasLogo;
        mPropertyModel.set(LOGO_IS_VISIBLE, shouldShowLogo);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void updateIdentityDisc(ButtonData buttonData) {
        boolean shouldShow = buttonData.canShow() && !mTabModelSelector.isIncognitoSelected();
        if (shouldShow) {
            ButtonSpec buttonSpec = buttonData.getButtonSpec();
            mPropertyModel.set(IDENTITY_DISC_CLICK_HANDLER, buttonSpec.getOnClickListener());
            // Take a defensive copy of the Drawable, since Drawables aren't immutable, and another
            // view mutating our drawable could cause it to display incorrectly.
            mPropertyModel.set(
                    IDENTITY_DISC_IMAGE, buttonSpec.getDrawable().getConstantState().newDrawable());
            mPropertyModel.set(IDENTITY_DISC_DESCRIPTION, buttonSpec.getContentDescriptionResId());
            mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, true);
            mShowIdentityIPHCallback.onResult(buttonSpec.getIPHCommandBuilder());
        } else {
            mPropertyModel.set(IDENTITY_DISC_IS_VISIBLE, false);
        }
    }

    private void updateNewTabViewVisibility() {
        boolean isShownTabSwitcherState = mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                || mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER;

        // This button is only shown for homepage when accessibility is enabled and
        // OverviewListLayout is shown as the tab switcher instead of the start surface.
        mPropertyModel.set(NEW_TAB_VIEW_IS_VISIBLE,
                isShownTabSwitcherState
                        || (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                                && !mIsTabGroupsAndroidContinuationEnabled));

        updateNewTabViewAtStart();
    }

    private void updateNewTabViewAtStart() {
        if (!mPropertyModel.get(NEW_TAB_VIEW_IS_VISIBLE)) return;
        mPropertyModel.set(NEW_TAB_VIEW_AT_START, !mPropertyModel.get(HOME_BUTTON_IS_VISIBLE));
        updateNewTabViewTextVisibility();
    }

    private void updateNewTabViewTextVisibility() {
        // Show new tab view text view when new tab view is at start and incognito switch
        // is hidden.
        mPropertyModel.set(NEW_TAB_VIEW_TEXT_IS_VISIBLE,
                mPropertyModel.get(NEW_TAB_VIEW_AT_START) && mHideIncognitoSwitchWhenNoTabs
                        && !hasIncognitoTabs());
    }

    private void updateHomeButtonVisibility() {
        boolean isShownTabSwitcherState = mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER
                || mStartSurfaceState == StartSurfaceState.SHOWING_TABSWITCHER;
        boolean shouldShow = mHomepageEnabled && isShownTabSwitcherState
                && !mPropertyModel.get(IS_INCOGNITO) && mShowHomeButtonOnTabSwitcher
                && mShouldShowStartSurfaceAsHomepage;
        // If start surface is not shown as the homepage, home button shouldn't be shown on tab
        // switcher page.
        mPropertyModel.set(HOME_BUTTON_IS_VISIBLE, shouldShow);
        updateNewTabViewAtStart();

        // If the home button is shown, maybe show the IPH.
        if (mHomeButtonView != null && shouldShow) {
            if (mStartSurfaceHomeButtonIPHController == null) {
                mStartSurfaceHomeButtonIPHController = new StartSurfaceHomeButtonIPHController(
                        mUserEducationHelper, mHomeButtonView);
            }
            mStartSurfaceHomeButtonIPHController.maybeShowIPH();
        }
    }

    private void updateTabSwitcherButtonVisibility() {
        // This button should only be shown on homepage. On tab switcher page, new tab button is
        // shown.
        boolean shouldShow = mShouldShowTabSwitcherButtonOnHomepage
                && mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE;
        mPropertyModel.set(TAB_SWITCHER_BUTTON_IS_VISIBLE, shouldShow);
        // If tab switcher button is visible, we should move identity disc to the left.
        mPropertyModel.set(IDENTITY_DISC_AT_START, shouldShow);
    }

    private void updateAppMenuUpdateBadgeSuppression() {
        mMenuButtonCoordinator.setAppMenuUpdateBadgeSuppressed(
                mStartSurfaceState == StartSurfaceState.SHOWN_TABSWITCHER);
    }

    private void updateTranslationY(float transY) {
        if (mStartSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE
                && !mPropertyModel.get(IS_INCOGNITO)) {
            // If it's on the non-incognito homepage, document the homepage translationY.
            mNonIncognitoHomepageTranslationY = transY;
            // Update the translationY of the toolbarView.
            mPropertyModel.set(TRANSLATION_Y, transY);
        } else {
            // If it's not on the non-incognito homepage, set the translationY as 0.
            mPropertyModel.set(TRANSLATION_Y, 0);
        }
    }

    private void updateShowAnimation(
            @StartSurfaceState int newState, boolean shouldShowStartSurfaceToolbar) {
        mPropertyModel.set(SHOW_ANIMATION,
                shouldShowStartSurfaceToolbar && mIsAnimationEnabled
                        && isSwitchingBetweenOverviews(mStartSurfaceState, newState));
    }

    private boolean isSwitchingBetweenOverviews(
            @StartSurfaceState int previousState, @StartSurfaceState int newState) {
        // The previousState should never be SHOWING states because if it's already on overview
        // page, SHOWING state should've been updated to SHOWN state. The following transitions are
        // handled:
        // * SHOWN_HOMEPAGE -> SHOWING_TABSWITCHER
        // * SHOWN_HOMEPAGE -> SHOWN_TABSWITCHER
        // * SHOWN_TABSWITCHER -> SHOWING_HOMEPAGE
        // * SHOWN_TABSWITCHER -> SHOWN_HOMEPAGE
        return (previousState == StartSurfaceState.SHOWN_HOMEPAGE
                       && newState == StartSurfaceState.SHOWING_TABSWITCHER)
                || (previousState == StartSurfaceState.SHOWN_HOMEPAGE
                        && newState == StartSurfaceState.SHOWN_TABSWITCHER)
                || (previousState == StartSurfaceState.SHOWN_TABSWITCHER
                        && newState == StartSurfaceState.SHOWING_HOMEPAGE)
                || (previousState == StartSurfaceState.SHOWN_TABSWITCHER
                        && newState == StartSurfaceState.SHOWN_HOMEPAGE);
    }

    @VisibleForTesting
    @StartSurfaceState
    int getOverviewModeStateForTesting() {
        return mStartSurfaceState;
    }

    @VisibleForTesting
    void setShowHomeButtonOnTabSwitcherForTesting(boolean showHomeButtonOnTabSwitcher) {
        mShowHomeButtonOnTabSwitcher = showHomeButtonOnTabSwitcher;
    }

    @VisibleForTesting
    void setStartSurfaceHomeButtonIPHControllerForTesting(
            StartSurfaceHomeButtonIPHController startSurfaceHomeButtonIPHController) {
        mStartSurfaceHomeButtonIPHController = startSurfaceHomeButtonIPHController;
    }
}
