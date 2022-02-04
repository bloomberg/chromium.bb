// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SigninReason;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of the SigninManager which provides access from the Java layer.
 * <p/>
 * This class handles common paths during the sign-in and sign-out flows.
 * <p/>
 * Only usable from the UI thread as the native SigninManager requires its access to be in the
 * UI thread.
 * <p/>
 * See chrome/browser/android/signin/signin_manager_android.h for more details.
 */
class SigninManagerImpl implements IdentityManager.Observer, SigninManager {
    private static final String TAG = "SigninManager";
    private static final int[] SYNC_DATA_TYPES = {BrowsingDataType.HISTORY, BrowsingDataType.CACHE,
            BrowsingDataType.COOKIES, BrowsingDataType.PASSWORDS, BrowsingDataType.FORM_DATA};

    /**
     * Address of the native Signin Manager android.
     * This is not final, as destroy() updates this.
     */
    private long mNativeSigninManagerAndroid;
    private final AccountTrackerService mAccountTrackerService;
    private final IdentityManager mIdentityManager;
    private final IdentityMutator mIdentityMutator;
    private final AndroidSyncSettings mAndroidSyncSettings;
    private final ObserverList<SignInStateObserver> mSignInStateObservers = new ObserverList<>();
    private final List<Runnable> mCallbacksWaitingForPendingOperation = new ArrayList<>();
    private boolean mSigninAllowedByPolicy;

    /**
     * Tracks whether the First Run check has been completed.
     *
     * A new sign-in can not be started while this is pending, to prevent the
     * pending check from eventually starting a 2nd sign-in.
     */
    private boolean mFirstRunCheckIsPending = true;

    /**
     * Will be set during the sign in process, and nulled out when there is not a pending sign in.
     * Needs to be null checked after ever async entry point because it can be nulled out at any
     * time by system accounts changing.
     */
    private @Nullable SignInState mSignInState;

    /**
     * Set during sign-out process and nulled out once complete. Helps to atomically gather/clear
     * various sign-out state.
     */
    private @Nullable SignOutState mSignOutState;

    /** Tracks whether deletion of browsing data is in progress. */
    private boolean mWipeUserDataInProgress;

    /**
     * Called by native to create an instance of SigninManager.
     * @param nativeSigninManagerAndroid A pointer to native's SigninManagerAndroid.
     */
    @CalledByNative
    @VisibleForTesting
    static SigninManager create(long nativeSigninManagerAndroid,
            AccountTrackerService accountTrackerService, IdentityManager identityManager,
            IdentityMutator identityMutator) {
        assert nativeSigninManagerAndroid != 0;
        assert accountTrackerService != null;
        assert identityManager != null;
        assert identityMutator != null;
        final SigninManagerImpl signinManager = new SigninManagerImpl(nativeSigninManagerAndroid,
                accountTrackerService, identityManager, identityMutator, AndroidSyncSettings.get());

        identityManager.addObserver(signinManager);
        AccountInfoServiceProvider.init(identityManager, accountTrackerService);

        signinManager.reloadAllAccountsFromSystem(CoreAccountInfo.getIdFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)));
        return signinManager;
    }

    private SigninManagerImpl(long nativeSigninManagerAndroid,
            AccountTrackerService accountTrackerService, IdentityManager identityManager,
            IdentityMutator identityMutator, AndroidSyncSettings androidSyncSettings) {
        ThreadUtils.assertOnUiThread();
        assert androidSyncSettings != null;
        mNativeSigninManagerAndroid = nativeSigninManagerAndroid;
        mAccountTrackerService = accountTrackerService;
        mIdentityManager = identityManager;
        mIdentityMutator = identityMutator;
        mAndroidSyncSettings = androidSyncSettings;

        mSigninAllowedByPolicy =
                SigninManagerImplJni.get().isSigninAllowedByPolicy(mNativeSigninManagerAndroid);
    }

    /**
     * Triggered during SigninManagerAndroidWrapper's KeyedService::Shutdown.
     * Drop references with external services and native.
     */
    @VisibleForTesting
    @CalledByNative
    void destroy() {
        AccountInfoServiceProvider.get().destroy();
        mIdentityManager.removeObserver(this);
        mNativeSigninManagerAndroid = 0;
    }

    /**
     * Extracts the domain name of a given account's email.
     */
    @Override
    public String extractDomainName(String accountEmail) {
        return SigninManagerImplJni.get().extractDomainName(accountEmail);
    };

    /**
     * Returns the IdentityManager used by SigninManager.
     */
    @Override
    public IdentityManager getIdentityManager() {
        return mIdentityManager;
    }

    /**
     * Notifies the SigninManager that the First Run check has completed.
     *
     * The user will be allowed to sign-in once this is signaled.
     */
    @Override
    public void onFirstRunCheckDone() {
        mFirstRunCheckIsPending = false;

        if (isSyncOptInAllowed()) {
            notifySignInAllowedChanged();
        }
    }

    /**
     * Returns true if sign in can be started now.
     */
    @Override
    public boolean isSigninAllowed() {
        return !mFirstRunCheckIsPending && mSignInState == null && mSigninAllowedByPolicy
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) == null
                && isSigninSupported();
    }

    /**
     * Returns true if sync opt in can be started now.
     */
    @Override
    public boolean isSyncOptInAllowed() {
        return !mFirstRunCheckIsPending && mSignInState == null && mSigninAllowedByPolicy
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC) == null
                && isSigninSupported();
    }

    /**
     * Returns true if signin is disabled by policy.
     */
    @Override
    public boolean isSigninDisabledByPolicy() {
        return !mSigninAllowedByPolicy;
    }

    /**
     * @return Whether true if the current user is not demo user and the user has a reasonable
     *         Google Play Services installed.
     */
    @Override
    public boolean isSigninSupported() {
        return !ApiCompatibilityUtils.isDemoUser() && isGooglePlayServicesPresent();
    }

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    @Override
    public boolean isForceSigninEnabled() {
        return SigninManagerImplJni.get().isForceSigninEnabled(mNativeSigninManagerAndroid);
    }

    /**
     * Registers a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    @Override
    public void addSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.addObserver(observer);
    }

    /**
     * Unregisters a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    @Override
    public void removeSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.removeObserver(observer);
    }

    private void notifySignInAllowedChanged() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            for (SignInStateObserver observer : mSignInStateObservers) {
                observer.onSignInAllowedChanged();
            }
        });
    }

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - Complete sign-in with the native IdentityManager.
     *   - Call the callback if provided.
     *
     * @param account The account to sign in to.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    @Override
    public void signin(Account account, @Nullable SignInCallback callback) {
        signinInternal(SignInState.createForSignin(account, callback));
    }

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - Wait for policy to be checked for the account.
     *   - If managed, wait for the policy to be fetched.
     *   - Complete sign-in with the native IdentityManager.
     *   - Call the callback if provided.
     *
     * @param accessPoint {@link SigninAccessPoint} that initiated the sign-in flow.
     * @param account The account to sign in to.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    @Override
    public void signinAndEnableSync(@SigninAccessPoint int accessPoint, Account account,
            @Nullable SignInCallback callback) {
        signinInternal(SignInState.createForSigninAndEnableSync(accessPoint, account, callback));
    }

    private void signinInternal(SignInState signInState) {
        assert isSyncOptInAllowed() : "Sign-in isn't allowed!";
        assert signInState != null : "SigninState shouldn't be null!";
        assert signInState.mCoreAccountInfo == null : "mCoreAccountInfo shouldn't be set!";

        // The mSignInState must be updated prior to the async processing below, as this indicates
        // that a signin operation is in progress and prevents other sign in operations from being
        // started until this one completes (see {@link isOperationInProgress()}).
        mSignInState = signInState;
        signInState = null;

        AccountInfoServiceProvider.get()
                .getAccountInfoByEmail(mSignInState.mAccount.name)
                .then(accountInfo -> {
                    mSignInState.mCoreAccountInfo = accountInfo;
                    notifySignInAllowedChanged();

                    if (mSignInState.shouldTurnSyncOn()) {
                        Log.d(TAG, "Checking if account has policy management enabled");
                        fetchAndApplyCloudPolicy(mSignInState.mCoreAccountInfo,
                                this::finishSignInAfterPolicyEnforced);
                    } else {
                        // Sign-in without sync doesn't enforce enterprise policy, so skip that
                        // step.
                        finishSignInAfterPolicyEnforced();
                    }
                });
    }

    /**
     * Finishes the sign-in flow. If the user is managed, the policy should be fetched and enforced
     * before calling this method.
     */
    @VisibleForTesting
    void finishSignInAfterPolicyEnforced() {
        assert mSignInState != null : "SigninState shouldn't be null!";
        assert !mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC)
            : "The user should not be already signed in";

        // Setting the primary account triggers observers which query accounts from IdentityManager.
        // Reloading before setting the primary ensures they don't get an empty list of accounts.
        reloadAllAccountsFromSystem(mSignInState.mCoreAccountInfo.getId());

        @ConsentLevel
        int consentLevel =
                mSignInState.shouldTurnSyncOn() ? ConsentLevel.SYNC : ConsentLevel.SIGNIN;
        if (!mIdentityMutator.setPrimaryAccount(
                    mSignInState.mCoreAccountInfo.getId(), consentLevel)) {
            Log.w(TAG, "Failed to set the PrimaryAccount in IdentityManager, aborting signin");
            abortSignIn();
            return;
        }

        if (mSignInState.shouldTurnSyncOn()) {
            // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that
            // uses the sync account before the native is loaded.
            SigninPreferencesManager.getInstance().setLegacySyncAccountEmail(
                    mSignInState.mCoreAccountInfo.getEmail());

            // Cache the signed-in account name. This must be done after the native call, otherwise
            // sync tries to start without being signed in the native code and crashes.
            mAndroidSyncSettings.updateAccount(
                    AccountUtils.createAccountFromName(mSignInState.mCoreAccountInfo.getEmail()));
            boolean atLeastOneDataTypeSynced = !SyncService.get().getChosenDataTypes().isEmpty();
            if (atLeastOneDataTypeSynced) {
                // Turn on sync only when user has at least one data type to sync, this is
                // consistent with {@link ManageSyncSettings#updataSyncStateFromSelectedModelTypes},
                // in which we turn off sync we stop sync service when the user toggles off all the
                // sync types.
                mAndroidSyncSettings.enableChromeSync();
            }

            RecordUserAction.record("Signin_Signin_Succeed");
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninCompletedAccessPoint",
                    mSignInState.getAccessPoint(), SigninAccessPoint.MAX);
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninReason",
                    SigninReason.SIGNIN_PRIMARY_ACCOUNT, SigninReason.MAX_VALUE + 1);
        }

        if (mSignInState.mCallback != null) {
            mSignInState.mCallback.onSignInComplete();
        }

        Log.d(TAG, "Signin completed.");
        mSignInState = null;
        notifyCallbacksWaitingForOperation();
        notifySignInAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    /**
     * Implements {@link IdentityManager.Observer}
     */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        switch (eventDetails.getEventTypeFor(ConsentLevel.SYNC)) {
            case PrimaryAccountChangeEvent.Type.SET:
                // Simply verify that the request is ongoing (mSignInState != null), as only
                // SigninManager should update IdentityManager. This is triggered by the call to
                // IdentityMutator.setPrimaryAccount
                assert mSignInState != null;
                break;
            case PrimaryAccountChangeEvent.Type.CLEARED:
                // This event can occur in two cases:
                // - Syncing account is signed out. User may choose to delete data from UI prompt
                //   if account is not managed. In this case mSigninOutState is set.
                // - RevokeSyncConsent() is called in native code. In this case the user may still
                //   be signed in with Consentlevel::SIGNIN and just lose sync privileges.
                //   If the account is managed then the data should be wiped.
                //
                //   TODO(https://crbug.com/1173016): It might be too late to get management status
                //       here. SyncService should call RevokeSyncConsent/ClearPrimaryAccount in
                //       SigninManager instead.
                if (mSignOutState == null) {
                    mSignOutState = new SignOutState(null, getManagementDomain() != null);
                }

                // TODO(https://crbug.com/1091858): Remove this after migrating the legacy code that
                //                                  uses the sync account before the native is
                //                                  loaded.
                SigninPreferencesManager.getInstance().setLegacySyncAccountEmail(null);
                disableSyncAndWipeData(mSignOutState.mShouldWipeUserData, this::finishSignOut);
                break;
            case PrimaryAccountChangeEvent.Type.NONE:
                if (eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)
                        == PrimaryAccountChangeEvent.Type.CLEARED) {
                    if (mSignOutState == null) {
                        // Don't wipe data as the user is not syncing.
                        mSignOutState = new SignOutState(null, false);
                    }
                    disableSyncAndWipeData(mSignOutState.mShouldWipeUserData, this::finishSignOut);
                }
                break;
        }
    }

    @Override
    @MainThread
    public void runAfterOperationInProgress(Runnable runnable) {
        ThreadUtils.assertOnUiThread();
        if (isOperationInProgress()) {
            mCallbacksWaitingForPendingOperation.add(runnable);
            return;
        }
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, runnable);
    }

    /**
     * Whether an operation is in progress for which we should wait before
     * scheduling users of {@link runAfterOperationInProgress}.
     */
    private boolean isOperationInProgress() {
        ThreadUtils.assertOnUiThread();
        return mSignInState != null || mSignOutState != null || mWipeUserDataInProgress;
    }

    /**
     * Maybe notifies any callbacks registered via runAfterOperationInProgress().
     *
     * The callbacks are notified in FIFO order, and each callback is only notified if there is no
     * outstanding work (either work which was outstanding at the time the callback was added, or
     * which was scheduled by subsequently dequeued callbacks).
     */
    private void notifyCallbacksWaitingForOperation() {
        ThreadUtils.assertOnUiThread();
        while (!mCallbacksWaitingForPendingOperation.isEmpty()) {
            if (isOperationInProgress()) return;
            Runnable callback = mCallbacksWaitingForPendingOperation.remove(0);
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback);
        }
    }

    /**
     * Signs out of Chrome. This method clears the signed-in username, stops sync and sends out a
     * sign-out notification on the native side.
     *
     * @param signoutSource describes the event driving the signout (e.g.
     *         {@link SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS}).
     * @param signOutCallback Callback to notify about the sign-out progress.
     * @param forceWipeUserData Whether user selected to wipe all device data.
     */
    @Override
    public void signOut(@SignoutReason int signoutSource, SignOutCallback signOutCallback,
            boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;
        // User data should not be wiped if the user is not syncing.
        assert mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC) || !forceWipeUserData;

        // Grab the management domain before nativeSignOut() potentially clears it.
        String managementDomain = getManagementDomain();
        mSignOutState =
                new SignOutState(signOutCallback, forceWipeUserData || managementDomain != null);
        Log.d(TAG, "Signing out, management domain: " + managementDomain);

        // User data will be wiped in disableSyncAndWipeData(), called from
        // onPrimaryAccountChanged().
        mIdentityMutator.clearPrimaryAccount(signoutSource,
                // Always use IGNORE_METRIC for the profile deletion argument. Chrome
                // Android has just a single-profile which is never deleted upon
                // sign-out.
                SignoutDelete.IGNORE_METRIC);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    @Override
    public String getManagementDomain() {
        return SigninManagerImplJni.get().getManagementDomain(mNativeSigninManagerAndroid);
    }

    /**
     * Aborts the current sign in.
     *
     * Package protected to allow dialog fragments to abort the signin flow.
     */
    private void abortSignIn() {
        // Ensure this function can only run once per signin flow.
        SignInState signInState = mSignInState;
        assert signInState != null;
        mSignInState = null;
        notifyCallbacksWaitingForOperation();

        if (signInState.mCallback != null) {
            signInState.mCallback.onSignInAborted();
        }

        stopApplyingCloudPolicy();

        Log.d(TAG, "Signin flow aborted.");
        notifySignInAllowedChanged();
    }

    @VisibleForTesting
    void finishSignOut() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        SignOutCallback signOutCallback = mSignOutState.mSignOutCallback;
        mSignOutState = null;

        if (signOutCallback != null) signOutCallback.signOutComplete();
        notifyCallbacksWaitingForOperation();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedOut();
        }
    }

    @CalledByNative
    private void onSigninAllowedByPolicyChanged(boolean newSigninAllowedByPolicy) {
        mSigninAllowedByPolicy = newSigninAllowedByPolicy;
        notifySignInAllowedChanged();
    }

    @Override
    public void onAccountsCookieDeletedByUserAction() {
        if (mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN) != null
                && mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC) == null) {
            // Clearing account cookies should trigger sign-out only when user is signed in
            // without sync.
            // If the user consented for sync, then the user should not be signed out,
            // since account cookies will be rebuilt by the account reconcilor.
            signOut(SignoutReason.USER_DELETED_ACCOUNT_COOKIES);
        }
    }

    /**
     * Verifies if the account is managed. Callback may be called either
     * synchronously or asynchronously depending on the availability of the
     * result.
     * @param email An email of the account.
     * @param callback The callback that will receive true if the account is managed, false
     *                 otherwise.
     */
    // TODO(crbug.com/1002408) Update API to use CoreAccountInfo instead of email
    @Override
    public void isAccountManaged(String email, final Callback<Boolean> callback) {
        assert email != null;
        CoreAccountInfo account = mIdentityManager.findExtendedAccountInfoByEmailAddress(email);
        assert account != null;
        SigninManagerImplJni.get().isAccountManaged(mNativeSigninManagerAndroid, account, callback);
    }

    @Override
    public void reloadAllAccountsFromSystem(@Nullable CoreAccountId primaryAccountId) {
        mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(primaryAccountId);
    }

    /**
     * Wipes the user's bookmarks and sync data.
     *
     * @param wipeDataCallback A callback which will be called once the data is wiped.
     *
     * TODO(crbug.com/1272911): this function and disableSyncAndWipeData() have very similar
     * functionality, but with different implementations.  Consider merging them.
     *
     * TODO(crbug.com/1272911): add test coverage for this function (including its effect on
     * notifyCallbacksWaitingForOperation()), after resolving the TODO above.
     */
    @Override
    public void wipeSyncUserData(Runnable wipeDataCallback) {
        assert !mWipeUserDataInProgress;
        mWipeUserDataInProgress = true;

        final BookmarkModel model = new BookmarkModel();
        model.finishLoadingBookmarkModel(new Runnable() {
            @Override
            public void run() {
                model.removeAllUserBookmarks();
                model.destroy();
                BrowsingDataBridge.getInstance().clearBrowsingData(
                        new BrowsingDataBridge.OnClearBrowsingDataListener() {
                            @Override
                            public void onBrowsingDataCleared() {
                                assert mWipeUserDataInProgress;
                                mWipeUserDataInProgress = false;
                                wipeDataCallback.run();
                                notifyCallbacksWaitingForOperation();
                            }
                        },
                        SYNC_DATA_TYPES, TimePeriod.ALL_TIME);
            }
        });
    }

    private boolean isGooglePlayServicesPresent() {
        return !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(
                ContextUtils.getApplicationContext());
    }

    private void fetchAndApplyCloudPolicy(CoreAccountInfo account, final Runnable callback) {
        SigninManagerImplJni.get().fetchAndApplyCloudPolicy(
                mNativeSigninManagerAndroid, account, callback);
    }

    private void stopApplyingCloudPolicy() {
        SigninManagerImplJni.get().stopApplyingCloudPolicy(mNativeSigninManagerAndroid);
    }

    private void disableSyncAndWipeData(
            boolean shouldWipeUserData, final Runnable wipeDataCallback) {
        Log.d(TAG, "On native signout, wipe user data: " + mSignOutState.mShouldWipeUserData);

        if (mSignOutState.mSignOutCallback != null) {
            mSignOutState.mSignOutCallback.preWipeData();
        }
        mAndroidSyncSettings.updateAccount(null);
        if (shouldWipeUserData) {
            SigninManagerImplJni.get().wipeProfileData(
                    mNativeSigninManagerAndroid, wipeDataCallback);
        } else {
            SigninManagerImplJni.get().wipeGoogleServiceWorkerCaches(
                    mNativeSigninManagerAndroid, wipeDataCallback);
        }
        ThreadUtils.postOnUiThread(mAccountTrackerService::onAccountsChanged);
    }

    @VisibleForTesting
    IdentityMutator getIdentityMutatorForTesting() {
        return mIdentityMutator;
    }

    /**
     * Contains all the state needed for signin. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        private final @SigninAccessPoint Integer mAccessPoint;
        final SignInCallback mCallback;

        /**
         * Contains the basic account information, which is available immediately at the start of
         * the sign-in operation.
         *
         */
        final Account mAccount;

        /**
         * Contains the full Core account info, which can be retrieved only once account seeding is
         * complete
         */
        @Nullable
        CoreAccountInfo mCoreAccountInfo;

        /**
         * State for the sign-in flow that doesn't enable sync.
         *
         * @param account The account to sign in to.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        static SignInState createForSignin(Account account, @Nullable SignInCallback callback) {
            return new SignInState(null, account, callback);
        }

        /**
         * State for the sync consent flow.
         *
         * @param accessPoint {@link SigninAccessPoint} that has initiated the sign-in.
         * @param account The account to sign in to.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        static SignInState createForSigninAndEnableSync(@SigninAccessPoint int accessPoint,
                Account account, @Nullable SignInCallback callback) {
            return new SignInState(accessPoint, account, callback);
        }

        private SignInState(@SigninAccessPoint Integer accessPoint, Account account,
                @Nullable SignInCallback callback) {
            assert account != null : "Account must be set and valid to progress.";
            mAccessPoint = accessPoint;
            mAccount = account;
            mCallback = callback;
        }

        /**
         * Getter for the access point that initiated sync consent flow. Shouldn't be called if
         * {@link #shouldTurnSyncOn()} is false.
         */
        @SigninAccessPoint
        int getAccessPoint() {
            assert mAccessPoint != null : "Not going to enable sync - no access point!";
            return mAccessPoint;
        }

        /**
         * Whether this sign-in flow should also turn on sync.
         */
        boolean shouldTurnSyncOn() {
            return mAccessPoint != null;
        }
    }

    /**
     * Contains all the state needed for sign out. Like SignInState, this forces flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignOutState {
        final @Nullable SignOutCallback mSignOutCallback;
        final boolean mShouldWipeUserData;

        /**
         * @param signOutCallback Hooks to call before/after data wiping phase of sign-out.
         * @param shouldWipeUserData Flag to wipe user data as requested by the user and enforced
         *         for managed users.
         */
        SignOutState(@Nullable SignOutCallback signOutCallback, boolean shouldWipeUserData) {
            this.mSignOutCallback = signOutCallback;
            this.mShouldWipeUserData = shouldWipeUserData;
        }
    }

    @NativeMethods
    interface Natives {
        boolean isSigninAllowedByPolicy(long nativeSigninManagerAndroid);

        boolean isForceSigninEnabled(long nativeSigninManagerAndroid);

        String extractDomainName(String email);

        void fetchAndApplyCloudPolicy(
                long nativeSigninManagerAndroid, CoreAccountInfo account, Runnable callback);

        void stopApplyingCloudPolicy(long nativeSigninManagerAndroid);

        void isAccountManaged(long nativeSigninManagerAndroid, CoreAccountInfo account,
                Callback<Boolean> callback);

        String getManagementDomain(long nativeSigninManagerAndroid);

        void wipeProfileData(long nativeSigninManagerAndroid, Runnable callback);

        void wipeGoogleServiceWorkerCaches(long nativeSigninManagerAndroid, Runnable callback);
    }
}
