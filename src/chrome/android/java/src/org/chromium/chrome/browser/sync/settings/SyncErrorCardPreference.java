// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

import java.util.Collections;

public class SyncErrorCardPreference extends Preference
        implements AndroidSyncSettings.AndroidSyncSettingsObserver,
                   ProfileSyncService.SyncStateChangedListener, ProfileDataCache.Observer {
    /**
     * Listener for the buttons in the error card.
     */
    public interface SyncErrorCardPreferenceListener {
        /**
         * Called to check if the preference should be hidden in case its created from signin
         * screen.
         */
        boolean shouldSuppressSyncSetupIncomplete();

        /**
         * Called when the user clicks the primary button.
         */
        void onSyncErrorCardPrimaryButtonClicked();

        /**
         * Called when the user clicks the secondary button. This button is only shown for
         * {@link SyncError.SYNC_SETUP_INCOMPLETE} error.
         */
        void onSyncErrorCardSecondaryButtonClicked();
    }

    private final ProfileDataCache mProfileDataCache;
    private SyncErrorCardPreferenceListener mListener;
    private @SyncError int mSyncError;

    public SyncErrorCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        mProfileDataCache = ProfileDataCache.createProfileDataCache(
                context, R.drawable.ic_sync_badge_error_20dp);
        setLayoutResource(R.layout.personalized_signin_promo_view_settings);
        mSyncError = SyncError.NO_ERROR;
    }

    @Override
    public void onAttached() {
        super.onAttached();
        mProfileDataCache.addObserver(this);
        AndroidSyncSettings.get().registerObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
        update();
    }

    @Override
    public void onDetached() {
        super.onDetached();
        mProfileDataCache.removeObserver(this);
        AndroidSyncSettings.get().unregisterObserver(this);
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        if (mSyncError == SyncError.NO_ERROR) {
            return;
        }

        PersonalizedSigninPromoView errorCardView =
                (PersonalizedSigninPromoView) holder.findViewById(R.id.signin_promo_view_container);
        setupSyncErrorCardView(errorCardView);
    }

    private void update() {
        // If feature is not enabled keep the preference at the default hidden state.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            return;
        }

        mSyncError = SyncSettingsUtils.getSyncError();
        boolean suppressSyncSetupIncompleteFromSigninPage =
                (mSyncError == SyncError.SYNC_SETUP_INCOMPLETE)
                && mListener.shouldSuppressSyncSetupIncomplete();
        if (mSyncError == SyncError.NO_ERROR || suppressSyncSetupIncompleteFromSigninPage) {
            setVisible(false);
        } else {
            setVisible(true);
            notifyChanged();
        }
    }

    private void setupSyncErrorCardView(PersonalizedSigninPromoView errorCardView) {
        String signedInAccount = CoreAccountInfo.getEmailFrom(
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC));
        // May happen if account is removed from the device while this screen is shown.
        // ManageSyncSettings will take care of finishing the activity in such case.
        if (signedInAccount == null) {
            return;
        }

        mProfileDataCache.update(Collections.singletonList(signedInAccount));
        Drawable accountImage =
                mProfileDataCache.getProfileDataOrDefault(signedInAccount).getImage();
        errorCardView.getImage().setImageDrawable(accountImage);

        errorCardView.getDismissButton().setVisibility(View.GONE);
        if (mSyncError == SyncError.SYNC_SETUP_INCOMPLETE) {
            errorCardView.getStatusMessage().setVisibility(View.GONE);
        } else {
            errorCardView.getStatusMessage().setVisibility(View.VISIBLE);
        }
        errorCardView.getDescription().setText(
                SyncSettingsUtils.getSyncErrorHint(getContext(), mSyncError));

        errorCardView.getPrimaryButton().setText(
                SyncSettingsUtils.getSyncErrorCardButtonLabel(getContext(), mSyncError));
        errorCardView.getPrimaryButton().setOnClickListener(
                v -> mListener.onSyncErrorCardPrimaryButtonClicked());
        if (mSyncError == SyncError.SYNC_SETUP_INCOMPLETE) {
            errorCardView.getSecondaryButton().setOnClickListener(
                    v -> mListener.onSyncErrorCardSecondaryButtonClicked());
            errorCardView.getSecondaryButton().setText(R.string.cancel);
        } else {
            errorCardView.getSecondaryButton().setVisibility(View.GONE);
        }
    }

    public void setSyncErrorCardPreferenceListener(SyncErrorCardPreferenceListener listener) {
        mListener = listener;
    }

    public @SyncError int getSyncError() {
        return mSyncError;
    }

    /**
     * {@link ProfileSyncServiceListener} implementation.
     */
    @Override
    public void syncStateChanged() {
        update();
    }

    /**
     * {@link AndroidSyncSettings.AndroidSyncSettingsObserver} implementation.
     */
    @Override
    public void androidSyncSettingsChanged() {
        update();
    }

    /**
     * {@link ProfileDataCache.Observer} implementation.
     */
    @Override
    public void onProfileDataUpdated(String accountId) {
        update();
    }
}
