// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity_disc;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;

/**
 * Handles displaying IdentityDisc on toolbar depending on several conditions
 * (user sign-in state, whether NTP is shown)
 */
public class IdentityDiscController implements NativeInitObserver, ProfileDataCache.Observer,
                                               IdentityManager.Observer, ButtonDataProvider {
    // Visual state of Identity Disc.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({IdentityDiscState.NONE, IdentityDiscState.SMALL, IdentityDiscState.LARGE})
    private @interface IdentityDiscState {
        // Identity Disc is hidden.
        int NONE = 0;

        // Small Identity Disc is shown.
        int SMALL = 1;

        // Large Identity Disc is shown.
        int LARGE = 2;
        int MAX = 3;
    }

    // Context is used for fetching resources and launching preferences page.
    private final Context mContext;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ObservableSupplier<Boolean> mBottomToolbarVisibilitySupplier;

    private @Nullable Callback<Boolean> mBottomToolbarVisibilityObserver;

    // We observe IdentityManager to receive primary account state change notifications.
    private IdentityManager mIdentityManager;

    // ProfileDataCache facilitates retrieving profile picture. Separate objects are maintained
    // for different visual states to cache profile pictures of different size.
    // mProfileDataCache[IdentityDiscState.NONE] should always be null since in this state
    // Identity Disc is not visible.
    private ProfileDataCache mProfileDataCache[] = new ProfileDataCache[IdentityDiscState.MAX];

    // Identity disc visibility state.
    @IdentityDiscState
    private int mState = IdentityDiscState.NONE;

    private ButtonData mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private boolean mNativeIsInitialized;

    /**
     *
     * @param context The Context for retrieving resources, launching preference activiy, etc.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g. native
     *         initialization completing.
     * @param bottomToolbarVisibilitySupplier Supplier that queries and updates the visibility of
     *         the bottom toolbar.
     */
    public IdentityDiscController(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<Boolean> bottomToolbarVisibilitySupplier) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mBottomToolbarVisibilitySupplier = bottomToolbarVisibilitySupplier;
        mActivityLifecycleDispatcher.register(this);

        mButtonData = new ButtonData(false, null,
                view
                -> {
                    recordIdentityDiscUsed();
                    SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                    settingsLauncher.launchSettingsActivity(
                            mContext, SyncAndServicesSettings.class);
                },
                R.string.accessibility_toolbar_btn_identity_disc, false,
                new IPHCommandBuilder(mContext.getResources(),
                        FeatureConstants.IDENTITY_DISC_FEATURE, R.string.iph_identity_disc_text,
                        R.string.iph_identity_disc_accessibility_text));
    }

    /**
     * Registers itself to observe sign-in and sync status events.
     */
    @Override
    public void onFinishNativeInitialization() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;
        mNativeIsInitialized = true;

        mIdentityManager = IdentityServicesProvider.get().getIdentityManager();
        mIdentityManager.addObserver(this);

        mBottomToolbarVisibilityObserver = (bottomToolbarIsVisible)
                -> notifyObservers(mIdentityManager.getPrimaryAccountInfo() != null);
        mBottomToolbarVisibilitySupplier.addObserver(mBottomToolbarVisibilityObserver);
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(Tab tab) {
        boolean isNtp = tab != null && tab.getNativePage() instanceof NewTabPage;
        if (!isNtp) {
            mButtonData.canShow = false;
            return mButtonData;
        }

        calculateButtonData(mBottomToolbarVisibilitySupplier.get());
        return mButtonData;
    }

    public ButtonData getForStartSurface(@OverviewModeState int overviewModeState) {
        if (overviewModeState != OverviewModeState.SHOWN_HOMEPAGE) {
            mButtonData.canShow = false;
            return mButtonData;
        }

        calculateButtonData(false);
        return mButtonData;
    }

    private void calculateButtonData(boolean bottomToolbarVisible) {
        if (!mNativeIsInitialized) {
            assert !mButtonData.canShow;
            return;
        }

        String email = CoreAccountInfo.getEmailFrom(
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC));
        boolean canShowIdentityDisc = email != null;
        boolean menuBottomOnBottom =
                bottomToolbarVisible && BottomToolbarVariationManager.isMenuButtonOnBottom();

        mState = !canShowIdentityDisc
                ? IdentityDiscState.NONE
                : menuBottomOnBottom ? IdentityDiscState.LARGE : IdentityDiscState.SMALL;
        ensureProfileDataCache(email, mState);

        if (mState != IdentityDiscState.NONE) {
            mButtonData.drawable = getProfileImage(email);
            mButtonData.canShow = true;
        } else {
            mButtonData.canShow = false;
        }
    }

    /**
     * Creates and initializes ProfileDataCache if it wasn't created previously. Subscribes
     * IdentityDiscController for profile data updates.
     */
    private void ensureProfileDataCache(String accountName, @IdentityDiscState int state) {
        if (state == IdentityDiscState.NONE || mProfileDataCache[state] != null) return;

        @DimenRes
        int dimension_id =
                (state == IdentityDiscState.SMALL) ? R.dimen.toolbar_identity_disc_size
                                                   : R.dimen.toolbar_identity_disc_size_duet;
        int imageSize = mContext.getResources().getDimensionPixelSize(dimension_id);
        ProfileDataCache profileDataCache = new ProfileDataCache(mContext, imageSize);
        profileDataCache.addObserver(this);
        profileDataCache.update(Collections.singletonList(accountName));
        mProfileDataCache[state] = profileDataCache;
    }

    /**
     * Returns Profile picture Drawable. The size of the image corresponds to current visual state.
     */
    private Drawable getProfileImage(String accountName) {
        assert mState != IdentityDiscState.NONE;
        return mProfileDataCache[mState].getProfileDataOrDefault(accountName).getImage();
    }

    /**
     * Hides IdentityDisc and resets all ProfileDataCache objects. Used for flushing cached images
     * when sign-in state changes.
     */
    private void resetIdentityDiscCache() {
        for (int i = 0; i < IdentityDiscState.MAX; i++) {
            if (mProfileDataCache[i] != null) {
                assert i != IdentityDiscState.NONE;
                mProfileDataCache[i].removeObserver(this);
                mProfileDataCache[i] = null;
            }
        }
    }

    private void notifyObservers(boolean hint) {
        for (ButtonDataObserver observer : mObservers) {
            observer.buttonDataChanged(hint);
        }
    }

    /**
     * Called after profile image becomes available. Updates the image on toolbar button.
     */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        if (mState == IdentityDiscState.NONE) return;
        assert mProfileDataCache[mState] != null;

        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo();
        if (accountEmail.equals(CoreAccountInfo.getEmailFrom(accountInfo))) {
            notifyObservers(true);
        }
    }

    // IdentityManager.Observer implementation.
    @Override
    public void onPrimaryAccountSet(CoreAccountInfo account) {
        resetIdentityDiscCache();
        notifyObservers(true);
    }

    @Override
    public void onPrimaryAccountCleared(CoreAccountInfo account) {
        notifyObservers(false);
    }

    /**
     * Call to tear down dependencies.
     */
    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        for (int i = 0; i < IdentityDiscState.MAX; i++) {
            if (mProfileDataCache[i] != null) {
                mProfileDataCache[i].removeObserver(this);
                mProfileDataCache[i] = null;
            }
        }

        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
            mIdentityManager = null;
        }

        if (mBottomToolbarVisibilityObserver != null) {
            mBottomToolbarVisibilitySupplier.removeObserver(mBottomToolbarVisibilityObserver);
            mBottomToolbarVisibilityObserver = null;
        }
    }

    /**
     * Records IdentityDisc usage with feature engagement tracker. This signal can be used to decide
     * whether to show in-product help.
     */
    private void recordIdentityDiscUsed() {
        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        Profile profile = Profile.getLastUsedRegularProfile();
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.IDENTITY_DISC_USED);
        RecordUserAction.record("MobileToolbarIdentityDiscTap");
    }
}
