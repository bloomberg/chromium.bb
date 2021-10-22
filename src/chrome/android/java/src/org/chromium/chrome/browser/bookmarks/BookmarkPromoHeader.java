// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
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
import org.chromium.chrome.browser.signin.ui.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that manages all the logic and UI behind the signin promo header in the bookmark
 * content UI. The header is shown only on certain situations, (e.g., not signed in).
 */
class BookmarkPromoHeader implements SyncService.SyncStateChangedListener, SignInStateObserver,
                                     ProfileDataCache.Observer, AccountsChangeObserver {
    /**
     * Specifies the various states in which the Bookmarks promo can be.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PromoState.PROMO_NONE, PromoState.PROMO_SIGNIN_PERSONALIZED,
            PromoState.PROMO_SYNC_PERSONALIZED, PromoState.PROMO_SYNC})
    @interface PromoState {
        int PROMO_NONE = 0;
        int PROMO_SIGNIN_PERSONALIZED = 1;
        int PROMO_SYNC_PERSONALIZED = 2;
        int PROMO_SYNC = 3;
    }

    // TODO(kkimlabs): Figure out the optimal number based on UMA data.
    private static final int MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT = 10;

    private static @Nullable @PromoState Integer sPromoStateForTests;

    private final Context mContext;
    private final SigninManager mSignInManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Runnable mPromoHeaderChangeAction;

    private @Nullable ProfileDataCache mProfileDataCache;
    private final @Nullable SigninPromoController mSigninPromoController;
    private @PromoState int mPromoState = PromoState.PROMO_NONE;
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
    @PromoState
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

    private @PromoState int calculatePromoState() {
        if (sPromoStateForTests != null) {
            return sPromoStateForTests;
        }

        if (mSyncService == null) {
            // |mSyncService| will remain null until the next browser startup, so no sense in
            // offering any promo.
            return PromoState.PROMO_NONE;
        }

        if (!mSyncService.isSyncAllowedByPlatform()) {
            return PromoState.PROMO_NONE;
        }

        if (!mSignInManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SYNC)) {
            if (!shouldShowBookmarkSigninPromo()) {
                return PromoState.PROMO_NONE;
            }

            return mSignInManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN)
                    ? PromoState.PROMO_SYNC_PERSONALIZED
                    : PromoState.PROMO_SIGNIN_PERSONALIZED;
        }

        boolean impressionLimitNotReached =
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT)
                < MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT;
        if (!mSyncService.isSyncRequested() && impressionLimitNotReached) {
            return PromoState.PROMO_SYNC;
        }
        return PromoState.PROMO_NONE;
    }

    private void updatePromoState() {
        final @PromoState int newState = calculatePromoState();
        if (newState == mPromoState) return;

        // PROMO_SYNC state and it's impression counts is not tracked by SigninPromoController.
        final boolean hasSyncPromoStateChangedtoShown =
                (mPromoState == PromoState.PROMO_NONE || mPromoState == PromoState.PROMO_SYNC)
                && (newState == PromoState.PROMO_SIGNIN_PERSONALIZED
                        || newState == PromoState.PROMO_SYNC_PERSONALIZED);
        if (mSigninPromoController != null && hasSyncPromoStateChangedtoShown) {
            mSigninPromoController.increasePromoShowCount();
        }
        if (newState == PromoState.PROMO_SYNC) {
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
    static void forcePromoStateForTests(@Nullable @PromoState Integer promoState) {
        sPromoStateForTests = promoState;
    }
}
