// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.LinearLayoutManager;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTabsFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.ui.base.Clipboard;

import java.util.List;

/**
 * Combines and manages the different UI components of browsing history.
 */
public class HistoryManager implements OnMenuItemClickListener, SelectionObserver<HistoryItem>,
                                       SearchDelegate, SnackbarController,
                                       HistoryContentManager.Observer {
    private static final String METRICS_PREFIX = "Android.HistoryPage.";

    // Keep consistent with the UMA constants on the WebUI history page (history/constants.js).
    private static final int UMA_MAX_BUCKET_VALUE = 1000;
    private static final int UMA_MAX_SUBSET_BUCKET_VALUE = 100;

    // TODO(msramek): The WebUI counterpart computes the bucket count by
    // dividing by 10 until it gets under 100, reaching 10 for both
    // UMA_MAX_BUCKET_VALUE and UMA_MAX_SUBSET_BUCKET_VALUE, and adds +1
    // for overflow. How do we keep that in sync with this code?
    private static final int UMA_BUCKET_COUNT = 11;

    private final Activity mActivity;
    private final boolean mIsIncognito;
    private final boolean mIsSeparateActivity;
    private SelectableListLayout<HistoryItem> mSelectableListLayout;
    private HistoryContentManager mContentManager;
    private SelectionDelegate<HistoryItem> mSelectionDelegate;
    private HistoryManagerToolbar mToolbar;
    private TextView mEmptyView;
    private final SnackbarManager mSnackbarManager;

    private boolean mIsSearching;

    /**
     * Creates a new HistoryManager.
     * @param activity The Activity associated with the HistoryManager.
     * @param isSeparateActivity Whether the history UI will be shown in a separate activity than
     *                           the main Chrome activity.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param isIncognito Whether the incognito tab model is currently selected.
     * @param tabCreatorManager Allows creation of tabs in different models, null if the history UI
     *                          will be shown in a separate activity.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *                    separate activity.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public HistoryManager(@NonNull Activity activity, boolean isSeparateActivity,
            @NonNull SnackbarManager snackbarManager, boolean isIncognito,
            @Nullable TabCreatorManager tabCreatorManager, @Nullable Supplier<Tab> tabSupplier) {
        mActivity = activity;
        mIsSeparateActivity = isSeparateActivity;
        mSnackbarManager = snackbarManager;
        mIsIncognito = isIncognito;

        recordUserAction("Show");
        // If incognito placeholder is shown, we don't need to create History UI elements.
        if (shouldShowIncognitoPlaceholder()) {
            return;
        }

        // 1. Create selectable components.
        mSelectableListLayout =
                (SelectableListLayout<HistoryItem>) LayoutInflater.from(activity).inflate(
                        R.layout.history_main, null);
        mSelectionDelegate = new SelectionDelegate<>();
        mSelectionDelegate.addObserver(this);

        // 2. Create HistoryContentManager and initialize recycler view.
        boolean shouldShowInfoHeader = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, true);
        mContentManager = new HistoryContentManager(mActivity, this, isSeparateActivity,
                isIncognito, shouldShowInfoHeader, /* shouldShowClearData */ true,
                /* hostName */ null, mSelectionDelegate, tabCreatorManager, tabSupplier);
        mSelectableListLayout.initializeRecyclerView(
                mContentManager.getAdapter(), mContentManager.getRecyclerView());

        // 3. Initialize toolbar.
        mToolbar = (HistoryManagerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.history_toolbar, mSelectionDelegate, R.string.menu_history,
                R.id.normal_menu_group, R.id.selection_mode_menu_group, this, true,
                isSeparateActivity);
        mToolbar.setManager(this);
        mToolbar.initializeSearchView(this, R.string.history_manager_search, R.id.search_menu_id);
        mToolbar.setInfoMenuItem(R.id.info_menu_id);
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());

        // 4. Width constrain the SelectableListLayout.
        mSelectableListLayout.configureWideDisplayStyle();

        // 5. Initialize empty view.
        mEmptyView = mSelectableListLayout.initializeEmptyView(
                R.string.history_manager_empty, R.string.history_manager_no_results);

        // 6. Load items.
        mContentManager.initialize();
    }

    /**
     * @return Whether the history manager UI is displayed in a separate activity than the main
     *         Chrome activity.
     */
    public boolean isDisplayedInSeparateActivity() {
        return mIsSeparateActivity;
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        mToolbar.hideOverflowMenu();

        if (item.getItemId() == R.id.close_menu_id && isDisplayedInSeparateActivity()) {
            mActivity.finish();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_open_in_new_tab) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), false);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_copy_link) {
            recordUserActionWithOptionalSearch("CopyLink");
            Clipboard.getInstance().setText(
                    mSelectionDelegate.getSelectedItemsAsList().get(0).getUrl().getSpec());
            mSelectionDelegate.clearSelection();
            Snackbar snackbar = Snackbar.make(mActivity.getString(R.string.copied), this,
                    Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_HISTORY_LINK_COPIED);
            mSnackbarManager.showSnackbar(snackbar);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_open_in_incognito) {
            openItemsInNewTabs(mSelectionDelegate.getSelectedItemsAsList(), true);
            mSelectionDelegate.clearSelection();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            recordUserActionWithOptionalSearch("RemoveSelected");

            int numItemsRemoved = 0;
            HistoryItem lastItemRemoved = null;
            for (HistoryItem historyItem : mSelectionDelegate.getSelectedItems()) {
                mContentManager.markItemForRemoval(historyItem);
                numItemsRemoved++;
                lastItemRemoved = historyItem;
            }

            mContentManager.removeItems();
            mSelectionDelegate.clearSelection();

            if (numItemsRemoved == 1) {
                assert lastItemRemoved != null;
                mContentManager.announceItemRemoved(lastItemRemoved);
            } else if (numItemsRemoved > 1) {
                mContentManager.getRecyclerView().announceForAccessibility(mActivity.getString(
                        R.string.multiple_history_items_deleted, numItemsRemoved));
            }

            return true;
        } else if (item.getItemId() == R.id.search_menu_id) {
            mContentManager.removeHeader();
            mToolbar.showSearchView();
            mSelectableListLayout.onStartSearch();
            recordUserAction("Search");
            mIsSearching = true;
            return true;
        } else if (item.getItemId() == R.id.info_menu_id) {
            boolean shouldShowInfoHeader =
                    !mContentManager.getShouldShowPrivacyDisclaimersIfAvailable();
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, shouldShowInfoHeader);
            mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeader);
            mContentManager.updatePrivacyDisclaimers(shouldShowInfoHeader);
        }
        return false;
    }

    /**
     * @return The view that shows the main browsing history UI.
     */
    public ViewGroup getView() {
        return shouldShowIncognitoPlaceholder() ? getIncognitoHistoryPlaceholderView()
                                                : mSelectableListLayout;
    }

    private boolean shouldShowIncognitoPlaceholder() {
        return mIsIncognito
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.UPDATE_HISTORY_ENTRY_POINTS_IN_INCOGNITO);
    }

    /**
     * @return The placeholder view to be shown instead of history UI in incognito mode.
     */
    private ViewGroup getIncognitoHistoryPlaceholderView() {
        ViewGroup placeholderView = (ViewGroup) LayoutInflater.from(mActivity).inflate(
                R.layout.incognito_history_placeholder, null);
        ImageButton dismissButton =
                placeholderView.findViewById(R.id.close_history_placeholder_button);
        if (mIsSeparateActivity) {
            dismissButton.setOnClickListener(v -> mActivity.finish());
        } else {
            dismissButton.setVisibility(View.GONE);
        }
        placeholderView.setFocusable(true);
        placeholderView.setFocusableInTouchMode(true);
        return placeholderView;
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    public void onDestroyed() {
        if (shouldShowIncognitoPlaceholder()) {
            // If Incognito placeholder is shown no need to call any destroy method.
            return;
        }
        mSelectableListLayout.onDestroyed();
        mContentManager.onDestroyed();
    }

    /**
     * Called when the user presses the back key. This is only going to be called
     * when the history UI is shown in a separate activity rather inside a tab.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
        if (shouldShowIncognitoPlaceholder()) {
            // If Incognito placeholder is shown, the back press should handled by HistoryActivity.
            return false;
        }
        return mSelectableListLayout.onBackPressed();
    }

    @Override
    public void onSearchTextChanged(String query) {
        mContentManager.search(query);
    }

    @Override
    public void onEndSearch() {
        mContentManager.onEndSearch();
        mSelectableListLayout.onEndSearch();
        mIsSearching = false;
    }

    /** @return The SelectableListLayout that displays HistoryItems. */
    public SelectableListLayout<HistoryItem> getSelectableListLayout() {
        return mSelectableListLayout;
    }

    private void openItemsInNewTabs(List<HistoryItem> items, boolean isIncognito) {
        recordUserActionWithOptionalSearch("OpenSelected" + (isIncognito ? "Incognito" : ""));

        for (HistoryItem item : items) {
            mContentManager.openUrl(item.getUrl(), isIncognito, true);
            recordOpenedItemMetrics(item);
        }
    }

    /**
     * @param action The user action string to record.
     */
    static void recordUserAction(String action) {
        RecordUserAction.record(METRICS_PREFIX + action);
    }

    /**
     * Records the user action with "Search" prepended if the user is currently searching.
     * @param action The user action string to record.
     */
    void recordUserActionWithOptionalSearch(String action) {
        recordUserAction((mIsSearching ? "Search." : "") + action);
    }

    /**
     * Records metrics about the age of an opened history |item|.
     * @param item The item that has been opened.
     */
    private void recordOpenedItemMetrics(HistoryItem item) {
        int ageInDays = 1
                + (int) ((System.currentTimeMillis() - item.getTimestamp())
                        / 1000 /* s/ms */ / 60 /* m/s */ / 60 /* h/m */ / 24 /* d/h */);

        RecordHistogram.recordCustomCountHistogram("HistoryPage.ClickAgeInDays",
                Math.min(ageInDays, UMA_MAX_BUCKET_VALUE), 1, UMA_MAX_BUCKET_VALUE,
                UMA_BUCKET_COUNT);

        if (ageInDays <= UMA_MAX_SUBSET_BUCKET_VALUE) {
            RecordHistogram.recordCustomCountHistogram("HistoryPage.ClickAgeInDaysSubset",
                    ageInDays, 1, UMA_MAX_SUBSET_BUCKET_VALUE, UMA_BUCKET_COUNT);
        }
    }

    private void recordClearBrowsingDataMetric() {
        @BrowserProfileType
        int type = mIsIncognito ? BrowserProfileType.INCOGNITO : BrowserProfileType.REGULAR;
        RecordHistogram.recordEnumeratedHistogram(
                METRICS_PREFIX + "ClearBrowsingData.PerProfileType", type,
                BrowserProfileType.MAX_VALUE + 1);
    }

    /**
     * @return True if info menu item should be shown on history toolbar, false otherwise.
     */
    boolean shouldShowInfoButton() {
        LinearLayoutManager layoutManager =
                (LinearLayoutManager) mContentManager.getRecyclerView().getLayoutManager();
        // Before the RecyclerView binds its items, LinearLayoutManager#firstVisibleItemPosition()
        // returns {@link RecyclerView#NO_POSITION}. If #findVisibleItemPosition() returns
        // NO_POSITION, the current adapter position should not prevent the info button from being
        // displayed if all of the other criteria is met. See crbug.com/756249#c3.
        boolean firstAdapterItemScrolledOff = layoutManager.findFirstVisibleItemPosition() > 0;

        return !firstAdapterItemScrolledOff && mContentManager.hasPrivacyDisclaimers()
                && mContentManager.getItemCount() > 0 && !mToolbar.isSearching()
                && !mSelectionDelegate.isSelectionEnabled();
    }

    /**
     * @return True if the available privacy disclaimers should be shown.
     * Note that this may return true even if there are currently no privacy disclaimers.
     */
    boolean shouldShowInfoHeaderIfAvailable() {
        return mContentManager.getShouldShowPrivacyDisclaimersIfAvailable();
    }

    @Override
    public void onSelectionStateChange(List<HistoryItem> selectedItems) {
        mContentManager.setSelectionActive(mSelectionDelegate.isSelectionEnabled());
    }

    @Override
    public void onAction(Object actionData) {
        // Handler for the link copied snackbar. Do nothing.
    }

    @Override
    public void onDismissNoAction(Object actionData) {
        // Handler for the link copied snackbar. Do nothing.
    }

    // HistoryContentManager.Observer
    @Override
    public void onScrolledCallback(boolean loadedMore) {
        // Show info button if available if first visible position is close to info header;
        // otherwise hide info button.
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
        if (loadedMore) {
            recordUserActionWithOptionalSearch("LoadMoreOnScroll");
        }
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemClicked(HistoryItem item) {
        recordUserActionWithOptionalSearch("OpenItem");
        recordOpenedItemMetrics(item);
    }

    // HistoryContentManager.Observer
    @Override
    public void onItemRemoved(HistoryItem item) {
        recordUserActionWithOptionalSearch("RemoveItem");
        if (mSelectionDelegate.isItemSelected(item)) {
            mSelectionDelegate.toggleSelectionForItem(item);
        }
    }

    // HistoryContentManager.Observer
    @Override
    public void onClearBrowsingDataClicked() {
        // Opens the clear browsing data preference.
        recordUserAction("ClearBrowsingData");
        recordClearBrowsingDataMetric();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(mActivity, ClearBrowsingDataTabsFragment.class);
    }

    // HistoryContentManager.Observer
    @Override
    public void onPrivacyDisclaimerHasChanged() {
        mToolbar.updateInfoMenuItem(shouldShowInfoButton(), shouldShowInfoHeaderIfAvailable());
    }

    // HistoryContentManager.Observer
    @Override
    public void onUserAccountStateChanged() {
        mToolbar.onSignInStateChange();
    }

    @VisibleForTesting
    TextView getEmptyViewForTests() {
        return mEmptyView;
    }

    @VisibleForTesting
    public HistoryContentManager getContentManagerForTests() {
        return mContentManager;
    }

    @VisibleForTesting
    SelectionDelegate<HistoryItem> getSelectionDelegateForTests() {
        return mSelectionDelegate;
    }

    @VisibleForTesting
    HistoryManagerToolbar getToolbarForTests() {
        return mToolbar;
    }
}
