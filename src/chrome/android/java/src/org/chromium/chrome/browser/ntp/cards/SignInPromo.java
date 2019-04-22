// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.Context;
import android.support.annotation.StringRes;
import android.text.format.DateUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninManager.SignInAllowedObserver;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;

/**
 * Shows a card prompting the user to sign in. This item is also an {@link OptionalLeaf}, and sign
 * in state changes control its visibility.
 */
public class SignInPromo extends OptionalLeaf {
    /**
     * Period for which promos are suppressed if signin is refused in FRE.
     */
    @VisibleForTesting
    static final long SUPPRESSION_PERIOD_MS = DateUtils.DAY_IN_MILLIS;

    private static boolean sDisablePromoForTests;

    /**
     * Whether the promo has been dismissed by the user.
     */
    private boolean mDismissed;

    /**
     * Whether signin promo can be shown.
     */
    private boolean mShowSigninPromo;

    /**
     * Whether personalized suggestions can be shown. If it's not the case, we have no reason to
     * offer the user to sign in.
     */
    private boolean mCanShowPersonalizedSuggestions;

    private final SigninObserver mSigninObserver;
    protected final SigninPromoController mSigninPromoController;
    protected final ProfileDataCache mProfileDataCache;

    protected SignInPromo() {
        Context context = ContextUtils.getApplicationContext();
        SigninManager signinManager = SigninManager.get();

        mShowSigninPromo = signinManager.isSignInAllowed() && !signinManager.isSignedInOnNative()
                && SigninPromoController.isSignInPromoAllowed();
        updateVisibility();

        int imageSize = context.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        mProfileDataCache = new ProfileDataCache(context, imageSize);
        mSigninPromoController =
                new SigninPromoController(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);

        mSigninObserver = new SigninObserver(signinManager);
    }

    /** Clear any dependencies. */
    public void destroy() {
        mSigninObserver.unregister();
    }

    /**
     * Update whether personalized suggestions can be shown and update visibility for this
     * {@link SignInPromo} accordingly.
     * @param canShow Whether personalized suggestions can be shown.
     */
    public void setCanShowPersonalizedSuggestions(boolean canShow) {
        mCanShowPersonalizedSuggestions = canShow;
        updateVisibility();
    }

    /**
     * Suppress signin promos in New Tab Page for {@link SignInPromo#SUPPRESSION_PERIOD_MS}. This
     * will not affect promos that were created before this call.
     */
    public static void temporarilySuppressPromos() {
        ChromePreferenceManager.getInstance().setNewTabPageSigninPromoSuppressionPeriodStart(
                System.currentTimeMillis());
    }

    /** @return Whether the {@link SignInPromo} should be created. */
    public static boolean shouldCreatePromo() {
        return !sDisablePromoForTests
                && !ChromePreferenceManager.getInstance().readBoolean(
                           ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, false)
                && !getSuppressionStatus();
    }

    private static boolean getSuppressionStatus() {
        long suppressedFrom = ChromePreferenceManager.getInstance()
                                      .getNewTabPageSigninPromoSuppressionPeriodStart();
        if (suppressedFrom == 0) return false;
        long currentTime = System.currentTimeMillis();
        long suppressedTo = suppressedFrom + SUPPRESSION_PERIOD_MS;
        if (suppressedFrom <= currentTime && currentTime < suppressedTo) {
            return true;
        }
        ChromePreferenceManager.getInstance().clearNewTabPageSigninPromoSuppressionPeriodStart();
        return false;
    }

    @Override
    @ItemViewType
    protected int getItemViewType() {
        return ItemViewType.PROMO;
    }

    @Override
    protected void onBindViewHolder(NewTabPageViewHolder holder) {
        ((PersonalizedPromoViewHolder) holder)
                .onBindViewHolder(mSigninPromoController, mProfileDataCache);
    }

    @Override
    public String describeForTesting() {
        return "SIGN_IN_PROMO";
    }

    /** Notify that the content for this {@link SignInPromo} has changed. */
    protected void notifyDataChanged() {
        if (isVisible()) notifyItemChanged(0, PersonalizedPromoViewHolder::update);
    }

    private void updateVisibility() {
        setVisibilityInternal(!mDismissed && mShowSigninPromo && mCanShowPersonalizedSuggestions);
    }

    @Override
    protected boolean canBeDismissed() {
        return true;
    }

    /** Hides the sign in promo and sets a preference to make sure it is not shown again. */
    @Override
    public void dismiss(Callback<String> itemRemovedCallback) {
        mDismissed = true;
        updateVisibility();

        ChromePreferenceManager.getInstance().writeBoolean(
                ChromePreferenceManager.NTP_SIGNIN_PROMO_DISMISSED, true);

        final @StringRes int promoHeader = mSigninPromoController.getDescriptionStringId();

        mSigninObserver.unregister();
        itemRemovedCallback.onResult(ContextUtils.getApplicationContext().getString(promoHeader));
    }

    @VisibleForTesting
    public static void setDisablePromoForTests(boolean disable) {
        sDisablePromoForTests = disable;
    }

    @VisibleForTesting
    public SigninObserver getSigninObserverForTesting() {
        return mSigninObserver;
    }

    @VisibleForTesting
    public class SigninObserver implements SignInStateObserver, SignInAllowedObserver,
                                           ProfileDataCache.Observer, AccountsChangeObserver {
        private final SigninManager mSigninManager;

        /** Guards {@link #unregister()}, which can be called multiple times. */
        private boolean mUnregistered;

        private SigninObserver(SigninManager signinManager) {
            mSigninManager = signinManager;
            mSigninManager.addSignInAllowedObserver(this);
            mSigninManager.addSignInStateObserver(this);

            mProfileDataCache.addObserver(this);
            AccountManagerFacade.get().addObserver(this);
        }

        private void unregister() {
            if (mUnregistered) return;
            mUnregistered = true;

            mSigninManager.removeSignInAllowedObserver(this);
            mSigninManager.removeSignInStateObserver(this);
            mProfileDataCache.removeObserver(this);
            AccountManagerFacade.get().removeObserver(this);
        }

        // SignInAllowedObserver implementation.
        @Override
        public void onSignInAllowedChanged() {
            // Listening to onSignInAllowedChanged is important for the FRE. Sign in is not allowed
            // until it is completed, but the NTP is initialised before the FRE is even shown. By
            // implementing this we can show the promo if the user did not sign in during the FRE.
            mShowSigninPromo = mSigninManager.isSignInAllowed()
                    && SigninPromoController.isSignInPromoAllowed();
            updateVisibility();
        }

        // SignInStateObserver implementation.
        @Override
        public void onSignedIn() {
            mShowSigninPromo = false;
            updateVisibility();
        }

        @Override
        public void onSignedOut() {
            mShowSigninPromo = mSigninManager.isSignInAllowed()
                    && SigninPromoController.isSignInPromoAllowed();
            updateVisibility();
        }

        // AccountsChangeObserver implementation.
        @Override
        public void onAccountsChanged() {
            notifyDataChanged();
        }

        // ProfileDataCache.Observer implementation.
        @Override
        public void onProfileDataUpdated(String accountId) {
            notifyDataChanged();
        }
    }
}
