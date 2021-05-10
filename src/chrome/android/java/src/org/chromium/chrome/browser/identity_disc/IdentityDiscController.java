// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity_disc;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

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
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver = this::setProfile;

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

    private ButtonDataImpl mButtonData;
    private ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private boolean mNativeIsInitialized;

    /**
     *
     * @param context The Context for retrieving resources, launching preference activiy, etc.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g. native
     *         initialization completing.
     */
    public IdentityDiscController(Context context,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mProfileSupplier = profileSupplier;
        mActivityLifecycleDispatcher.register(this);

        mButtonData = new ButtonDataImpl(/*canShow=*/false, /*drawable=*/null,
                /*onClickListener=*/
                view
                -> {
                    recordIdentityDiscUsed();
                    SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                    settingsLauncher.launchSettingsActivity(mContext,
                            ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                                    ? MainSettings.class
                                    : SyncAndServicesSettings.class);
                },
                R.string.accessibility_toolbar_btn_identity_disc, /*supportsTinting=*/false,
                new IPHCommandBuilder(mContext.getResources(),
                        FeatureConstants.IDENTITY_DISC_FEATURE, R.string.iph_identity_disc_text,
                        R.string.iph_identity_disc_accessibility_text),
                /*isEnabled=*/true);
    }

    /**
     * Registers itself to observe sign-in and sync status events.
     */
    @Override
    public void onFinishNativeInitialization() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityLifecycleDispatcher = null;
        mNativeIsInitialized = true;

        mProfileSupplier.addObserver(mProfileSupplierObserver);
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
            mButtonData.setCanShow(false);
            return mButtonData;
        }

        calculateButtonData();
        return mButtonData;
    }

    public ButtonData getForStartSurface(@StartSurfaceState int overviewModeState) {
        if (overviewModeState != StartSurfaceState.SHOWN_HOMEPAGE) {
            mButtonData.setCanShow(false);
            return mButtonData;
        }

        calculateButtonData();
        return mButtonData;
    }

    private void calculateButtonData() {
        if (!mNativeIsInitialized) {
            assert !mButtonData.canShow();
            return;
        }

        String email = CoreAccountInfo.getEmailFrom(getSignedInAccountInfo());
        mState = email == null ? IdentityDiscState.NONE : IdentityDiscState.SMALL;
        ensureProfileDataCache(email, mState);

        if (mState != IdentityDiscState.NONE) {
            mButtonData.setButtonSpec(
                    buttonSpecWithDrawable(mButtonData.getButtonSpec(), getProfileImage(email)));
            mButtonData.setCanShow(true);
        } else {
            mButtonData.setCanShow(false);
        }
    }

    private static ButtonSpec buttonSpecWithDrawable(ButtonSpec buttonSpec, Drawable drawable) {
        if (buttonSpec.getDrawable() == drawable) return buttonSpec;
        return new ButtonSpec(drawable, buttonSpec.getOnClickListener(),
                buttonSpec.getContentDescriptionResId(), buttonSpec.getSupportsTinting(),
                buttonSpec.getIPHCommandBuilder());
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
        ProfileDataCache profileDataCache =
                ProfileDataCache.createWithoutBadge(mContext, dimension_id);
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

        if (accountEmail.equals(CoreAccountInfo.getEmailFrom(getSignedInAccountInfo()))) {
            /**
             * We need to call {@link notifyObservers(false)} before caling
             * {@link notifyObservers(true)}. This is because {@link notifyObservers(true)} has been
             * called in {@link setProfile()}, and without calling {@link notifyObservers(false)},
             * the ObservableSupplierImpl doesn't propagate the call. See https://cubug.com/1137535.
             */
            notifyObservers(false);
            notifyObservers(true);
        }
    }

    /**
     * Implements {@link IdentityManager.Observer}.
     *
     * IdentityDisc should be shown as long as the user is signed in. Whether the user is syncing
     * or not should not matter.
     */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        switch (eventDetails.getEventTypeFor(ConsentLevel.NOT_REQUIRED)) {
            case PrimaryAccountChangeEvent.Type.SET:
                resetIdentityDiscCache();
                notifyObservers(true);
                break;
            case PrimaryAccountChangeEvent.Type.CLEARED:
                notifyObservers(false);
                break;
            case PrimaryAccountChangeEvent.Type.NONE:
                break;
        }
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

        if (mNativeIsInitialized) {
            mProfileSupplier.removeObserver(mProfileSupplierObserver);
        }
    }

    /**
     * Records IdentityDisc usage with feature engagement tracker. This signal can be used to decide
     * whether to show in-product help.
     */
    private void recordIdentityDiscUsed() {
        assert mProfileSupplier != null && mProfileSupplier.get() != null;
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfileSupplier.get());
        tracker.notifyEvent(EventConstants.IDENTITY_DISC_USED);
        RecordUserAction.record("MobileToolbarIdentityDiscTap");
    }

    /**
     * Returns the account info of mIdentityManager if current profile is regular, and
     * null for off-the-record ones.
     * @return account info for the current profile. Returns null for OTR profile.
     */
    private CoreAccountInfo getSignedInAccountInfo() {
        @ConsentLevel
        int consentLevel =
                ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
                ? ConsentLevel.NOT_REQUIRED
                : ConsentLevel.SYNC;
        return mIdentityManager != null ? mIdentityManager.getPrimaryAccountInfo(consentLevel)
                                        : null;
    }

    /**
     * Triggered by mProfileSupplierObserver when profile is changed in mProfileSupplier.
     * mIdentityManager is updated with the profile, as set to null if profile is off-the-record.
     */
    private void setProfile(Profile profile) {
        if (mIdentityManager != null) {
            mIdentityManager.removeObserver(this);
        }

        if (profile.isOffTheRecord()) {
            mIdentityManager = null;
        } else {
            mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
            mIdentityManager.addObserver(this);
            notifyObservers(true);
        }
    }
}
