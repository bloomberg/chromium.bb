// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncPromoView;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SigninPromoController;
import org.chromium.chrome.browser.ui.signin.SigninPromoController.SyncPromoState;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Class that manages all the logic and UI behind the signin promo header in the bookmark
 * content UI. The header is shown only on certain situations, (e.g., not signed in).
 */
class BookmarkPromoHeader implements SyncService.SyncStateChangedListener, SignInStateObserver,
                                     ProfileDataCache.Observer, AccountsChangeObserver {
    // TODO(kkimlabs): Figure out the optimal number based on UMA data.
    private static final int MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT = 10;

    private static @Nullable @SyncPromoState Integer sPromoStateForTests;

    private final Context mContext;
    private final SigninManager mSignInManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Runnable mPromoHeaderChangeAction;

    private @Nullable ProfileDataCache mProfileDataCache;
    private final @Nullable SigninPromoController mSigninPromoController;
    private @SyncPromoState int mPromoState = SyncPromoState.NO_PROMO;
    private final @Nullable SyncService mSyncService;

    /**
     * Initializes the class. Note that this will start listening to signin related events and
     * update itself if needed.
     */
    BookmarkPromoHeader(Context context, Runnable promoHeaderChangeAction) {
        mContext = context;
        mPromoHeaderChangeAction = promoHeaderChangeAction;

        mSyncService = SyncService.get();
        if (mSyncService != null) mSyncService.addSyncStateChangedListener(this);

        mSignInManager = IdentityServicesProvider.get().getSigninManager(
                Profile.getLastUsedRegularProfile());
        mSignInManager.addSignInStateObserver(this);

        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        if (SigninPromoController.canShowSyncPromo(SigninAccessPoint.BOOKMARK_MANAGER)) {
            mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
            mProfileDataCache.addObserver(this);
            mSigninPromoController = new SigninPromoController(
                    SigninAccessPoint.BOOKMARK_MANAGER, SyncConsentActivityLauncherImpl.get());
            mAccountManagerFacade.addObserver(this);
        } else {
            mProfileDataCache = null;
            mSigninPromoController = null;
        }
        updatePromoState();
    }

    /**
     * Clean ups the class. Must be called once done using this class.
     */
    void destroy() {
        if (mSyncService != null) mSyncService.removeSyncStateChangedListener(this);

        if (mSigninPromoController != null) {
            mAccountManagerFacade.removeObserver(this);
            mProfileDataCache.removeObserver(this);
            mSigninPromoController.onPromoDestroyed();
        }

        mSignInManager.removeSignInStateObserver(this);
    }

    /**
     * @return The current state of the promo.
     */
    @SyncPromoState
    int getPromoState() {
        return mPromoState;
    }

    /**
     * @return Personalized signin promo header {@link ViewHolder} instance that can be used with
     *         {@link RecyclerView}.
     */
    ViewHolder createPersonalizedSigninAndSyncPromoHolder(ViewGroup parent) {
        View view = LayoutInflater.from(mContext).inflate(
                R.layout.personalized_signin_promo_view_bookmarks, parent, false);

        // ViewHolder is abstract and it cannot be instantiated directly.
        return new ViewHolder(view) {};
    }

    /**
     * @return Sync promo header {@link ViewHolder} instance that can be used with
     *         {@link RecyclerView}.
     */
    ViewHolder createSyncPromoHolder(ViewGroup parent) {
        SyncPromoView view = SyncPromoView.create(parent, SigninAccessPoint.BOOKMARK_MANAGER);

        // ViewHolder is abstract and it cannot be instantiated directly.
        return new ViewHolder(view) {};
    }

    /**
     * Sets up the sync promo view.
     */
    void setUpSyncPromoView(PersonalizedSigninPromoView view) {
        mSigninPromoController.setUpSyncPromoView(
                mProfileDataCache, view, this::setPersonalizedSigninPromoDeclined);
    }

    /**
     * Detaches the previously configured {@link PersonalizedSigninPromoView}.
     */
    void detachPersonalizePromoView() {
        if (mSigninPromoController != null) mSigninPromoController.detach();
    }

    /**
     * Saves that the personalized signin promo was declined and updates the UI.
     */
    private void setPersonalizedSigninPromoDeclined() {
        mPromoState = calculatePromoState();
        triggerPromoUpdate();
    }

    /**
     * @return Whether the personalized signin promo should be shown to user.
     */
    private boolean shouldShowBookmarkSigninPromo() {
        return mSignInManager.isSignInAllowed()
                && SigninPromoController.canShowSyncPromo(SigninAccessPoint.BOOKMARK_MANAGER);
    }

    private @SyncPromoState int calculatePromoState() {
        if (sPromoStateForTests != null) {
            return sPromoStateForTests;
        }

        if (mSyncService == null) {
            // |mSyncService| will remain null until the next browser startup, so no sense in
            // offering any promo.
            return SyncPromoState.NO_PROMO;
        }

        if (!mSyncService.isSyncAllowedByPlatform()) {
            return SyncPromoState.NO_PROMO;
        }

        if (!mSignInManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SYNC)) {
            if (!shouldShowBookmarkSigninPromo()) {
                return SyncPromoState.NO_PROMO;
            }

            return mSignInManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN)
                    ? SyncPromoState.PROMO_FOR_SIGNED_IN_STATE
                    : SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE;
        }

        boolean impressionLimitNotReached =
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT)
                < MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT;
        if (!mSyncService.isSyncRequested() && impressionLimitNotReached) {
            return SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE;
        }
        return SyncPromoState.NO_PROMO;
    }

    private void updatePromoState() {
        final @SyncPromoState int newState = calculatePromoState();
        if (newState == mPromoState) return;

        // PROMO_SYNC state and it's impression counts is not tracked by SigninPromoController.
        final boolean hasSyncPromoStateChangedtoShown =
                (mPromoState == SyncPromoState.NO_PROMO
                        || mPromoState == SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE)
                && (newState == SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE
                        || newState == SyncPromoState.PROMO_FOR_SIGNED_IN_STATE);
        if (mSigninPromoController != null && hasSyncPromoStateChangedtoShown) {
            mSigninPromoController.increasePromoShowCount();
        }
        if (newState == SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE) {
            SharedPreferencesManager.getInstance().incrementInt(
                    ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT);
        }
        mPromoState = newState;
    }

    // SyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        updatePromoState();
        triggerPromoUpdate();
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        updatePromoState();
        triggerPromoUpdate();
    }

    @Override
    public void onSignedOut() {
        updatePromoState();
        triggerPromoUpdate();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        triggerPromoUpdate();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onAccountsChanged() {
        triggerPromoUpdate();
    }

    private void triggerPromoUpdate() {
        detachPersonalizePromoView();
        mPromoHeaderChangeAction.run();
    }

    /**
     * Forces the promo state to a particular value for testing purposes.
     * @param promoState The promo state to which the header will be set to.
     */
    @VisibleForTesting
    static void forcePromoStateForTests(@Nullable @SyncPromoState Integer promoState) {
        sPromoStateForTests = promoState;
    }
}
