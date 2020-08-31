// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.chrome.browser.sync.ui.PassphraseCreationDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.browser.sync.ui.PassphraseTypeDialogFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.PassphraseType;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.ButtonCompat;

import java.util.HashSet;
import java.util.Set;

/**
 * Settings fragment to customize Sync options (data types, encryption). Can be accessed from
 * {@link SyncAndServicesSettings}.
 */
public class ManageSyncSettings extends PreferenceFragmentCompat
        implements PassphraseDialogFragment.Listener, PassphraseCreationDialogFragment.Listener,
                   PassphraseTypeDialogFragment.Listener, Preference.OnPreferenceChangeListener,
                   ProfileSyncService.SyncStateChangedListener,
                   SettingsActivity.OnBackPressedListener {
    private static final String IS_FROM_SIGNIN_SCREEN = "ManageSyncSettings.isFromSigninScreen";
    private static final String FRAGMENT_CANCEL_SYNC = "cancel_sync_dialog";

    @VisibleForTesting
    public static final String FRAGMENT_ENTER_PASSPHRASE = "enter_password";
    @VisibleForTesting
    public static final String FRAGMENT_CUSTOM_PASSPHRASE = "custom_password";
    @VisibleForTesting
    public static final String FRAGMENT_PASSPHRASE_TYPE = "password_type";

    @VisibleForTesting
    public static final String PREF_SYNCING_CATEGORY = "syncing_category";
    @VisibleForTesting
    public static final String PREF_SYNC_EVERYTHING = "sync_everything";
    @VisibleForTesting
    public static final String PREF_SYNC_AUTOFILL = "sync_autofill";
    @VisibleForTesting
    public static final String PREF_SYNC_BOOKMARKS = "sync_bookmarks";
    @VisibleForTesting
    public static final String PREF_SYNC_PAYMENTS_INTEGRATION = "sync_payments_integration";
    @VisibleForTesting
    public static final String PREF_SYNC_HISTORY = "sync_history";
    @VisibleForTesting
    public static final String PREF_SYNC_PASSWORDS = "sync_passwords";
    @VisibleForTesting
    public static final String PREF_SYNC_RECENT_TABS = "sync_recent_tabs";
    @VisibleForTesting
    public static final String PREF_SYNC_SETTINGS = "sync_settings";
    @VisibleForTesting
    public static final String PREF_GOOGLE_ACTIVITY_CONTROLS = "google_activity_controls";
    @VisibleForTesting
    public static final String PREF_ENCRYPTION = "encryption";
    @VisibleForTesting
    public static final String PREF_SYNC_MANAGE_DATA = "sync_manage_data";
    @VisibleForTesting
    public static final String PREF_SEARCH_AND_BROWSE_CATEGORY = "search_and_browse_category";

    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";

    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;

    private final ProfileSyncService mProfileSyncService = ProfileSyncService.get();

    private boolean mIsFromSigninScreen;

    private PreferenceCategory mSyncingCategory;

    private ChromeSwitchPreference mSyncEverything;
    private CheckBoxPreference mSyncAutofill;
    private CheckBoxPreference mSyncBookmarks;
    private CheckBoxPreference mSyncPaymentsIntegration;
    private CheckBoxPreference mSyncHistory;
    private CheckBoxPreference mSyncPasswords;
    private CheckBoxPreference mSyncRecentTabs;
    private CheckBoxPreference mSyncSettings;
    // Contains preferences for all sync data types.
    private CheckBoxPreference[] mSyncTypePreferences;

    private Preference mGoogleActivityControls;
    private Preference mSyncEncryption;
    private Preference mManageSyncData;

    private PreferenceCategory mSearchAndBrowseCategory;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;

    private ProfileSyncService.SyncSetupInProgressHandle mSyncSetupInProgressHandle;

    /**
     * Creates an argument bundle for this fragment.
     * @param isFromSigninScreen Whether the screen is started from the sign-in screen.
     */
    public static Bundle createArguments(boolean isFromSigninScreen) {
        Bundle result = new Bundle();
        result.putBoolean(IS_FROM_SIGNIN_SCREEN, isFromSigninScreen);
        return result;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {
        mIsFromSigninScreen =
                IntentUtils.safeGetBoolean(getArguments(), IS_FROM_SIGNIN_SCREEN, false);

        getActivity().setTitle(R.string.manage_sync_title);
        setHasOptionsMenu(true);
        // TODO(https://crbug.com/1063982): Change accessibility text for Advanced Sync Flow.

        SettingsUtils.addPreferencesFromResource(this, R.xml.manage_sync_preferences);

        mSyncingCategory = (PreferenceCategory) findPreference(PREF_SYNCING_CATEGORY);

        mSyncEverything = (ChromeSwitchPreference) findPreference(PREF_SYNC_EVERYTHING);
        mSyncEverything.setOnPreferenceChangeListener(this);

        mSyncAutofill = (CheckBoxPreference) findPreference(PREF_SYNC_AUTOFILL);
        mSyncBookmarks = (CheckBoxPreference) findPreference(PREF_SYNC_BOOKMARKS);
        mSyncPaymentsIntegration =
                (CheckBoxPreference) findPreference(PREF_SYNC_PAYMENTS_INTEGRATION);
        mSyncHistory = (CheckBoxPreference) findPreference(PREF_SYNC_HISTORY);
        mSyncPasswords = (CheckBoxPreference) findPreference(PREF_SYNC_PASSWORDS);
        mSyncRecentTabs = (CheckBoxPreference) findPreference(PREF_SYNC_RECENT_TABS);
        mSyncSettings = (CheckBoxPreference) findPreference(PREF_SYNC_SETTINGS);

        mGoogleActivityControls = findPreference(PREF_GOOGLE_ACTIVITY_CONTROLS);
        mSyncEncryption = findPreference(PREF_ENCRYPTION);
        mSyncEncryption.setOnPreferenceClickListener(
                SyncSettingsUtils.toOnClickListener(this, this::onSyncEncryptionClicked));
        mManageSyncData = findPreference(PREF_SYNC_MANAGE_DATA);
        mManageSyncData.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> SyncSettingsUtils.openSyncDashboard(getActivity())));

        mSyncTypePreferences =
                new CheckBoxPreference[] {mSyncAutofill, mSyncBookmarks, mSyncPaymentsIntegration,
                        mSyncHistory, mSyncPasswords, mSyncRecentTabs, mSyncSettings};
        for (CheckBoxPreference type : mSyncTypePreferences) {
            type.setOnPreferenceChangeListener(this);
        }

        Profile profile = Profile.getLastUsedRegularProfile();
        if (profile.isChild()) {
            mGoogleActivityControls.setSummary(
                    R.string.sign_in_google_activity_controls_summary_child_account);
        }

        // Prevent sync settings changes from taking effect until the user leaves this screen.
        mSyncSetupInProgressHandle = mProfileSyncService.getSetupInProgressHandle();

        mSearchAndBrowseCategory =
                (PreferenceCategory) findPreference(PREF_SEARCH_AND_BROWSE_CATEGORY);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile));
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener((preference, newValue) -> {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    profile, (boolean) newValue);
            return true;
        });
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate((
                ChromeManagedPreferenceDelegate) (preference
                -> UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(profile)));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mSyncSetupInProgressHandle.close();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                && item.getItemId() == android.R.id.home) {
            if (!mIsFromSigninScreen) return false; // Let Settings activity handle it.
            showCancelSyncDialog();
            return true;
        } else if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance().show(getActivity(),
                    getString(R.string.help_context_sync_and_services),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                || !mIsFromSigninScreen) {
            return super.onCreateView(inflater, container, savedInstanceState);
        }

        // Advanced sync consent flow - add a bottom bar and un-hide relevant preferences.
        ViewGroup result = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);
        inflater.inflate(R.layout.sync_and_services_bottom_bar, result, true);

        ButtonCompat cancelButton = result.findViewById(R.id.cancel_button);
        cancelButton.setOnClickListener(view -> cancelSync());
        ButtonCompat confirmButton = result.findViewById(R.id.confirm_button);
        confirmButton.setOnClickListener(view -> confirmSettings());

        mSearchAndBrowseCategory.setVisible(true);
        mSyncingCategory.setVisible(true);

        return result;
    }

    @Override
    public void onStart() {
        super.onStart();
        mProfileSyncService.addSyncStateChangedListener(this);
    }

    @Override
    public void onStop() {
        super.onStop();
        mProfileSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSyncPreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        // A change to Preference state hasn't been applied yet. Defer
        // updateSyncStateFromSelectedModelTypes so it gets the updated state from isChecked().
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updateSyncStateFromSelectedModelTypes);
        return true;
    }

    /**
     * ProfileSyncService.SyncStateChangedListener implementation, listens to sync state changes.
     *
     * If the user has just turned on sync, this listener is needed in order to enable
     * the encryption settings once the engine has initialized.
     */
    @Override
    public void syncStateChanged() {
        // This is invoked synchronously from ProfileSyncService.setChosenDataTypes, postpone the
        // update to let updateSyncStateFromSelectedModelTypes finish saving the state.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updateSyncPreferences);
    }

    /**
     * Gets the current state of data types from {@link ProfileSyncService} and updates UI elements
     * from this state.
     */
    private void updateSyncPreferences() {
        String signedInAccountName = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                        ConsentLevel.SYNC));
        if (signedInAccountName == null) {
            // May happen if account is removed from the device while this screen is shown.
            getActivity().finish();
            return;
        }

        mGoogleActivityControls.setOnPreferenceClickListener(SyncSettingsUtils.toOnClickListener(
                this, () -> onGoogleActivityControlsClicked(signedInAccountName)));

        updateDataTypeState();
        updateEncryptionState();
    }

    /**
     * Gets the state from data type checkboxes and saves this state into {@link ProfileSyncService}
     * and {@link PersonalDataManager}.
     */
    private void updateSyncStateFromSelectedModelTypes() {
        mProfileSyncService.setChosenDataTypes(
                mSyncEverything.isChecked(), getSelectedModelTypes());
        // Note: mSyncPaymentsIntegration should be checked if mSyncEverything is checked, but if
        // mSyncEverything was just enabled, then that state may not have propagated to
        // mSyncPaymentsIntegration yet. See crbug.com/972863.
        PersonalDataManager.setPaymentsIntegrationEnabled(mSyncEverything.isChecked()
                || (mSyncPaymentsIntegration.isChecked() && mSyncAutofill.isChecked()));
        // Some calls to setChosenDataTypes don't trigger syncStateChanged, so schedule update here.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::updateSyncPreferences);
    }

    /**
     * Update the encryption state.
     *
     * If sync's engine is initialized, the button is enabled and the dialog will present the
     * valid encryption options for the user. Otherwise, any encryption dialogs will be closed
     * and the button will be disabled because the engine is needed in order to know and
     * modify the encryption state.
     */
    private void updateEncryptionState() {
        boolean isEngineInitialized = mProfileSyncService.isEngineInitialized();
        mSyncEncryption.setEnabled(isEngineInitialized);
        mSyncEncryption.setSummary(null);
        if (!isEngineInitialized) {
            // If sync is not initialized, encryption state is unavailable and can't be changed.
            // Leave the button disabled and the summary empty. Additionally, close the dialogs in
            // case they were open when a stop and clear comes.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            return;
        }

        if (mProfileSyncService.isTrustedVaultKeyRequired()) {
            // The user cannot manually enter trusted vault keys, so it needs to gets treated as an
            // error.
            closeDialogIfOpen(FRAGMENT_CUSTOM_PASSPHRASE);
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
            mSyncEncryption.setSummary(mProfileSyncService.isEncryptEverythingEnabled()
                            ? R.string.sync_error_card_title
                            : R.string.sync_passwords_error_card_title);
            return;
        }

        if (!mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        }
        if (mProfileSyncService.isPassphraseRequiredForPreferredDataTypes() && isAdded()) {
            mSyncEncryption.setSummary(
                    errorSummary(getString(R.string.sync_need_passphrase), getActivity()));
        }
    }

    /** Applies a span to the given string to give it an error color. */
    private static Spannable errorSummary(String string, Context context) {
        SpannableString summary = new SpannableString(string);
        summary.setSpan(new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                                context.getResources(), R.color.input_underline_error_color)),
                0, summary.length(), 0);
        return summary;
    }

    private Set<Integer> getSelectedModelTypes() {
        Set<Integer> types = new HashSet<>();
        if (mSyncAutofill.isChecked()) types.add(ModelType.AUTOFILL);
        if (mSyncBookmarks.isChecked()) types.add(ModelType.BOOKMARKS);
        if (mSyncHistory.isChecked()) types.add(ModelType.TYPED_URLS);
        if (mSyncPasswords.isChecked()) types.add(ModelType.PASSWORDS);
        if (mSyncRecentTabs.isChecked()) types.add(ModelType.PROXY_TABS);
        if (mSyncSettings.isChecked()) types.add(ModelType.PREFERENCES);
        return types;
    }

    private void displayPassphraseTypeDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseTypeDialogFragment dialog =
                PassphraseTypeDialogFragment.create(mProfileSyncService.getPassphraseType(),
                        mProfileSyncService.getExplicitPassphraseTime(),
                        mProfileSyncService.isEncryptEverythingAllowed());
        dialog.show(ft, FRAGMENT_PASSPHRASE_TYPE);
        dialog.setTargetFragment(this, -1);
    }

    private void displayPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseDialogFragment.newInstance(this).show(ft, FRAGMENT_ENTER_PASSPHRASE);
    }

    private void displayCustomPassphraseDialog() {
        FragmentTransaction ft = getFragmentManager().beginTransaction();
        PassphraseCreationDialogFragment dialog = new PassphraseCreationDialogFragment();
        dialog.setTargetFragment(this, -1);
        dialog.show(ft, FRAGMENT_CUSTOM_PASSPHRASE);
    }

    private void closeDialogIfOpen(String tag) {
        FragmentManager manager = getFragmentManager();
        if (manager == null) {
            // Do nothing if the manager doesn't exist yet; see http://crbug.com/480544.
            return;
        }
        DialogFragment df = (DialogFragment) manager.findFragmentByTag(tag);
        if (df != null) {
            df.dismiss();
        }
    }

    /** Returns whether the passphrase successfully decrypted the pending keys. */
    private boolean handleDecryption(String passphrase) {
        if (passphrase.isEmpty() || !mProfileSyncService.setDecryptionPassphrase(passphrase)) {
            return false;
        }
        // PassphraseDialogFragment doesn't handle closing itself, so do it here. This is not done
        // in updateSyncStateFromAndroidSyncSettings() because that happens onResume and possibly in
        // other cases where the dialog should stay open.
        closeDialogIfOpen(FRAGMENT_ENTER_PASSPHRASE);
        // Update our configuration UI.
        updateSyncPreferences();
        return true;
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public boolean onPassphraseEntered(String passphrase) {
        if (!mProfileSyncService.isEngineInitialized()
                || !mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            // If the engine was shut down since the dialog was opened, or the passphrase isn't
            // required anymore, do nothing.
            return false;
        }
        return handleDecryption(passphrase);
    }

    /** Callback for PassphraseDialogFragment.Listener */
    @Override
    public void onPassphraseCanceled() {}

    /** Callback for PassphraseCreationDialogFragment.Listener */
    @Override
    public void onPassphraseCreated(String passphrase) {
        if (!mProfileSyncService.isEngineInitialized()) {
            // If the engine was shut down since the dialog was opened, do nothing.
            return;
        }
        mProfileSyncService.enableEncryptEverything();
        mProfileSyncService.setEncryptionPassphrase(passphrase);
        // Save the current state of data types - this tells the sync engine to
        // apply our encryption configuration changes.
        updateSyncStateFromSelectedModelTypes();
    }

    /** Callback for PassphraseTypeDialogFragment.Listener */
    @Override
    public void onPassphraseTypeSelected(@PassphraseType int type) {
        if (!mProfileSyncService.isEngineInitialized()) {
            // If the engine was shut down since the dialog was opened, do nothing.
            return;
        }

        boolean isAllDataEncrypted = mProfileSyncService.isEncryptEverythingEnabled();
        boolean isUsingSecondaryPassphrase = mProfileSyncService.isUsingSecondaryPassphrase();

        // The passphrase type should only ever be selected if the account doesn't have
        // full encryption enabled. Otherwise both options should be disabled.
        assert !isAllDataEncrypted;
        assert !isUsingSecondaryPassphrase;
        displayCustomPassphraseDialog();
    }

    private void onGoogleActivityControlsClicked(String signedInAccountName) {
        AppHooks.get().createGoogleActivityController().openWebAndAppActivitySettings(
                getActivity(), signedInAccountName);
        RecordUserAction.record("Signin_AccountSettings_GoogleActivityControlsClicked");
    }

    private void onSyncEncryptionClicked() {
        if (!mProfileSyncService.isEngineInitialized()) return;

        if (mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            displayPassphraseDialog();
        } else if (mProfileSyncService.isTrustedVaultKeyRequired()) {
            CoreAccountInfo primaryAccountInfo =
                    IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                            ConsentLevel.SYNC);
            if (primaryAccountInfo != null) {
                SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog(
                        this, primaryAccountInfo, REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL);
            }
        } else {
            displayPassphraseTypeDialog();
        }
    }

    /**
     * Gets the current state of data types from {@link ProfileSyncService} and updates the UI.
     */
    private void updateDataTypeState() {
        boolean syncEverything = mProfileSyncService.hasKeepEverythingSynced();
        mSyncEverything.setChecked(syncEverything);
        if (syncEverything) {
            for (CheckBoxPreference pref : mSyncTypePreferences) {
                pref.setChecked(true);
                pref.setEnabled(false);
            }
            return;
        }

        Set<Integer> syncTypes = mProfileSyncService.getChosenDataTypes();
        mSyncAutofill.setChecked(syncTypes.contains(ModelType.AUTOFILL));
        mSyncAutofill.setEnabled(true);
        mSyncBookmarks.setChecked(syncTypes.contains(ModelType.BOOKMARKS));
        mSyncBookmarks.setEnabled(true);
        mSyncHistory.setChecked(syncTypes.contains(ModelType.TYPED_URLS));
        mSyncHistory.setEnabled(true);
        mSyncPasswords.setChecked(syncTypes.contains(ModelType.PASSWORDS));
        mSyncPasswords.setEnabled(true);
        mSyncRecentTabs.setChecked(syncTypes.contains(ModelType.PROXY_TABS));
        mSyncRecentTabs.setEnabled(true);
        mSyncSettings.setChecked(syncTypes.contains(ModelType.PREFERENCES));
        mSyncSettings.setEnabled(true);

        // Payments integration requires AUTOFILL model type
        boolean syncAutofill = syncTypes.contains(ModelType.AUTOFILL);
        mSyncPaymentsIntegration.setChecked(
                syncAutofill && PersonalDataManager.isPaymentsIntegrationEnabled());
        mSyncPaymentsIntegration.setEnabled(syncAutofill);
    }

    /**
     * Called upon completion of an activity started by a previous call to startActivityForResult()
     * via SyncSettingsUtils.openTrustedVaultKeyRetrievalDialog().
     * @param requestCode Request code of the requested intent.
     * @param resultCode Result code of the requested intent.
     * @param data The data returned by the intent.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // Upon key retrieval completion, the keys in TrustedVaultClient could have changed. This is
        // done even if the user cancelled the flow (i.e. resultCode != RESULT_OK) because it's
        // harmless to issue a redundant notifyKeysChanged().
        if (requestCode == REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL) {
            TrustedVaultClient.get().notifyKeysChanged();
        }
    }

    @Override
    public boolean onBackPressed() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                || !mIsFromSigninScreen) {
            return false; // Let parent activity handle it.
        }
        showCancelSyncDialog();
        return true;
    }

    private void showCancelSyncDialog() {
        RecordUserAction.record("Signin_Signin_BackOnAdvancedSyncSettings");
        CancelSyncDialog dialog = new CancelSyncDialog();
        dialog.setTargetFragment(this, 0);
        dialog.show(getFragmentManager(), FRAGMENT_CANCEL_SYNC);
    }

    private void confirmSettings() {
        RecordUserAction.record("Signin_Signin_ConfirmAdvancedSyncSettings");
        ProfileSyncService.get().setFirstSetupComplete(
                SyncFirstSetupCompleteSource.ADVANCED_FLOW_CONFIRM);
        UnifiedConsentServiceBridge.recordSyncSetupDataTypesHistogram(
                Profile.getLastUsedRegularProfile());
        // Settings will be applied when mSyncSetupInProgressHandle is released in onDestroy.
        getActivity().finish();
    }

    private void cancelSync() {
        RecordUserAction.record("Signin_Signin_CancelAdvancedSyncSettings");
        IdentityServicesProvider.get().getSigninManager().signOut(
                org.chromium.components.signin.metrics.SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        getActivity().finish();
    }

    /**
     * The dialog that offers the user to cancel sync. Only shown when
     * {@link ManageSyncSettings} is opened from the sign-in screen. Shown when the user
     * tries to close the settings page without confirming settings.
     */
    public static class CancelSyncDialog extends DialogFragment {
        public CancelSyncDialog() {
            // Fragment must have an empty public constructor
        }

        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            return new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                    .setTitle(R.string.cancel_sync_dialog_title)
                    .setMessage(R.string.cancel_sync_dialog_message)
                    .setNegativeButton(R.string.back, (dialog, which) -> onBackPressed())
                    .setPositiveButton(
                            R.string.cancel_sync_button, (dialog, which) -> onCancelSyncPressed())
                    .create();
        }

        private void onBackPressed() {
            RecordUserAction.record("Signin_Signin_CancelCancelAdvancedSyncSettings");
            dismiss();
        }

        public void onCancelSyncPressed() {
            RecordUserAction.record("Signin_Signin_ConfirmCancelAdvancedSyncSettings");
            ManageSyncSettings fragment = (ManageSyncSettings) getTargetFragment();
            fragment.cancelSync();
        }
    }
}
