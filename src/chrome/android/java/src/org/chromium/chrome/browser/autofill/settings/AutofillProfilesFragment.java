// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.prefeditor.EditorObserverForTest;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/**
 * Autofill profiles fragment, which allows the user to edit autofill profiles.
 */
public class AutofillProfilesFragment extends PreferenceFragmentCompat
        implements PersonalDataManager.PersonalDataManagerObserver {
    private static EditorObserverForTest sObserverForTest;
    static final String PREF_NEW_PROFILE = "new_profile";

    EditorDialog mLastEditorDialogForTest;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_addresses_settings_title);

        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        // Suppresses unwanted animations while Preferences are removed from and re-added to the
        // screen.
        screen.setShouldUseGeneratedIds(false);

        setPreferenceScreen(screen);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Always rebuild our list of profiles.  Although we could detect if profiles are added or
        // deleted (GUID list changes), the profile summary (name+addr) might be different.  To be
        // safe, we update all.
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    private void rebuildProfileList() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        ChromeSwitchPreference autofillSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_profiles_toggle_label);
        autofillSwitch.setSummary(R.string.autofill_enable_profiles_toggle_sublabel);
        autofillSwitch.setChecked(PersonalDataManager.isAutofillProfileEnabled());
        autofillSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            PersonalDataManager.setAutofillProfileEnabled((boolean) newValue);
            return true;
        });
        autofillSwitch.setManagedPreferenceDelegate(new ChromeManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillProfileManaged();
            }

            @Override
            public boolean isPreferenceClickDisabledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillProfileManaged()
                        && !PersonalDataManager.isAutofillProfileEnabled();
            }
        });
        getPreferenceScreen().addPreference(autofillSwitch);

        for (AutofillProfile profile : PersonalDataManager.getInstance().getProfilesForSettings()) {
            // Add a preference for the profile.
            Preference pref;
            if (profile.getIsLocal()) {
                pref = new AutofillProfileEditorPreference(getStyledContext());
                pref.setTitle(profile.getFullName());
                pref.setSummary(profile.getLabel());
                pref.setKey(pref.getTitle().toString()); // For testing.
            } else {
                pref = new Preference(getStyledContext());
                pref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
                pref.setFragment(AutofillServerProfileFragment.class.getName());
            }
            Bundle args = pref.getExtras();
            args.putString(AutofillEditorBase.AUTOFILL_GUID, profile.getGUID());
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                getPreferenceScreen().addPreference(pref);
            }
        }

        // Add 'Add address' button. Tap of it brings up address editor which allows users type in
        // new addresses.
        if (PersonalDataManager.isAutofillProfileEnabled()) {
            AutofillProfileEditorPreference pref =
                    new AutofillProfileEditorPreference(getStyledContext());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(ApiCompatibilityUtils.getColor(
                                            getResources(), R.color.default_control_color_active),
                    PorterDuff.Mode.SRC_IN);
            pref.setIcon(plusIcon);
            pref.setTitle(R.string.autofill_create_profile);
            pref.setKey(PREF_NEW_PROFILE); // For testing.

            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                getPreferenceScreen().addPreference(pref);
            }
        }
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManager.getInstance().registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManager.getInstance().unregisterDataObserver(this);
        super.onDestroyView();
    }

    @VisibleForTesting
    public static void setObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
        EditorDialog.setEditorObserverForTest(sObserverForTest);
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof AutofillProfileEditorPreference) {
            String guid = ((AutofillProfileEditorPreference) preference).getGUID();
            EditorDialog editorDialog = prepareEditorDialog(guid);
            mLastEditorDialogForTest = editorDialog;
            AutofillAddress autofillAddress = guid == null
                    ? null
                    : new AutofillAddress(
                            getActivity(), PersonalDataManager.getInstance().getProfile(guid));
            editAddress(editorDialog, autofillAddress);
            return;
        }

        super.onDisplayPreferenceDialog(preference);
    }

    @VisibleForTesting
    EditorDialog prepareEditorDialog(String guid) {
        Runnable runnable = guid == null ? null : () -> {
            PersonalDataManager.getInstance().deleteProfile(guid);
            SettingsAutofillAndPaymentsObserver.getInstance().notifyOnAddressDeleted(guid);
            if (sObserverForTest != null) {
                sObserverForTest.onEditorReadyToEdit();
            }
        };

        return new EditorDialog(getActivity(), runnable);
    }

    private void editAddress(EditorDialog dialog, AutofillAddress autofillAddress) {
        AddressEditor addressEditor =
                new AddressEditor(AddressEditor.Purpose.AUTOFILL_SETTINGS, /*saveToDisk=*/true);
        addressEditor.setEditorDialog(dialog);

        /*
         * There are four cases for |address| here.
         * (1) |address| is null: the user canceled address creation
         * (2) |address| is non-null: the user canceled editing an existing address
         * (3) |address| is non-null: the user edited an existing address.
         * (4) |address| is non-null: the user created a new address.
         * We should save the changes (set the profile) for cases 3 and 4,
         * and it's OK to set the profile for 2.
         */
        addressEditor.edit(autofillAddress, address -> {
            if (address != null) {
                PersonalDataManager.getInstance().setProfile(address.getProfile());
                SettingsAutofillAndPaymentsObserver.getInstance().notifyOnAddressUpdated(address);
            }
            if (sObserverForTest != null) {
                sObserverForTest.onEditorReadyToEdit();
            }
        });
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    EditorDialog getEditorDialogForTest() {
        return mLastEditorDialogForTest;
    }
}
