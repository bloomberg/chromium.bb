// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.ui.SigninActivityLauncher;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * SigninActivityLauncher creates the proper intent and then launches the
 * SigninActivity in different scenarios.
 */
public class SigninActivityLauncherImpl implements SigninActivityLauncher {
    private static SigninActivityLauncherImpl sLauncher;

    /**
     * Singleton instance getter
     */
    public static SigninActivityLauncherImpl get() {
        if (sLauncher == null) {
            sLauncher = new SigninActivityLauncherImpl();
        }
        return sLauncher;
    }

    @VisibleForTesting
    public static void setLauncherForTest(@Nullable SigninActivityLauncherImpl launcher) {
        sLauncher = launcher;
    }

    private SigninActivityLauncherImpl() {}

    /**
     * Launches the SigninActivity with default sign-in flow from personalized sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    @Override
    public void launchActivityForPromoDefaultFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SigninFragment.createArgumentsForPromoDefaultFlow(accessPoint, accountName));
    }

    /**
     * Launches the SigninActivity with "Choose account" sign-in flow from personalized
     * sign-in promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @param accountName The account to preselect or null to preselect the default account.
     */
    @Override
    public void launchActivityForPromoChooseAccountFlow(
            Context context, @SigninAccessPoint int accessPoint, String accountName) {
        launchInternal(context,
                SigninFragment.createArgumentsForPromoChooseAccountFlow(accessPoint, accountName));
    }

    /**
     * Launches the SigninActivity with "New account" sign-in flow from personalized sign-in
     * promo.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    @Override
    public void launchActivityForPromoAddAccountFlow(
            Context context, @SigninAccessPoint int accessPoint) {
        launchInternal(context, SigninFragment.createArgumentsForPromoAddAccountFlow(accessPoint));
    }

    /**
     * Launches the signin activity.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     */
    @Override
    public void launchActivity(Context context, @SigninAccessPoint int accessPoint) {
        launchInternal(context, SigninFragment.createArguments(accessPoint));
    }

    /**
     * Launches the {@link SigninActivity} if signin is allowed.
     * @param context A {@link Context} object.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @return a boolean indicating if the {@link SigninActivity} is launched.
     */
    @Override
    public boolean launchActivityIfAllowed(Context context, @SigninAccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        if (signinManager.isSignInAllowed()) {
            launchActivity(context, accessPoint);
            return true;
        }
        if (signinManager.isSigninDisabledByPolicy()) {
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        }
        return false;
    }

    private void launchInternal(Context context, Bundle fragmentArgs) {
        Intent intent = SigninActivity.createIntent(context, fragmentArgs);
        context.startActivity(intent);
    }
}
