// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.support.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.invalidation.InvalidationController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.Passphrase;
import org.chromium.components.sync.StopSource;

/**
 * SyncController handles the coordination of sync state between the invalidation controller,
 * the Android sync settings, and the native sync code.
 *
 * It also handles initialization of some pieces of sync state on startup.
 *
 * Sync state can be changed from four places:
 *
 * - The Chrome UI, which will call SyncController directly.
 * - Native sync, which can disable it via a dashboard stop and clear.
 * - Android's Chrome sync setting.
 * - Android's master sync setting.
 *
 * SyncController implements listeners for the last three cases. When master sync is disabled, we
 * are careful to not change the Android Chrome sync setting so we know whether to turn sync back
 * on when it is re-enabled.
 */
public class SyncController implements ProfileSyncService.SyncStateChangedListener,
                                       AndroidSyncSettings.AndroidSyncSettingsObserver {
    private static final String TAG = "SyncController";

    /**
     * An identifier for the generator in UniqueIdentificationGeneratorFactory to be used to
     * generate the sync sessions ID. The generator is registered in the Application's onCreate
     * method.
     */
    public static final String GENERATOR_ID = "SYNC";

    @VisibleForTesting
    public static final String SESSION_TAG_PREFIX = "session_sync";

    @SuppressLint("StaticFieldLeak")
    private static SyncController sInstance;
    private static boolean sInitialized;

    private final ChromeSigninController mChromeSigninController;
    private final ProfileSyncService mProfileSyncService;
    private final SyncNotificationController mSyncNotificationController;

    private SyncController() {
        mChromeSigninController = ChromeSigninController.get();
        AndroidSyncSettings.get().registerObserver(this);
        mProfileSyncService = ProfileSyncService.get();
        mProfileSyncService.addSyncStateChangedListener(this);
        mProfileSyncService.setMasterSyncEnabledProvider(
                new ProfileSyncService.MasterSyncEnabledProvider() {
                    @Override
                    public boolean isMasterSyncEnabled() {
                        return AndroidSyncSettings.get().isMasterSyncEnabled();
                    }
                });

        setSessionsId();

        // Create the SyncNotificationController.
        mSyncNotificationController = new SyncNotificationController();
        mProfileSyncService.addSyncStateChangedListener(mSyncNotificationController);

        updateSyncStateFromAndroid();

        // When the application gets paused, tell sync to flush the directory to disk.
        ApplicationStatus.registerStateListenerForAllActivities(new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.PAUSED) {
                    mProfileSyncService.flushDirectory();
                }
            }
        });

        SigninManager.get().addSignInStateObserver(new SigninManager.SignInStateObserver() {
            @Override
            public void onSignedIn() {
                mProfileSyncService.requestStart();
            }

            @Override
            public void onSignedOut() {}
        });
    }

    /**
     * Retrieve the singleton instance of this class.
     *
     * @return the singleton instance.
     */
    @Nullable
    public static SyncController get() {
        ThreadUtils.assertOnUiThread();
        if (!sInitialized) {
            if (ProfileSyncService.get() != null) {
                sInstance = new SyncController();
            }
            sInitialized = true;
        }
        return sInstance;
    }

    /**
     * Retrieve the singleton instance of this class.
     * @deprecated Use get with no arguments instead.
     * @return the singleton instance.
     */
    @Nullable
    public static SyncController get(Context context) {
        return get();
    }

    /**
     * Updates sync to reflect the state of the Android sync settings.
     */
    private void updateSyncStateFromAndroid() {
        // Note: |isChromeSyncEnabled| maps to SyncRequested, and
        // |isMasterSyncEnabled| maps to *both* SyncRequested and
        // SyncAllowedByPlatform.
        // TODO(crbug.com/921025): Don't mix these two concepts.

        mProfileSyncService.setSyncAllowedByPlatform(
                AndroidSyncSettings.get().isMasterSyncEnabled());

        boolean isSyncEnabled = AndroidSyncSettings.get().isSyncEnabled();
        if (isSyncEnabled == mProfileSyncService.isSyncRequested()) return;
        if (isSyncEnabled) {
            mProfileSyncService.requestStart();
        } else {
            if (Profile.getLastUsedProfile().isChild()) {
                // For child accounts, Sync needs to stay enabled, so we reenable it in settings.
                // TODO(bauerb): Remove the dependency on child account code and instead go through
                // prefs (here and in the Sync customization UI).
                AndroidSyncSettings.get().enableChromeSync();
            } else {
                if (AndroidSyncSettings.get().isMasterSyncEnabled()) {
                    RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                            StopSource.ANDROID_CHROME_SYNC, StopSource.STOP_SOURCE_LIMIT);
                } else {
                    RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                            StopSource.ANDROID_MASTER_SYNC, StopSource.STOP_SOURCE_LIMIT);
                }
                mProfileSyncService.requestStop();
            }
        }
    }

    /**
     * From {@link ProfileSyncService.SyncStateChangedListener}.
     *
     * Changes the invalidation controller and Android sync setting state to match
     * the new native sync state.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();
        InvalidationController invalidationController = InvalidationController.get();
        if (mProfileSyncService.isSyncRequested()) {
            if (!invalidationController.isStarted()) {
                invalidationController.ensureStartedAndUpdateRegisteredTypes();
            }
            if (!AndroidSyncSettings.get().isSyncEnabled()) {
                AndroidSyncSettings.get().enableChromeSync();
            }
        } else {
            if (invalidationController.isStarted()) {
                invalidationController.stop();
            }
            if (AndroidSyncSettings.get().isSyncEnabled()) {
                // Both Android's master and Chrome sync setting are enabled, so we want to disable
                // the Chrome sync setting to match isSyncRequested. We have to be careful not to
                // disable it when isSyncRequested becomes false due to master sync being disabled
                // so that sync will turn back on if master sync is re-enabled.
                // TODO(crbug.com/921025): Master sync shouldn't influence isSyncRequested.
                AndroidSyncSettings.get().disableChromeSync();
            }
        }
    }

    /**
     * From {@link AndroidSyncSettings.AndroidSyncSettingsObserver}.
     */
    @Override
    public void androidSyncSettingsChanged() {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                updateSyncStateFromAndroid();
            }
        });
    }

    /**
     * @return Whether sync is enabled to sync urls or open tabs with a non custom passphrase.
     */
    public boolean isSyncingUrlsWithKeystorePassphrase() {
        return mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.getPreferredDataTypes().contains(ModelType.TYPED_URLS)
                && mProfileSyncService.getPassphraseType() == Passphrase.Type.KEYSTORE;
    }

    /**
     * Returns the SyncNotificationController.
     */
    public SyncNotificationController getSyncNotificationController() {
        return mSyncNotificationController;
    }

    /**
     * Set the sessions ID using the generator that was registered for GENERATOR_ID.
     */
    private void setSessionsId() {
        UniqueIdentificationGenerator generator =
                UniqueIdentificationGeneratorFactory.getInstance(GENERATOR_ID);
        String uniqueTag = generator.getUniqueId(null);
        if (uniqueTag.isEmpty()) {
            Log.e(TAG, "Unable to get unique tag for sync. "
                    + "This may lead to unexpected tab sync behavior.");
            return;
        }
        mProfileSyncService.setSessionsId(SESSION_TAG_PREFIX + uniqueTag);
    }
}
