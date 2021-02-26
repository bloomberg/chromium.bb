// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonCoordinator;
import org.chromium.chrome.browser.toolbar.TabSwitcherButtonView;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The controller for the StartSurfaceToolbar. This class handles all interactions that the
 * StartSurfaceToolbar has with the outside world. Lazily creates the tab toolbar the first time
 * it's needed.
 */
public class StartSurfaceToolbarCoordinator {
    private final StartSurfaceToolbarMediator mToolbarMediator;
    private final ViewStub mStub;
    private final PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private StartSurfaceToolbarView mView;
    private TabModelSelector mTabModelSelector;
    private IncognitoSwitchCoordinator mIncognitoSwitchCoordinator;
    private TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;
    private TabSwitcherButtonView mTabSwitcherButtonView;
    private TabCountProvider mTabCountProvider;
    private ThemeColorProvider mThemeColorProvider;
    private OnClickListener mTabSwitcherClickListener;
    private OnLongClickListener mTabSwitcherLongClickListener;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private CallbackController mCallbackController = new CallbackController();

    StartSurfaceToolbarCoordinator(ViewStub startSurfaceToolbarStub,
            UserEducationHelper userEducationHelper,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ObservableSupplier<Boolean> identityDiscStateSupplier, ThemeColorProvider provider,
            MenuButtonCoordinator menuButtonCoordinator,
            Supplier<ButtonData> identityDiscButtonSupplier) {
        mStub = startSurfaceToolbarStub;

        layoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));

        mPropertyModel =
                new PropertyModel.Builder(StartSurfaceToolbarProperties.ALL_KEYS)
                        .with(StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE,
                                !StartSurfaceConfiguration
                                                .START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB
                                                .getValue()
                                        && !StartSurfaceConfiguration
                                                    .START_SURFACE_HIDE_INCOGNITO_SWITCH.getValue())
                        .with(StartSurfaceToolbarProperties.IN_START_SURFACE_MODE, false)
                        .with(StartSurfaceToolbarProperties.MENU_IS_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IS_VISIBLE, true)
                        .build();

        // START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB and START_SURFACE_SHOW_STACK_TAB_SWITCHER
        // should not be both true.
        assert !(StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB.getValue()
                && StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue());
        mToolbarMediator = new StartSurfaceToolbarMediator(mPropertyModel,
                (iphCommandBuilder)
                        -> {
                    // TODO(crbug.com/865801): Replace the null check with an assert after fixing or
                    // removing the ShareButtonControllerTest that necessitated it.
                    if (mView == null) return;
                    userEducationHelper.requestShowIPH(
                            iphCommandBuilder.setAnchorView(mView.getIdentityDiscView()).build());
                },
                StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB.getValue(),
                StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH.getValue(),
                StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue(),
                menuButtonCoordinator, identityDiscStateSupplier, identityDiscButtonSupplier);

        mThemeColorProvider = provider;
        mMenuButtonCoordinator = menuButtonCoordinator;
    }

    /**
     * Cleans up any code and removes observers as necessary.
     */
    void destroy() {
        mToolbarMediator.destroy();
        if (mIncognitoSwitchCoordinator != null) mIncognitoSwitchCoordinator.destroy();
        if (mTabSwitcherButtonCoordinator != null) mTabSwitcherButtonCoordinator.destroy();
        if (mMenuButtonCoordinator != null) {
            mMenuButtonCoordinator.destroy();
            mMenuButtonCoordinator = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        mTabSwitcherButtonCoordinator = null;
        mTabSwitcherButtonView = null;
        mTabCountProvider = null;
        mThemeColorProvider = null;
        mTabSwitcherClickListener = null;
        mTabSwitcherLongClickListener = null;
    }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mToolbarMediator.setOnNewTabClickHandler(listener);
    }

    /**
     * Sets the current {@Link TabModelSelector} so the toolbar can pass it into buttons that need
     * access to it.
     */
    void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        mToolbarMediator.setTabModelSelector(selector);
    }

    /**
     * Called when Start Surface mode is entered or exited.
     * @param inStartSurfaceMode Whether or not start surface mode should be shown or hidden.
     */
    void setStartSurfaceMode(boolean inStartSurfaceMode) {
        if (!isInflated()) {
            inflate();
        }
        mToolbarMediator.setStartSurfaceMode(inStartSurfaceMode);
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mToolbarMediator.setIncognitoStateProvider(provider);
    }

    /**
     * Called when accessibility status changes.
     * @param enabled whether accessibility status is enabled.
     */
    void onAccessibilityStatusChanged(boolean enabled) {
        mToolbarMediator.onAccessibilityStatusChanged(enabled);
    }

    /**
     * @param layoutStateProvider The {@link LayoutStateProvider} to observe layout state changes.
     */
    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        mToolbarMediator.setLayoutStateProvider(layoutStateProvider);
    }

    /**
     * @param tabCountProvider The {@link TabCountProvider} to update the tab switcher button.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.setTabCountProvider(tabCountProvider);
        } else {
            mTabCountProvider = tabCountProvider;
        }
    }

    /**
     * @param onClickListener The {@link OnClickListener} for the tab switcher button.
     */
    void setTabSwitcherListener(OnClickListener onClickListener) {
        if (mTabSwitcherButtonCoordinator != null) {
            mTabSwitcherButtonCoordinator.setTabSwitcherListener(onClickListener);
        } else {
            mTabSwitcherClickListener = onClickListener;
        }
    }

    /**
     * @param listener The {@link OnLongClickListener} for the tab switcher button.
     */
    void setOnTabSwitcherLongClickHandler(OnLongClickListener listener) {
        if (mTabSwitcherButtonView != null) {
            mTabSwitcherButtonView.setOnLongClickListener(listener);
        } else {
            mTabSwitcherLongClickListener = listener;
        }
    }

    /**
     * Called when start surface state is changed.
     * @param newState The new {@link StartSurfaceState}.
     * @param shouldShowStartSurfaceToolbar Whether or not should show start surface toolbar.
     */
    void onStartSurfaceStateChanged(
            @StartSurfaceState int newState, boolean shouldShowStartSurfaceToolbar) {
        mToolbarMediator.onStartSurfaceStateChanged(newState, shouldShowStartSurfaceToolbar);
    }

    /**
     * Triggered when the offset of start surface header view is changed.
     * @param verticalOffset The start surface header view's offset.
     */
    void onStartSurfaceHeaderOffsetChanged(int verticalOffset) {
        mToolbarMediator.onStartSurfaceHeaderOffsetChanged(verticalOffset);
    }

    /**
     * @param toolbarHeight The height of start surface toolbar.
     * @return Whether or not toolbar container view should be hidden.
     */
    boolean shouldHideToolbarContainer(int toolbarHeight) {
        return mToolbarMediator.shouldHideToolbarContainer(toolbarHeight);
    }

    void onNativeLibraryReady() {
        // It is possible that the {@link mIncognitoSwitchCoordinator} isn't created because
        // inflate() is called when the native library isn't ready. So create it now.
        if (isInflated()) {
            assert mTabModelSelector != null;
            maybeCreateIncognitoSwitchCoordinator();
        }
        mToolbarMediator.onNativeLibraryReady();
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        mToolbarMediator.setNewTabButtonHighlight(highlight);
    }

    private void inflate() {
        mStub.setLayoutResource(R.layout.start_top_toolbar);
        mView = (StartSurfaceToolbarView) mStub.inflate();
        mMenuButtonCoordinator.setMenuButton(mView.findViewById(R.id.menu_button_wrapper));
        mMenuButtonCoordinator.setVisibility(
                mPropertyModel.get(StartSurfaceToolbarProperties.MENU_IS_VISIBLE));
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mView, StartSurfaceToolbarViewBinder::bind);
        if (LibraryLoader.getInstance().isInitialized()) {
            maybeCreateIncognitoSwitchCoordinator();
        }

        if (StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue()) {
            mTabSwitcherButtonView = mView.findViewById(R.id.start_tab_switcher_button);
            if (mTabSwitcherLongClickListener != null) {
                mTabSwitcherButtonView.setOnLongClickListener(mTabSwitcherLongClickListener);
                mTabSwitcherLongClickListener = null;
            }
            mTabSwitcherButtonCoordinator =
                    new TabSwitcherButtonCoordinator(mTabSwitcherButtonView);
            mTabSwitcherButtonCoordinator.setThemeColorProvider(mThemeColorProvider);
            mTabSwitcherButtonView.setVisibility(View.VISIBLE);
            if (mTabCountProvider != null) {
                mTabSwitcherButtonCoordinator.setTabCountProvider(mTabCountProvider);
                mTabCountProvider = null;
            }
            if (mTabSwitcherClickListener != null) {
                mTabSwitcherButtonCoordinator.setTabSwitcherListener(mTabSwitcherClickListener);
                mTabSwitcherClickListener = null;
            }
        }
    }

    private void maybeCreateIncognitoSwitchCoordinator() {
        if (mIncognitoSwitchCoordinator != null || mTabModelSelector == null) {
            return;
        }

        if (IncognitoUtils.isIncognitoModeEnabled()
                && !StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER.getValue()) {
            mIncognitoSwitchCoordinator = new IncognitoSwitchCoordinator(mView, mTabModelSelector);
        }
    }

    private boolean isInflated() {
        return mView != null;
    }

    @VisibleForTesting
    public IncognitoSwitchCoordinator getIncognitoSwitchCoordinatorForTesting() {
        return mIncognitoSwitchCoordinator;
    }
}
