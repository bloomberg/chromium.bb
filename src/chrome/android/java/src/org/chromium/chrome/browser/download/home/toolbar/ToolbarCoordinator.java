// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.toolbar;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.metrics.UmaUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.FadingShadow;
import org.chromium.chrome.browser.ui.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.chrome.download.R;

import java.util.List;

/**
 * A top level class to handle various toolbar related functionalities in download home.
 */
public class ToolbarCoordinator implements SelectionObserver<ListItem> {
    /**
     * A delegate to handle various actions taken by user that relate to list items.
     */
    public interface ToolbarListActionDelegate {
        /**
         * Invoked when user taps on the delete button to delete the currently selected items.
         * @return The number of items that were deleted.
         */
        int deleteSelectedItems();

        /**
         * Invoked when user taps on the share button to share the currently selected items.
         * @return The number of items that were shared.
         */
        int shareSelectedItems();

        /**
         * Invoked when user starts a search on download home.
         * @param query The search text on which downloads will be filtered.
         */
        void setSearchQuery(String query);
    }

    /**
     * A delegate to handle various non-list related actions taken by the user on the download home
     * toolbar.
     */
    public interface ToolbarActionDelegate {
        /**
         * Invoked when user taps on close button to close download home.
         */
        void close();

        /**
         * Invoked when user taps on settings menu button to open the download home settings page.
         */
        void openSettings();
    }

    private final ToolbarListActionDelegate mListActionDelegate;
    private final ToolbarActionDelegate mDelegate;

    private final ViewGroup mView;
    private final DownloadHomeToolbar mToolbar;
    private final FadingShadowView mShadow;

    private boolean mShowToolbarShadow;

    private SelectableListToolbar.SearchDelegate mSearchDelegate =
            new SelectableListToolbar.SearchDelegate() {
                @Override
                public void onSearchTextChanged(String query) {
                    mListActionDelegate.setSearchQuery(query);
                }

                @Override
                public void onEndSearch() {
                    mListActionDelegate.setSearchQuery(null);
                    updateShadowVisibility();
                }
            };

    public ToolbarCoordinator(Context context, ToolbarActionDelegate delegate,
            ToolbarListActionDelegate listActionDelegate,
            SelectionDelegate<ListItem> selectionDelegate, boolean hasCloseButton,
            Profile profile) {
        mDelegate = delegate;
        mListActionDelegate = listActionDelegate;

        final boolean isLocationEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE);

        final int normalMenuGroupId =
                isLocationEnabled ? R.id.with_settings_normal_menu_group : R.id.normal_menu_group;
        final int searchMenuId =
                isLocationEnabled ? R.id.with_settings_search_menu_id : R.id.search_menu_id;
        final int closeMenuId =
                isLocationEnabled ? R.id.with_settings_close_menu_id : R.id.close_menu_id;
        final int infoMenuId = R.id.info_menu_id;

        mView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.download_home_toolbar, null);
        mToolbar = mView.findViewById(R.id.download_toolbar);
        mShadow = mView.findViewById(R.id.shadow);

        mToolbar.initialize(selectionDelegate, 0 /* titleResId */, normalMenuGroupId,
                R.id.selection_mode_menu_group, hasCloseButton);
        mToolbar.setOnMenuItemClickListener(this ::onMenuItemClick);

        // TODO(crbug.com/881037): Pass the visible group to the toolbar during initialization.
        mToolbar.getMenu().setGroupVisible(normalMenuGroupId, true);
        mToolbar.initializeSearchView(
                mSearchDelegate, R.string.download_manager_search, searchMenuId);

        if (isLocationEnabled) ToolbarUtils.setupTrackerForDownloadSettingsIPH(mToolbar, profile);

        mShadow.init(ApiCompatibilityUtils.getColor(
                             context.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);

        if (!hasCloseButton) mToolbar.removeMenuItem(closeMenuId);
        mToolbar.removeMenuItem(infoMenuId);
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void destroy() {
        mToolbar.destroy();
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    /**
     * Called to update whether the toolbar should show a shadow.
     * @param show Whether the shadow should be shown.
     */
    public void setShowToolbarShadow(boolean show) {
        if (mShowToolbarShadow == show) return;

        mShowToolbarShadow = show;
        updateShadowVisibility();
    }

    /**
     * Called to enable/disable the search menu button.
     * @param searchEnabled Whether search is currently enabled
     */
    public void setSearchEnabled(boolean searchEnabled) {
        mToolbar.setSearchEnabled(searchEnabled);
    }

    /**
     * Invoked on the event back button was pressed. Gives a chance to this view to run any special
     * handling.
     * @return True if the event was handled, false otherwise.
     */
    public boolean handleBackPressed() {
        if (mToolbar.isSearching()) {
            mToolbar.hideSearchView();
            return true;
        }

        return false;
    }

    // SelectionObserver<ListItem> implementation.
    @Override
    public void onSelectionStateChange(List<ListItem> selectedItems) {
        updateShadowVisibility();
    }

    private boolean onMenuItemClick(MenuItem item) {
        UmaUtils.recordTopMenuAction(item.getItemId());

        if (item.getItemId() == R.id.close_menu_id
                || item.getItemId() == R.id.with_settings_close_menu_id) {
            mDelegate.close();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            int itemsDeleted = mListActionDelegate.deleteSelectedItems();
            UmaUtils.recordTopMenuDeleteCount(itemsDeleted);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_share_menu_id) {
            int itemsShared = mListActionDelegate.shareSelectedItems();
            UmaUtils.recordTopMenuShareCount(itemsShared);
            return true;
        } else if (item.getItemId() == R.id.with_settings_search_menu_id
                || item.getItemId() == R.id.search_menu_id) {
            mToolbar.showSearchView();
            updateShadowVisibility();
            return true;
        } else if (item.getItemId() == R.id.settings_menu_id) {
            mDelegate.openSettings();
            return true;
        } else {
            return false;
        }
    }

    // TODO(shaktisahu): May be merge toolbar shadow logic into the toolbar itself.
    private void updateShadowVisibility() {
        boolean show = mShowToolbarShadow || mToolbar.isSearching();
        mShadow.setVisibility(show ? ViewGroup.VISIBLE : View.GONE);
    }
}
