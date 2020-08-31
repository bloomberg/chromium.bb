// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfoControllerDelegate;
import org.chromium.chrome.browser.page_info.ChromePermissionParamsListBuilderDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusViewCoordinator implements View.OnClickListener, UrlTextChangeListener {
    private final StatusView mStatusView;
    private final StatusMediator mMediator;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private ToolbarDataProvider mToolbarDataProvider;
    private boolean mUrlHasFocus;

    /**
     * Creates a new StatusViewCoordinator.
     * @param isTablet Whether the UI is shown on a tablet.
     * @param statusView The status view, used to supply and manipulate child views.
     * @param urlBarEditingTextStateProvider The url coordinator.
     */
    public StatusViewCoordinator(boolean isTablet, StatusView statusView,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider) {
        mIsTablet = isTablet;
        mStatusView = statusView;

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(mModel, mStatusView, new StatusViewBinder());

        Runnable forceModelViewReconciliationRunnable = () -> {
            final View securityIconView = getSecurityIconView();
            mStatusView.setAlpha(1f);
            securityIconView.setAlpha(mModel.get(StatusProperties.STATUS_ICON_ALPHA));
            securityIconView.setVisibility(
                    mModel.get(StatusProperties.SHOW_STATUS_ICON) ? View.VISIBLE : View.GONE);
        };
        mMediator = new StatusMediator(mModel, mStatusView.getResources(), mStatusView.getContext(),
                urlBarEditingTextStateProvider, isTablet, forceModelViewReconciliationRunnable);

        Resources res = mStatusView.getResources();
        mMediator.setUrlMinWidth(res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_start_icon_width)
                + (res.getDimensionPixelSize(R.dimen.location_bar_lateral_padding) * 2));

        mMediator.setSeparatorFieldMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer));

        mMediator.setVerboseStatusTextMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width));
    }

    /**
     * Provides data and state for the toolbar component.
     * @param toolbarDataProvider The data provider.
     */
    public void setToolbarDataProvider(ToolbarDataProvider toolbarDataProvider) {
        mToolbarDataProvider = toolbarDataProvider;
        mMediator.setToolbarCommonPropertiesModel(mToolbarDataProvider);
        mStatusView.setToolbarCommonPropertiesModel(mToolbarDataProvider);
        // Update status immediately after receiving the data provider to avoid initial presence
        // glitch on tablet devices. This glitch would be typically seen upon launch of app, right
        // before the landing page is presented to the user.
        updateStatusIcon();
    }

    /**
     * Signals that native initialization has completed.
     */
    public void onNativeInitialized() {
        mMediator.setStatusClickListener(this);
    }

    /**
     * @param urlHasFocus Whether the url currently has focus.
     */
    public void onUrlFocusChange(boolean urlHasFocus) {
        mMediator.setUrlHasFocus(urlHasFocus);
        mUrlHasFocus = urlHasFocus;
        updateVerboseStatusVisibility();
    }

    /** @param urlHasFocus Whether the url currently has focus. */
    public void onUrlAnimationFinished(boolean urlHasFocus) {
        mMediator.setUrlAnimationFinished(urlHasFocus);
    }

    /** @param show Whether the status icon should be VISIBLE, otherwise GONE. */
    public void setStatusIconShown(boolean show) {
        mMediator.setStatusIconShown(show);
    }

    /**
     * Set the url focus change percent.
     * @param percent The current focus percent.
     */
    public void setUrlFocusChangePercent(float percent) {
        mMediator.setUrlFocusChangePercent(percent);
    }

    /**
     * @param useDarkColors Whether dark colors should be for the status icon and text.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mMediator.setUseDarkColors(useDarkColors);

        // TODO(ender): remove this once icon selection has complete set of
        // corresponding properties (for tinting etc).
        updateStatusIcon();
    }

    /**
     * @param incognitoBadgeVisible Whether or not the incognito badge is visible.
     */
    public void setIncognitoBadgeVisibility(boolean incognitoBadgeVisible) {
        mMediator.setIncognitoBadgeVisibility(incognitoBadgeVisible);
    }

    /**
     * @param modalDialogManagerSupplier A supplier for {@link ModalDialogManager} used
     *         to display a dialog.
     */
    public void setModalDialogManagerSupplier(
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    /**
     * Updates the security icon displayed in the LocationBar.
     */
    public void updateStatusIcon() {
        mMediator.setSecurityIconResource(mToolbarDataProvider.getSecurityIconResource(mIsTablet));
        mMediator.setSecurityIconTint(mToolbarDataProvider.getSecurityIconColorStateList());
        mMediator.setSecurityIconDescription(
                mToolbarDataProvider.getSecurityIconContentDescriptionResourceId());

        // TODO(ender): drop these during final cleanup round.
        updateVerboseStatusVisibility();
    }

    /**
     * @return The view displaying the security icon.
     */
    public View getSecurityIconView() {
        return mStatusView.getSecurityButton();
    }

    /**
     * @return Whether the security button is currently being displayed.
     */
    @VisibleForTesting
    public boolean isSecurityButtonShown() {
        return mMediator.isSecurityButtonShown();
    }

    /** @return The ID of the drawable currently shown in the security icon. */
    @DrawableRes
    public int getSecurityIconResourceIdForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) == null
                ? 0
                : mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting();
    }

    /** @return The icon identifier used for custom resources. */
    public String getSecurityIconIdentifierForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) == null
                ? null
                : mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconIdentifierForTesting();
    }

    /**
     * Update visibility of the verbose status based on the button type and focus state of the
     * omnibox.
     */
    private void updateVerboseStatusVisibility() {
        // TODO(ender): turn around logic for ToolbarDataProvider to offer
        // notifications rather than polling for these attributes.
        mMediator.setPageSecurityLevel(mToolbarDataProvider.getSecurityLevel());
        mMediator.setPageIsOffline(mToolbarDataProvider.isOfflinePage());
        mMediator.setPageIsPreview(mToolbarDataProvider.isPreview());
    }

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus) return;

        if (!mToolbarDataProvider.hasTab()
                || mToolbarDataProvider.getTab().getWebContents() == null) {
            return;
        }

        Tab tab = mToolbarDataProvider.getTab();
        WebContents webContents = tab.getWebContents();
        Activity activity = TabUtils.getActivity(tab);
        PageInfoController.show(activity, webContents, null,
                PageInfoController.OpenedFromSource.TOOLBAR,
                new ChromePageInfoControllerDelegate(activity, webContents,
                        mModalDialogManagerSupplier,
                        /*offlinePageLoadUrlDelegate=*/
                        new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab)),
                new ChromePermissionParamsListBuilderDelegate());
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused.
     * This value is used to determine whether the verbose status text should be visible.
     * @param width The unfocused location bar width.
     */
    public void setUnfocusedLocationBarWidth(int width) {
        mMediator.setUnfocusedLocationBarWidth(width);
    }

    /**
     * Toggle animation of icon changes.
     */
    public void setShouldAnimateIconChanges(boolean shouldAnimate) {
        mMediator.setAnimationsEnabled(shouldAnimate);
    }

    /**
     * Specify whether URL should present icons when focused.
     */
    public void setShowIconsWhenUrlFocused(boolean showIconsWithUrlFocused) {
        mMediator.setShowIconsWhenUrlFocused(showIconsWithUrlFocused);
    }

    /**
     * Specify whether suggestion for URL bar is a search action.
     */
    public void setFirstSuggestionIsSearchType(boolean firstSuggestionIsSearchQuery) {
        mMediator.setFirstSuggestionIsSearchType(firstSuggestionIsSearchQuery);
    }

    public void setIncognitoStateProvider(IncognitoStateProvider incognitoStateProvider) {
        mMediator.setIncognitoStateProvider(incognitoStateProvider);
    }

    /**
     * Update information required to display the search engine icon.
     */
    public void updateSearchEngineStatusIcon(boolean shouldShowSearchEngineLogo,
            boolean isSearchEngineGoogle, String searchEngineUrl) {
        mMediator.updateSearchEngineStatusIcon(
                shouldShowSearchEngineLogo, isSearchEngineGoogle, searchEngineUrl);
    }

    /**
     * @return Width of the status icon including start/end margins.
     */
    public int getStatusIconWidth() {
        return mStatusView.getStatusIconWidth();
    }

    @Override
    public void onTextChanged(String textWithoutAutocomplete, String textWithAutocomplete) {
        mMediator.onTextChanged(textWithoutAutocomplete);
    }
}
