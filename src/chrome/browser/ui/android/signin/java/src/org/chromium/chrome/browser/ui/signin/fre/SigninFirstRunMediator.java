// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fre;

import android.accounts.Account;
import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerDialogCoordinator;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunCoordinator.Delegate;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunProperties.FrePolicy;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.ArrayList;
import java.util.List;

class SigninFirstRunMediator implements AccountsChangeObserver, ProfileDataCache.Observer,
                                        AccountPickerCoordinator.Listener {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Delegate mDelegate;
    private final PrivacyPreferencesManager mPrivacyPreferencesManager;
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private AccountPickerDialogCoordinator mDialogCoordinator;
    private @Nullable String mSelectedAccountName;
    private @Nullable String mDefaultAccountName;

    SigninFirstRunMediator(Context context, ModalDialogManager modalDialogManager,
            Delegate delegate, PrivacyPreferencesManager privacyPreferencesManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDelegate = delegate;
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
        mModel = SigninFirstRunProperties.createModel(this::onSelectedAccountClicked,
                this::onContinueAsClicked, this::onDismissClicked,
                ExternalAuthUtils.getInstance().canUseGooglePlayServices(),
                getFooterString(/*hasChildAccount=*/false, /*isMetricsReportingDisabled=*/false));

        mProfileDataCache.addObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
        mAccountManagerFacade.addObserver(this);
        updateAccounts(
                AccountUtils.getAccountsIfFulfilledOrEmpty(mAccountManagerFacade.getAccounts()));
    }

    PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
        mAccountManagerFacade.removeObserver(this);
    }

    void onNativeAndPolicyLoaded(boolean hasPolicies) {
        mModel.set(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED, true);
        mModel.set(SigninFirstRunProperties.FRE_POLICY, hasPolicies ? new FrePolicy() : null);
        final boolean isSigninSupported = ExternalAuthUtils.getInstance().canUseGooglePlayServices()
                && !IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .isSigninDisabledByPolicy();
        mModel.set(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED, isSigninSupported);

        if (mPrivacyPreferencesManager.isMetricsReportingDisabledByPolicy()) {
            // If metrics reporting is disabled by policy then there is at least one policy.
            // Therefore, policies have loaded and frePolicy is not null.
            assert hasPolicies;

            final FrePolicy frePolicy = mModel.get(SigninFirstRunProperties.FRE_POLICY);
            frePolicy.metricsReportingDisabledByPolicy = true;
        }

        final boolean isChild = mModel.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED);
        mModel.set(SigninFirstRunProperties.FOOTER_STRING,
                getFooterString(isChild, isMetricsReportingDisabledByPolicy()));
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        updateSelectedAccountData(accountEmail);
    }

    /** Implements {@link AccountsChangeObserver}. */
    @Override
    public void onAccountsChanged() {
        mAccountManagerFacade.getAccounts().then(this::updateAccounts);
    }

    @Override
    public void onAccountSelected(String accountName) {
        setSelectedAccountName(accountName);
        if (mDialogCoordinator != null) mDialogCoordinator.dismissDialog();
    }

    @Override
    public void addAccount() {
        mDelegate.addAccount();
    }

    protected boolean isMetricsReportingDisabledByPolicy() {
        @Nullable
        FrePolicy frePolicy = mModel.get(SigninFirstRunProperties.FRE_POLICY);
        return frePolicy != null && frePolicy.metricsReportingDisabledByPolicy;
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_SELECTED_ACCOUNT_CLICKED}.
     */
    private void onSelectedAccountClicked() {
        mDialogCoordinator =
                new AccountPickerDialogCoordinator(mContext, this, mModalDialogManager);
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_CONTINUE_AS_CLICKED}.
     */
    private void onContinueAsClicked() {
        if (!mModel.get(SigninFirstRunProperties.IS_SIGNIN_SUPPORTED)) {
            mDelegate.acceptTermsOfService();
            mDelegate.advanceToNextPage();
            return;
        }
        if (mSelectedAccountName == null) {
            mDelegate.addAccount();
            return;
        }

        // In all other cases, the button text is "Continue as ...", so mark ToS as accepted.
        // This is needed to get metrics/crash reports from the sign-in flow itself.
        mDelegate.acceptTermsOfService();
        if (mModel.get(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED)) {
            // Don't perform the sign-in here, as it will be handled by SigninChecker.
            mDelegate.advanceToNextPage();
            return;
        }
        assert mModel.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED)
            : "The continue button shouldn't be visible before the native is not initialized!";
        mDelegate.recordFreProgressHistogram(
                TextUtils.equals(mDefaultAccountName, mSelectedAccountName)
                        ? MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT
                        : MobileFreProgress.WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT);
        // If the user signs into an account on the FRE, goes to the sync consent page and presses
        // back to come back to the FRE, then there will already be an account signed in.
        @Nullable
        CoreAccountInfo signedInAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (signedInAccount != null && signedInAccount.getEmail().equals(mSelectedAccountName)) {
            mDelegate.advanceToNextPage();
            return;
        }
        final SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        signinManager.onFirstRunCheckDone();
        signinManager.signin(
                AccountUtils.createAccountFromName(mSelectedAccountName), new SignInCallback() {
                    @Override
                    public void onSignInComplete() {
                        // Wait for sign-in to be complete before advancing to the next page.
                        mDelegate.advanceToNextPage();
                    }

                    @Override
                    public void onSignInAborted() {
                        // TODO(crbug/1248090): Handle the sign-in error here
                    }
                });
    }

    /**
     * Callback for the PropertyKey {@link SigninFirstRunProperties#ON_DISMISS_CLICKED}.
     */
    private void onDismissClicked() {
        assert mModel.get(SigninFirstRunProperties.ARE_NATIVE_AND_POLICY_LOADED)
            : "The dismiss button shouldn't be visible before the native is not initialized!";
        mDelegate.recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
        mDelegate.acceptTermsOfService();
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .signOut(SignoutReason.ABORT_SIGNIN, mDelegate::advanceToNextPage,
                            /* forceWipeUserData= */ false);
        } else {
            mDelegate.advanceToNextPage();
        }
    }

    private void setSelectedAccountName(String accountName) {
        mSelectedAccountName = accountName;
        updateSelectedAccountData(mSelectedAccountName);
    }

    private void updateSelectedAccountData(String accountEmail) {
        if (TextUtils.equals(mSelectedAccountName, accountEmail)) {
            mModel.set(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA,
                    mProfileDataCache.getProfileDataOrDefault(accountEmail));
        }
    }

    private void updateAccounts(List<Account> accounts) {
        if (accounts.isEmpty()) {
            mDefaultAccountName = null;
            mSelectedAccountName = null;
            mModel.set(SigninFirstRunProperties.SELECTED_ACCOUNT_DATA, null);
            if (mDialogCoordinator != null) {
                mDialogCoordinator.dismissDialog();
            }
        } else {
            mDefaultAccountName = accounts.get(0).name;
            if (mSelectedAccountName == null
                    || AccountUtils.findAccountByName(accounts, mSelectedAccountName) == null) {
                setSelectedAccountName(mDefaultAccountName);
            }
        }

        AccountUtils.checkChildAccountStatus(
                mAccountManagerFacade, accounts, this::onChildAccountStatusReady);
    }

    private void onChildAccountStatusReady(boolean isChild, @Nullable Account childAccount) {
        mModel.set(SigninFirstRunProperties.IS_SELECTED_ACCOUNT_SUPERVISED, isChild);
        mModel.set(SigninFirstRunProperties.FOOTER_STRING,
                getFooterString(isChild, isMetricsReportingDisabledByPolicy()));
        // Selected account data will be updated in {@link #onProfileDataUpdated}
        mProfileDataCache.setBadge(isChild ? R.drawable.ic_account_child_20dp : 0);
    }

    /**
     * Builds footer string dynamically.
     * First line has a TOS link. A privacy notice will also be shown for child accounts.
     * Second line appears only if MetricsReporting is not disabled by policy.
     */
    private SpannableString getFooterString(
            boolean hasChildAccount, boolean isMetricsReportingDisabled) {
        String footerString = mContext.getString(hasChildAccount
                        ? R.string.signin_fre_footer_tos_with_supervised_user
                        : R.string.signin_fre_footer_tos);

        ArrayList<SpanApplier.SpanInfo> spans = new ArrayList<>();
        // Terms of Service SpanInfo.
        final NoUnderlineClickableSpan clickableTermsOfServiceSpan =
                new NoUnderlineClickableSpan(mContext.getResources(),
                        view -> mDelegate.showInfoPage(R.string.google_terms_of_service_url));
        spans.add(
                new SpanApplier.SpanInfo("<TOS_LINK>", "</TOS_LINK>", clickableTermsOfServiceSpan));

        // Privacy notice Link SpanInfo.
        if (hasChildAccount) {
            final NoUnderlineClickableSpan clickablePrivacyPolicySpan =
                    new NoUnderlineClickableSpan(mContext.getResources(),
                            view -> mDelegate.showInfoPage(R.string.google_privacy_policy_url));

            spans.add(new SpanApplier.SpanInfo(
                    "<PRIVACY_LINK>", "</PRIVACY_LINK>", clickablePrivacyPolicySpan));
        }

        // Metrics and Crash Reporting SpanInfo.
        if (!isMetricsReportingDisabled) {
            footerString += "\n" + mContext.getString(R.string.signin_fre_footer_metrics_reporting);
            final NoUnderlineClickableSpan clickableUMADialogSpan = new NoUnderlineClickableSpan(
                    mContext.getResources(), view -> mDelegate.openUmaDialog());
            spans.add(
                    new SpanApplier.SpanInfo("<UMA_LINK>", "</UMA_LINK>", clickableUMADialogSpan));
        }

        // Apply spans to footer string.
        return SpanApplier.applySpans(footerString, spans.toArray(new SpanApplier.SpanInfo[0]));
    }
}
