// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.privacy.PrivacyPreferencesManager;
import org.chromium.chrome.browser.services.AndroidEduAndChildAccountHelper;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.ChromeSigninController;

import java.util.List;

/**
 * A helper to determine what should be the sequence of First Run Experience screens, and whether
 * it should be run.
 *
 * Usage:
 * new FirstRunFlowSequencer(activity, launcherProvidedProperties) {
 *     override onFlowIsKnown
 * }.start();
 */
public abstract class FirstRunFlowSequencer  {
    private static final String TAG = "firstrun";

    private final Activity mActivity;

    // The following are initialized via initializeSharedState().
    private boolean mIsAndroidEduDevice;
    private @ChildAccountStatus.Status int mChildAccountStatus;
    private List<Account> mGoogleAccounts;
    private boolean mForceEduSignIn;

    /**
     * Callback that is called once the flow is determined.
     * If the properties is null, the First Run experience needs to finish and
     * restart the original intent if necessary.
     * @param freProperties Properties to be used in the First Run activity, or null.
     */
    public abstract void onFlowIsKnown(Bundle freProperties);

    public FirstRunFlowSequencer(Activity activity) {
        mActivity = activity;
    }

    /**
     * Starts determining parameters for the First Run.
     * Once finished, calls onFlowIsKnown().
     */
    public void start() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
                || ApiCompatibilityUtils.isDemoUser(mActivity)) {
            onFlowIsKnown(null);
            return;
        }

        new AndroidEduAndChildAccountHelper() {
            @Override
            public void onParametersReady() {
                initializeSharedState(isAndroidEduDevice(), getChildAccountStatus());
                processFreEnvironmentPreNative();
            }
        }.start();
    }

    @VisibleForTesting
    protected boolean isFirstRunFlowComplete() {
        return FirstRunStatus.getFirstRunFlowComplete();
    }

    @VisibleForTesting
    protected boolean isSignedIn() {
        return ChromeSigninController.get().isSignedIn();
    }

    @VisibleForTesting
    protected boolean isSyncAllowed() {
        SigninManager signinManager = IdentityServicesProvider.getSigninManager();
        return FeatureUtilities.canAllowSync() && !signinManager.isSigninDisabledByPolicy()
                && signinManager.isSigninSupported();
    }

    @VisibleForTesting
    protected List<Account> getGoogleAccounts() {
        return AccountManagerFacade.get().tryGetGoogleAccounts();
    }

    @VisibleForTesting
    protected boolean hasAnyUserSeenToS() {
        return ToSAckedReceiver.checkAnyUserHasSeenToS();
    }

    @VisibleForTesting
    protected boolean shouldSkipFirstUseHints() {
        return ApiCompatibilityUtils.shouldSkipFirstUseHints(mActivity.getContentResolver());
    }

    @VisibleForTesting
    protected boolean isFirstRunEulaAccepted() {
        return PrefServiceBridge.getInstance().isFirstRunEulaAccepted();
    }

    protected boolean shouldShowDataReductionPage() {
        return !DataReductionProxySettings.getInstance().isDataReductionProxyManaged()
                && DataReductionProxySettings.getInstance().isDataReductionProxyFREPromoAllowed();
    }

    @VisibleForTesting
    protected boolean shouldShowSearchEnginePage() {
        @LocaleManager.SearchEnginePromoType
        int searchPromoType = LocaleManager.getInstance().getSearchEnginePromoShowType();
        return searchPromoType == LocaleManager.SearchEnginePromoType.SHOW_NEW
                || searchPromoType == LocaleManager.SearchEnginePromoType.SHOW_EXISTING;
    }

    @VisibleForTesting
    protected void setDefaultMetricsAndCrashReporting() {
        PrivacyPreferencesManager.getInstance().setUsageAndCrashReporting(
                FirstRunActivity.DEFAULT_METRICS_AND_CRASH_REPORTING);
    }

    @VisibleForTesting
    protected void setFirstRunFlowSignInComplete() {
        FirstRunSignInProcessor.setFirstRunFlowSignInComplete(true);
    }

    void initializeSharedState(
            boolean isAndroidEduDevice, @ChildAccountStatus.Status int childAccountStatus) {
        mIsAndroidEduDevice = isAndroidEduDevice;
        mChildAccountStatus = childAccountStatus;
        mGoogleAccounts = getGoogleAccounts();
        // EDU devices should always have exactly 1 google account, which will be automatically
        // signed-in. All FRE screens are skipped in this case.
        mForceEduSignIn = mIsAndroidEduDevice && mGoogleAccounts.size() == 1 && !isSignedIn();
    }

    void processFreEnvironmentPreNative() {
        if (isFirstRunFlowComplete()) {
            assert isFirstRunEulaAccepted();
            // We do not need any interactive FRE.
            onFlowIsKnown(null);
            return;
        }

        Bundle freProperties = new Bundle();

        // In the full FRE we always show the Welcome page, except on EDU devices.
        boolean showWelcomePage = !mForceEduSignIn;
        freProperties.putBoolean(FirstRunActivity.SHOW_WELCOME_PAGE, showWelcomePage);
        freProperties.putInt(SigninFirstRunFragment.CHILD_ACCOUNT_STATUS, mChildAccountStatus);

        // Initialize usage and crash reporting according to the default value.
        // The user can explicitly enable or disable the reporting on the Welcome page.
        // This is controlled by the administrator via a policy on EDU devices.
        setDefaultMetricsAndCrashReporting();

        onFlowIsKnown(freProperties);
        if (ChildAccountStatus.isChild(mChildAccountStatus) || mForceEduSignIn) {
            // Child and Edu forced signins are processed independently.
            setFirstRunFlowSignInComplete();
        }
    }

    /**
     * Called onNativeInitialized() a given flow as completed.
     * @param freProperties Resulting FRE properties bundle.
     */
    public void onNativeInitialized(Bundle freProperties) {
        // We show the sign-in page if sync is allowed, and not signed in, and this is not
        // an EDU device, and
        // - no "skip the first use hints" is set, or
        // - "skip the first use hints" is set, but there is at least one account.
        boolean offerSignInOk = isSyncAllowed() && !isSignedIn() && !mForceEduSignIn
                && (!shouldSkipFirstUseHints() || !mGoogleAccounts.isEmpty());
        freProperties.putBoolean(FirstRunActivity.SHOW_SIGNIN_PAGE, offerSignInOk);
        if (mForceEduSignIn || ChildAccountStatus.isChild(mChildAccountStatus)) {
            // If the device is an Android EDU device or has a child account, there should be
            // exactly account on the device. Force sign-in in to that account.
            freProperties.putString(
                    SigninFirstRunFragment.FORCE_SIGNIN_ACCOUNT_TO, mGoogleAccounts.get(0).name);
        }

        freProperties.putBoolean(
                FirstRunActivity.SHOW_DATA_REDUCTION_PAGE, shouldShowDataReductionPage());
        freProperties.putBoolean(
                FirstRunActivity.SHOW_SEARCH_ENGINE_PAGE, shouldShowSearchEnginePage());
    }

    /**
     * Marks a given flow as completed.
     * @param signInAccountName The account name for the pending sign-in request. (Or null)
     * @param showSignInSettings Whether the user selected to see the settings once signed in.
     */
    public static void markFlowAsCompleted(String signInAccountName, boolean showSignInSettings) {
        // When the user accepts ToS in the Setup Wizard (see ToSAckedReceiver), we do not
        // show the ToS page to the user because the user has already accepted one outside FRE.
        if (!PrefServiceBridge.getInstance().isFirstRunEulaAccepted()) {
            PrefServiceBridge.getInstance().setEulaAccepted();
        }

        // Mark the FRE flow as complete and set the sign-in flow preferences if necessary.
        FirstRunSignInProcessor.finalizeFirstRunFlowState(signInAccountName, showSignInSettings);
    }

    /**
     * Checks if the First Run needs to be launched.
     * @param context The context.
     * @param fromIntent The intent that was used to launch Chrome.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @return Whether the First Run Experience needs to be launched.
     */
    public static boolean checkIfFirstRunIsNecessary(
            Context context, Intent fromIntent, boolean preferLightweightFre) {
        // If FRE is disabled (e.g. in tests), proceed directly to the intent handling.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
                || ApiCompatibilityUtils.isDemoUser(context)) {
            return false;
        }

        // If Chrome isn't opened via the Chrome icon, and the user accepted the ToS
        // in the Setup Wizard, skip any First Run Experience screens and proceed directly
        // to the intent handling.
        final boolean fromChromeIcon =
                fromIntent != null && TextUtils.equals(fromIntent.getAction(), Intent.ACTION_MAIN);
        if (!fromChromeIcon && ToSAckedReceiver.checkAnyUserHasSeenToS()) return false;

        if (FirstRunStatus.getFirstRunFlowComplete()) {
            // Promo pages are removed, so there is nothing else to show in FRE.
            return false;
        }
        return !preferLightweightFre
                || (!FirstRunStatus.shouldSkipWelcomePage()
                           && !FirstRunStatus.getLightweightFirstRunFlowComplete());
    }

    /**
     * Tries to launch the First Run Experience.  If the Activity was launched with the wrong Intent
     * flags, we first relaunch it to make sure it runs in its own task, then trigger First Run.
     *
     * @param caller               Activity instance that is checking if first run is necessary.
     * @param fromIntent           Intent used to launch the caller.
     * @param requiresBroadcast    Whether or not the Intent triggers a BroadcastReceiver.
     * @param preferLightweightFre Whether to prefer the Lightweight First Run Experience.
     * @return Whether startup must be blocked (e.g. via Activity#finish or dropping the Intent).
     */
    public static boolean launch(Context caller, Intent fromIntent, boolean requiresBroadcast,
            boolean preferLightweightFre) {
        // Check if the user needs to go through First Run at all.
        if (!checkIfFirstRunIsNecessary(caller, fromIntent, preferLightweightFre)) return false;

        String intentUrl = IntentHandler.getUrlFromIntent(fromIntent);
        Uri uri = intentUrl != null ? Uri.parse(intentUrl) : null;
        if (uri != null && UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
            caller.grantUriPermission(
                    caller.getPackageName(), uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }

        Log.d(TAG, "Redirecting user through FRE.");
        if ((fromIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0) {
            FreIntentCreator intentCreator = AppHooks.get().createFreIntentCreator();
            Intent freIntent = intentCreator.create(
                    caller, fromIntent, requiresBroadcast, preferLightweightFre);

            if (!(caller instanceof Activity)) freIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            boolean isVrIntent = VrModuleProvider.getIntentDelegate().isVrIntent(fromIntent);
            if (isVrIntent) {
                freIntent =
                        VrModuleProvider.getIntentDelegate().setupVrFreIntent(caller, freIntent);
                // We cannot access Chrome right now, e.g. because the VR module is not installed.
                if (freIntent == null) return true;
            }
            IntentUtils.safeStartActivity(caller, freIntent);
        } else {
            // First Run requires that the Intent contains NEW_TASK so that it doesn't sit on top
            // of something else.
            Intent newIntent = new Intent(fromIntent);
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.safeStartActivity(caller, newIntent);
        }
        return true;
    }
}
