// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.annotation.DrawableRes;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.v4.util.ArraySet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ListView;

import org.chromium.base.CollectionUtil;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataTab;
import org.chromium.chrome.browser.browsing_data.CookieOrCacheDeletionChoice;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.historyreport.AppIndexingReporter;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ClearBrowsingDataCheckBoxPreference;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.SpinnerPreference;
import org.chromium.chrome.browser.preferences.privacy.BrowsingDataCounterBridge.BrowsingDataCounterCallback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Preference screen that allows the user to clear browsing data.
 * The user can choose which types of data to clear (history, cookies, etc), and the time range
 * from which to clear data.
 */
public abstract class ClearBrowsingDataPreferences extends PreferenceFragment
        implements BrowsingDataBridge.OnClearBrowsingDataListener,
                   Preference.OnPreferenceClickListener, Preference.OnPreferenceChangeListener {
    /**
     * Represents a single item in the dialog.
     */
    private static class Item implements BrowsingDataCounterCallback,
                                         Preference.OnPreferenceClickListener {
        private static final int MIN_DP_FOR_ICON = 360;
        private final ClearBrowsingDataPreferences mParent;
        private final @DialogOption int mOption;
        private final ClearBrowsingDataCheckBoxPreference mCheckbox;
        private BrowsingDataCounterBridge mCounter;
        private boolean mShouldAnnounceCounterResult;

        public Item(Context context, ClearBrowsingDataPreferences parent, @DialogOption int option,
                ClearBrowsingDataCheckBoxPreference checkbox, boolean selected, boolean enabled) {
            super();
            mParent = parent;
            mOption = option;
            mCheckbox = checkbox;
            mCounter = new BrowsingDataCounterBridge(this,
                    ClearBrowsingDataPreferences.getDataType(mOption), mParent.getPreferenceType());

            mCheckbox.setOnPreferenceClickListener(this);
            mCheckbox.setEnabled(enabled);
            mCheckbox.setChecked(selected);

            int dp = mParent.getResources().getConfiguration().smallestScreenWidthDp;
            if (dp >= MIN_DP_FOR_ICON) {
                mCheckbox.setIcon(PreferenceUtils.getTintedIcon(
                        context, ClearBrowsingDataPreferences.getIcon(option)));
            }
        }

        public void destroy() {
            mCounter.destroy();
        }

        public @DialogOption int getOption() {
            return mOption;
        }

        public boolean isSelected() {
            return mCheckbox.isChecked();
        }

        @Override
        public boolean onPreferenceClick(Preference preference) {
            assert mCheckbox == preference;

            mParent.updateButtonState();
            mShouldAnnounceCounterResult = true;
            PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                    ClearBrowsingDataPreferences.getDataType(mOption), mParent.getPreferenceType(),
                    mCheckbox.isChecked());
            return true;
        }

        @Override
        public void onCounterFinished(String result) {
            mCheckbox.setSummary(result);
            if (mShouldAnnounceCounterResult) {
                mCheckbox.announceForAccessibility(result);
            }
        }

        /**
         * Sets whether the BrowsingDataCounter result should be announced. This is when the counter
         * recalculation was caused by a checkbox state change (as opposed to fragment
         * initialization or time period change).
         */
        public void setShouldAnnounceCounterResult(boolean value) {
            mShouldAnnounceCounterResult = value;
        }
    }

    /**
     * An option to be shown in the time period spiner.
     */
    protected static class TimePeriodSpinnerOption {
        private @TimePeriod int mTimePeriod;
        private String mTitle;

        /**
         * Constructs this time period spinner option.
         * @param timePeriod The time period.
         * @param title The text that will be used to represent this item in the spinner.
         */
        public TimePeriodSpinnerOption(@TimePeriod int timePeriod, String title) {
            mTimePeriod = timePeriod;
            mTitle = title;
        }

        /**
         * @return The time period.
         */
        public @TimePeriod int getTimePeriod() {
            return mTimePeriod;
        }

        @Override
        public String toString() {
            return mTitle;
        }
    }

    @VisibleForTesting
    public static final String PREF_GOOGLE_SUMMARY = "google_summary";
    @VisibleForTesting
    public static final String PREF_GENERAL_SUMMARY = "general_summary";
    private static final String PREF_TIME_RANGE = "time_period_spinner";

    /** The "Clear" button preference. */
    @VisibleForTesting
    public static final String PREF_CLEAR_BUTTON = "clear_button";

    /** The tag used for logging. */
    public static final String TAG = "ClearBrowsingDataPreferences";

    /** The histogram for the dialog about other forms of browsing history. */
    private static final String DIALOG_HISTOGRAM =
            "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing";

    /**
     * Used for the onActivityResult pattern. The value is arbitrary, just to distinguish from other
     * activities that we might be using onActivityResult with as well.
     */
    private static final int IMPORTANT_SITES_DIALOG_CODE = 1;

    private static final int IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT = 20;

    /**
     * The various data types that can be cleared via this screen.
     */
    @IntDef({DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
            DialogOption.CLEAR_CACHE, DialogOption.CLEAR_PASSWORDS, DialogOption.CLEAR_FORM_DATA,
            DialogOption.CLEAR_SITE_SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogOption {
        // Values used in "for" loop below - should start from 0 and can't have gaps, lowest value
        // is additionally used for starting loop.
        // All updates here must also be reflected in {@link #getDataType(int) getDataType}, {@link
        // #getPreferenceKey(int) getPreferenceKey} and {@link #getIcon(int) getIcon}.
        int CLEAR_HISTORY = 0;
        int CLEAR_COOKIES_AND_SITE_DATA = 1;
        int CLEAR_CACHE = 2;
        int CLEAR_PASSWORDS = 3;
        int CLEAR_FORM_DATA = 4;
        int CLEAR_SITE_SETTINGS = 5;
        int NUM_ENTRIES = 6;
    }

    public static final String CLEAR_BROWSING_DATA_FETCHER = "clearBrowsingDataFetcher";

    private OtherFormsOfHistoryDialogFragment mDialogAboutOtherFormsOfBrowsingHistory;

    private ProgressDialog mProgressDialog;
    private Item[] mItems;
    private ClearBrowsingDataFetcher mFetcher;

    // This is the dialog we show to the user that lets them 'uncheck' (or exclude) the above
    // important domains from being cleared.
    private ConfirmImportantSitesDialogFragment mConfirmImportantSitesDialog;

    // Time in ms, when the dialog was created.
    private long mDialogOpened;

    /**
     * @return All available {@link DialogOption} entries.
     */
    protected final static Set<Integer> getAllOptions() {
        assert DialogOption.CLEAR_HISTORY == 0;

        Set<Integer> all = new ArraySet<>();
        for (@DialogOption int i = DialogOption.CLEAR_HISTORY; i < DialogOption.NUM_ENTRIES; i++) {
            all.add(i);
        }
        return all;
    }

    static @BrowsingDataType int getDataType(@DialogOption int type) {
        switch (type) {
            case DialogOption.CLEAR_CACHE:
                return BrowsingDataType.CACHE;
            case DialogOption.CLEAR_COOKIES_AND_SITE_DATA:
                return BrowsingDataType.COOKIES;
            case DialogOption.CLEAR_FORM_DATA:
                return BrowsingDataType.FORM_DATA;
            case DialogOption.CLEAR_HISTORY:
                return BrowsingDataType.HISTORY;
            case DialogOption.CLEAR_PASSWORDS:
                return BrowsingDataType.PASSWORDS;
            case DialogOption.CLEAR_SITE_SETTINGS:
                return BrowsingDataType.SITE_SETTINGS;
            default:
                throw new IllegalArgumentException();
        }
    }

    static String getPreferenceKey(@DialogOption int type) {
        switch (type) {
            case DialogOption.CLEAR_CACHE:
                return "clear_cache_checkbox";
            case DialogOption.CLEAR_COOKIES_AND_SITE_DATA:
                return "clear_cookies_checkbox";
            case DialogOption.CLEAR_FORM_DATA:
                return "clear_form_data_checkbox";
            case DialogOption.CLEAR_HISTORY:
                return "clear_history_checkbox";
            case DialogOption.CLEAR_PASSWORDS:
                return "clear_passwords_checkbox";
            case DialogOption.CLEAR_SITE_SETTINGS:
                return "clear_site_settings_checkbox";
            default:
                throw new IllegalArgumentException();
        }
    }

    static @DrawableRes int getIcon(@DialogOption int type) {
        switch (type) {
            case DialogOption.CLEAR_CACHE:
                return R.drawable.ic_collections_grey;
            case DialogOption.CLEAR_COOKIES_AND_SITE_DATA:
                return R.drawable.permission_cookie;
            case DialogOption.CLEAR_FORM_DATA:
                return R.drawable.ic_edit_24dp;
            case DialogOption.CLEAR_HISTORY:
                return R.drawable.ic_watch_later_24dp;
            case DialogOption.CLEAR_PASSWORDS:
                return R.drawable.ic_vpn_key_grey;
            case DialogOption.CLEAR_SITE_SETTINGS:
                return R.drawable.ic_tv_options_input_settings_rotated_grey;
            default:
                throw new IllegalArgumentException();
        }
    }

    /**
     * @return The currently selected {@link DialogOption} entries.
     */
    protected final Set<Integer> getSelectedOptions() {
        Set<Integer> selected = new ArraySet<>();
        for (Item item : mItems) {
            if (item.isSelected()) selected.add(item.getOption());
        }
        return selected;
    }

    /**
     * @param fetcher A ClearBrowsingDataFetcher.
     */
    public void setClearBrowsingDataFetcher(ClearBrowsingDataFetcher fetcher) {
        mFetcher = fetcher;
    }

    @VisibleForTesting
    public ClearBrowsingDataFetcher getClearBrowsingDataFetcher() {
        return mFetcher;
    }

    /**
     * Notifies subclasses that browsing data is about to be cleared.
     */
    protected void onClearBrowsingData() {}

    /**
     * Requests the browsing data corresponding to the given dialog options to be deleted.
     * @param options The dialog options whose corresponding data should be deleted.
     */
    private void clearBrowsingData(Set<Integer> options, @Nullable String[] blacklistedDomains,
            @Nullable int[] blacklistedDomainReasons, @Nullable String[] ignoredDomains,
            @Nullable int[] ignoredDomainReasons) {
        onClearBrowsingData();
        showProgressDialog();
        Set<Integer> dataTypes = new ArraySet<>();
        for (@DialogOption Integer option : options) {
            dataTypes.add(getDataType(option));
        }

        RecordHistogram.recordMediumTimesHistogram("History.ClearBrowsingData.TimeSpentInDialog",
                SystemClock.elapsedRealtime() - mDialogOpened);

        final @CookieOrCacheDeletionChoice int choice;
        if (dataTypes.contains(BrowsingDataType.COOKIES)) {
            choice = dataTypes.contains(BrowsingDataType.CACHE)
                    ? CookieOrCacheDeletionChoice.BOTH_COOKIES_AND_CACHE
                    : CookieOrCacheDeletionChoice.ONLY_COOKIES;
        } else {
            choice = dataTypes.contains(BrowsingDataType.CACHE)
                    ? CookieOrCacheDeletionChoice.ONLY_CACHE
                    : CookieOrCacheDeletionChoice.NEITHER_COOKIES_NOR_CACHE;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice,
                CookieOrCacheDeletionChoice.MAX_CHOICE_VALUE);

        Object spinnerSelection =
                ((SpinnerPreference) findPreference(PREF_TIME_RANGE)).getSelectedOption();
        @TimePeriod
        int timePeriod = ((TimePeriodSpinnerOption) spinnerSelection).getTimePeriod();
        // TODO(bsazonov): Change integerListToIntArray to handle Collection<Integer>.
        int[] dataTypesArray = CollectionUtil.integerListToIntArray(new ArrayList<>(dataTypes));
        if (blacklistedDomains != null && blacklistedDomains.length != 0) {
            BrowsingDataBridge.getInstance().clearBrowsingDataExcludingDomains(this, dataTypesArray,
                    timePeriod, blacklistedDomains, blacklistedDomainReasons, ignoredDomains,
                    ignoredDomainReasons);
        } else {
            BrowsingDataBridge.getInstance().clearBrowsingData(this, dataTypesArray, timePeriod);
        }

        // Clear all reported entities.
        AppIndexingReporter.getInstance().clearHistory();
    }

    private void dismissProgressDialog() {
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            mProgressDialog.dismiss();
        }
        mProgressDialog = null;
    }

    /**
     * Returns the list of supported {@link DialogOption}.
     */
    abstract protected List<Integer> getDialogOptions();

    /**
     * Returns whether this preference page is a basic or advanced tab in order to use separate
     * preferences.
     */
    abstract protected int getPreferenceType();

    /**
     * Returns the Array of time periods. Options are displayed in the same order as they appear
     * in the array.
     */
    private TimePeriodSpinnerOption[] getTimePeriodSpinnerOptions() {
        Activity activity = getActivity();

        List<TimePeriodSpinnerOption> options = new ArrayList<>();
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_HOUR,
                activity.getString(R.string.clear_browsing_data_tab_period_hour)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_DAY,
                activity.getString(R.string.clear_browsing_data_tab_period_24_hours)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_WEEK,
                activity.getString(R.string.clear_browsing_data_tab_period_7_days)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.FOUR_WEEKS,
                activity.getString(R.string.clear_browsing_data_tab_period_four_weeks)));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CLEAR_OLD_BROWSING_DATA)) {
            options.add(new TimePeriodSpinnerOption(TimePeriod.OLDER_THAN_30_DAYS,
                    activity.getString(
                            R.string.clear_browsing_data_tab_period_older_than_30_days)));
        }
        options.add(new TimePeriodSpinnerOption(TimePeriod.ALL_TIME,
                activity.getString(R.string.clear_browsing_data_tab_period_everything)));
        return options.toArray(new TimePeriodSpinnerOption[0]);
    }

    /**
     * Decides whether a given dialog option should be selected when the dialog is initialized.
     *
     * @param option The option in question.
     * @return boolean Whether the given option should be preselected.
     */
    private boolean isOptionSelectedByDefault(@DialogOption int option) {
        return PrefServiceBridge.getInstance().getBrowsingDataDeletionPreference(
                getDataType(option), getPreferenceType());
    }

    /**
     * Called when clearing browsing data completes.
     * Implements the BrowsingDataBridge.OnClearBrowsingDataListener interface.
     */
    @Override
    public void onBrowsingDataCleared() {
        if (getActivity() == null) return;

        // If the user deleted their browsing history, the dialog about other forms of history
        // is enabled, and it has never been shown before, show it. Note that opening a new
        // DialogFragment is only possible if the Activity is visible.
        //
        // If conditions to show the dialog about other forms of history are not met, just close
        // this preference screen.
        if (MultiWindowUtils.isActivityVisible(getActivity())
                && getSelectedOptions().contains(DialogOption.CLEAR_HISTORY)
                && mFetcher.isDialogAboutOtherFormsOfBrowsingHistoryEnabled()
                && !OtherFormsOfHistoryDialogFragment.wasDialogShown()) {
            mDialogAboutOtherFormsOfBrowsingHistory = new OtherFormsOfHistoryDialogFragment();
            mDialogAboutOtherFormsOfBrowsingHistory.show(getActivity());
            dismissProgressDialog();
            RecordHistogram.recordBooleanHistogram(DIALOG_HISTOGRAM, true);
        } else {
            dismissProgressDialog();
            getActivity().finish();
            RecordHistogram.recordBooleanHistogram(DIALOG_HISTOGRAM, false);
        }
    }

    /**
     * Returns if we should show the important sites dialog. We check to see if
     * <ol>
     * <li>We've fetched the important sites,
     * <li>there are important sites,
     * <li>the feature is enabled, and
     * <li>we have cache or cookies selected.
     * </ol>
     */
    private boolean shouldShowImportantSitesDialog() {
        Set<Integer> selectedOptions = getSelectedOptions();
        if (!selectedOptions.contains(DialogOption.CLEAR_CACHE)
                && !selectedOptions.contains(DialogOption.CLEAR_COOKIES_AND_SITE_DATA)) {
            return false;
        }
        boolean haveImportantSites = mFetcher.getSortedImportantDomains() != null
                && mFetcher.getSortedImportantDomains().length != 0;
        RecordHistogram.recordBooleanHistogram(
                "History.ClearBrowsingData.ImportantDialogShown", haveImportantSites);
        return haveImportantSites;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference.getKey().equals(PREF_CLEAR_BUTTON)) {
            onClearButtonClicked();
            return true;
        }
        return false;
    }

    /**
     * Either shows the important sites dialog or clears browsing data according to the selected
     * options.
     */
    private void onClearButtonClicked() {
        if (shouldShowImportantSitesDialog()) {
            showImportantDialogThenClear();
            return;
        }
        // If sites haven't been fetched, just clear the browsing data regularly rather than
        // waiting to show the important sites dialog.
        clearBrowsingData(getSelectedOptions(), null, null, null, null);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object value) {
        if (preference.getKey().equals(PREF_TIME_RANGE)) {
            // Inform the items that a recalculation is going to happen as a result of the time
            // period change.
            for (Item item : mItems) {
                item.setShouldAnnounceCounterResult(false);
            }

            PrefServiceBridge.getInstance().setBrowsingDataDeletionTimePeriod(
                    getPreferenceType(), ((TimePeriodSpinnerOption) value).getTimePeriod());
            return true;
        }
        return false;
    }

    /**
     * Disable the "Clear" button if none of the options are selected. Otherwise, enable it.
     */
    private void updateButtonState() {
        Button clearButton = (Button) getView().findViewById(R.id.clear_button);
        boolean isEnabled = !getSelectedOptions().isEmpty();
        clearButton.setEnabled(isEnabled);
    }

    /**
     * @return The id of the preference xml that should be displayed.
     */
    private int getPreferenceXmlId() {
        return R.xml.clear_browsing_data_preferences_tab;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            mFetcher = savedInstanceState.getParcelable(CLEAR_BROWSING_DATA_FETCHER);
        }
        mDialogOpened = SystemClock.elapsedRealtime();
        getActivity().setTitle(R.string.clear_browsing_data_title);
        PreferenceUtils.addPreferencesFromResource(this, getPreferenceXmlId());
        List<Integer> options = getDialogOptions();
        mItems = new Item[options.size()];
        for (int i = 0; i < options.size(); i++) {
            @DialogOption int option = options.get(i);
            boolean enabled = true;

            // It is possible to disable the deletion of browsing history.
            if (option == DialogOption.CLEAR_HISTORY
                    && !PrefServiceBridge.getInstance().getBoolean(
                               Pref.ALLOW_DELETING_BROWSER_HISTORY)) {
                enabled = false;
                PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                        getDataType(DialogOption.CLEAR_HISTORY), ClearBrowsingDataTab.BASIC, false);
                PrefServiceBridge.getInstance().setBrowsingDataDeletionPreference(
                        getDataType(DialogOption.CLEAR_HISTORY), ClearBrowsingDataTab.ADVANCED,
                        false);
            }

            mItems[i] = new Item(getActivity(), this, option,
                    (ClearBrowsingDataCheckBoxPreference) findPreference(getPreferenceKey(option)),
                    isOptionSelectedByDefault(option), enabled);
        }

        // Not all checkboxes defined in the layout are necessarily handled by this class
        // or a particular subclass. Hide those that are not.
        Set<Integer> unboundOptions = getAllOptions();
        unboundOptions.removeAll(options);
        for (@DialogOption Integer option : unboundOptions) {
            getPreferenceScreen().removePreference(findPreference(getPreferenceKey(option)));
        }

        // The time range selection spinner.
        SpinnerPreference spinner = (SpinnerPreference) findPreference(PREF_TIME_RANGE);
        TimePeriodSpinnerOption[] spinnerOptions = getTimePeriodSpinnerOptions();
        @TimePeriod
        int selectedTimePeriod = PrefServiceBridge.getInstance().getBrowsingDataDeletionTimePeriod(
                getPreferenceType());
        int spinnerOptionIndex = -1;
        for (int i = 0; i < spinnerOptions.length; ++i) {
            if (spinnerOptions[i].getTimePeriod() == selectedTimePeriod) {
                spinnerOptionIndex = i;
                break;
            }
        }
        assert spinnerOptionIndex != -1;
        spinner.setOptions(spinnerOptions, spinnerOptionIndex);
        spinner.setOnPreferenceChangeListener(this);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // mFetcher acts as a cache for important sites and history data. If the activity gets
        // suspended, we can save the cached data and reuse it when we are activated again.
        outState.putParcelable(CLEAR_BROWSING_DATA_FETCHER, mFetcher);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Replace default preferences view with custom XML that contains a footer.
        View view = inflater.inflate(R.layout.clear_browsing_data_tab_content, container, false);

        Button clearButton = (Button) view.findViewById(R.id.clear_button);
        clearButton.setOnClickListener((View v) -> onClearButtonClicked());

        return view;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        // Now that the dialog's view has been created, update the button state.
        updateButtonState();

        // Remove the dividers between checkboxes, and make sure the individual widgets can be
        // focused.
        ListView view = (ListView) getView().findViewById(android.R.id.list);
        view.setDivider(null);
        view.setItemsCanFocus(true);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        dismissProgressDialog();
        for (Item item : mItems) {
            item.destroy();
        }
    }

    // We either show the dialog, or modify the current one to display our messages.  This avoids
    // a dialog flash.
    private void showProgressDialog() {
        if (getActivity() == null) return;
        mProgressDialog = ProgressDialog.show(getActivity(),
                getActivity().getString(R.string.clear_browsing_data_progress_title),
                getActivity().getString(R.string.clear_browsing_data_progress_message), true,
                false);
    }

    @VisibleForTesting
    ProgressDialog getProgressDialog() {
        return mProgressDialog;
    }

    @VisibleForTesting
    ConfirmImportantSitesDialogFragment getImportantSitesDialogFragment() {
        return mConfirmImportantSitesDialog;
    }

    /**
     * This method shows the important sites dialog. After the dialog is shown, we correctly clear.
     */
    private void showImportantDialogThenClear() {
        mConfirmImportantSitesDialog = ConfirmImportantSitesDialogFragment.newInstance(
                mFetcher.getSortedImportantDomains(), mFetcher.getSortedImportantDomainReasons(),
                mFetcher.getSortedExampleOrigins());
        mConfirmImportantSitesDialog.setTargetFragment(this, IMPORTANT_SITES_DIALOG_CODE);
        mConfirmImportantSitesDialog.show(
                getFragmentManager(), ConfirmImportantSitesDialogFragment.FRAGMENT_TAG);
    }

    /**
     * Used only to access the dialog about other forms of browsing history from tests.
     */
    @VisibleForTesting
    OtherFormsOfHistoryDialogFragment getDialogAboutOtherFormsOfBrowsingHistory() {
        return mDialogAboutOtherFormsOfBrowsingHistory;
    }

    /**
     * This is the callback for the important domain dialog. We should only clear if we get the
     * positive button response.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == IMPORTANT_SITES_DIALOG_CODE && resultCode == Activity.RESULT_OK) {
            // Deselected means that the user is excluding the domain from being cleared.
            String[] deselectedDomains = data.getStringArrayExtra(
                    ConfirmImportantSitesDialogFragment.DESELECTED_DOMAINS_TAG);
            int[] deselectedDomainReasons = data.getIntArrayExtra(
                    ConfirmImportantSitesDialogFragment.DESELECTED_DOMAIN_REASONS_TAG);
            String[] ignoredDomains = data.getStringArrayExtra(
                    ConfirmImportantSitesDialogFragment.IGNORED_DOMAINS_TAG);
            int[] ignoredDomainReasons = data.getIntArrayExtra(
                    ConfirmImportantSitesDialogFragment.IGNORED_DOMAIN_REASONS_TAG);
            if (deselectedDomains != null && mFetcher.getSortedImportantDomains() != null) {
                // mMaxImportantSites is a constant on the C++ side.
                RecordHistogram.recordCustomCountHistogram(
                        "History.ClearBrowsingData.ImportantDeselectedNum",
                        deselectedDomains.length, 1, mFetcher.getMaxImportantSites() + 1,
                        mFetcher.getMaxImportantSites() + 1);
                RecordHistogram.recordCustomCountHistogram(
                        "History.ClearBrowsingData.ImportantIgnoredNum", ignoredDomains.length, 1,
                        mFetcher.getMaxImportantSites() + 1, mFetcher.getMaxImportantSites() + 1);
                // We put our max at 20 instead of 100 to reduce the number of empty buckets (as
                // our maximum denominator is 5).
                RecordHistogram.recordEnumeratedHistogram(
                        "History.ClearBrowsingData.ImportantDeselectedPercent",
                        deselectedDomains.length * IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT
                                / mFetcher.getSortedImportantDomains().length,
                        IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT + 1);
                RecordHistogram.recordEnumeratedHistogram(
                        "History.ClearBrowsingData.ImportantIgnoredPercent",
                        ignoredDomains.length * IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT
                                / mFetcher.getSortedImportantDomains().length,
                        IMPORTANT_SITES_PERCENTAGE_BUCKET_COUNT + 1);
            }
            clearBrowsingData(getSelectedOptions(), deselectedDomains, deselectedDomainReasons,
                    ignoredDomains, ignoredDomainReasons);
        }
    }
}
