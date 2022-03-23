// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationPermissionState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionConstants;
import org.chromium.ui.permissions.PermissionPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Central class containing the logic for when to trigger notification permission request optionally
 * with a rationale.
 */
public class NotificationPermissionController implements UnownedUserData {
    /** Field trial param controlling rationale behavior. */
    public static final String FIELD_TRIAL_ALWAYS_SHOW_RATIONALE_BEFORE_REQUESTING_PERMISSION =
            "always_show_rationale_before_requesting_permission";

    /** Refers to what type of permission UI should be shown. */
    @IntDef({PermissionRequestMode.DO_NOT_REQUEST, PermissionRequestMode.REQUEST_ANDROID_PERMISSION,
            PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PermissionRequestMode {
        /** Do not start any permission request. */
        int DO_NOT_REQUEST = 0;

        /** Show android permission dialog. */
        int REQUEST_ANDROID_PERMISSION = 1;

        /**
         * Show the in-app permission dialog. Depending on user action it might lead to the android
         * permission dialog.
         */
        int REQUEST_PERMISSION_WITH_RATIONALE = 2;
    }

    /** A delegate to show an in-app UI demonstrating rationale behind the permission request. */
    interface RationaleDelegate {
        /**
         * Called to show the in-app UI.
         * @param callback The callback to be invoked as part of the user action on the dialog UI.
         */
        void showRationaleUi(Callback<Boolean> callback);
    }

    private static final long PERMISSION_REQUEST_RETRIGGER_INTERVAL = TimeUnit.DAYS.toMillis(7);

    private static final UnownedUserDataKey<NotificationPermissionController> KEY =
            new UnownedUserDataKey<>(NotificationPermissionController.class);

    private final AndroidPermissionDelegate mAndroidPermissionDelegate;
    private final RationaleDelegate mRationaleDelegate;

    /**
     * Constructor. Should only be called by {@link ChromeTabbedActivity}. Features looking to
     * request this permission in context should instead use {@link
     * ContextualNotificationPermissionRequester}.
     * @param androidPermissionDelegate The delegate to request Android permissions.
     * @param rationaleDelegate The delegate to show the rationale UI.
     */
    public NotificationPermissionController(AndroidPermissionDelegate androidPermissionDelegate,
            RationaleDelegate rationaleDelegate) {
        mAndroidPermissionDelegate = androidPermissionDelegate;
        mRationaleDelegate = rationaleDelegate;
    }

    /**
     * Get the activity's {@link NotificationPermissionController} from the provided {@link
     * WindowAndroid}.
     * @param window The window to get the manager from.
     * @return The {@link NotificationPermissionController} associated with the activity.
     */
    public static @Nullable NotificationPermissionController from(WindowAndroid window) {
        if (window == null) return null;
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * Make this instance of NotificationPermissionController available through the activity's
     * {@link WindowAndroid} for ease of access.
     * @param window A {@link WindowAndroid} to attach to.
     * @param controller The {@link NotificationPermissionController} to attach.
     */
    public static void attach(WindowAndroid window, NotificationPermissionController controller) {
        KEY.attachToHost(window.getUnownedUserDataHost(), controller);
    }

    /**
     * Detach the provided NotificationPermissionController from any {@link WindowAndroid} it is
     * attached with.
     * @param controller The {@link NotificationPermissionController} to detach.
     */
    public static void detach(NotificationPermissionController controller) {
        KEY.detachFromAllHosts(controller);
    }

    /** Called on startup to request permission. See next method for more details. */
    public void requestPermissionIfNeeded() {
        requestPermissionIfNeeded(false);
    }

    /**
     * Called to request notification permission if not granted. Called on startup and contextually
     * by some features using notifications. Internally handles the logic for when to make
     * permission request directly and when to show a rationale beforehand.
     * @param contextual Whether this request is made in context. True for requesting from features
     *        using notifications, false for invoking on startup.
     */
    public void requestPermissionIfNeeded(boolean contextual) {
        // Notifications only require permission starting at Android T.
        if (!BuildInfo.isAtLeastT()) {
            return;
        }

        // Record the state of the notification permission before trying to ask.
        recordNotificationPermissionState();

        @PermissionRequestMode
        int requestMode = shouldRequestPermission();
        if (requestMode == PermissionRequestMode.DO_NOT_REQUEST) return;

        NotificationUmaTracker.getInstance().onNotificationPermissionRequested();

        if (requestMode == PermissionRequestMode.REQUEST_ANDROID_PERMISSION) {
            requestAndroidPermission();
        } else if (requestMode == PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE) {
            SharedPreferencesManager.getInstance().writeLong(
                    ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                    System.currentTimeMillis());
            mRationaleDelegate.showRationaleUi(accept -> {
                if (accept) {
                    requestAndroidPermission();
                }
            });
        }
    }

    @PermissionRequestMode
    int shouldRequestPermission() {
        if (!BuildInfo.isAtLeastT()) return PermissionRequestMode.DO_NOT_REQUEST;
        if (mAndroidPermissionDelegate.hasPermission(PermissionConstants.NOTIFICATION_PERMISSION)) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }
        if (!mAndroidPermissionDelegate.canRequestPermission(
                    PermissionConstants.NOTIFICATION_PERMISSION)) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }

        // Check if it is too soon to request permission again.
        if (wasPermissionRequestShown() && !hasEnoughTimeExpiredForRetriggerSinceLastDenial()) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }

        // Check if we have already exhausted the max number of times we can show the rationale. If
        // shouldAlwaysShowRationaleFirst is false, we can show the rationale max once.
        boolean wasRationaleShown =
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY, 0)
                != 0;
        boolean exceedsRationaleShowLimit = !shouldAlwaysShowRationaleFirst() && wasRationaleShown;
        if (exceedsRationaleShowLimit) return PermissionRequestMode.DO_NOT_REQUEST;

        // Decide whether to show the rationale or just the system prompt.
        boolean meetsAndroidRationaleAPI =
                mAndroidPermissionDelegate.shouldShowRequestPermissionRationale(
                        PermissionConstants.NOTIFICATION_PERMISSION);
        boolean shouldShowRationale = shouldAlwaysShowRationaleFirst() || meetsAndroidRationaleAPI;
        return shouldShowRationale ? PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE
                                   : PermissionRequestMode.REQUEST_ANDROID_PERMISSION;
    }

    private void recordNotificationPermissionState() {
        if (mAndroidPermissionDelegate.hasPermission(PermissionConstants.NOTIFICATION_PERMISSION)) {
            NotificationUmaTracker.getInstance().recordNotificationPermissionState(
                    NotificationPermissionState.ALLOWED);
            return;
        }

        if (mAndroidPermissionDelegate.isPermissionRevokedByPolicy(
                    PermissionConstants.NOTIFICATION_PERMISSION)) {
            NotificationUmaTracker.getInstance().recordNotificationPermissionState(
                    NotificationPermissionState.DENIED_BY_DEVICE_POLICY);
            return;
        }

        // Get number of times we've requested for notification permission at startup.
        // This count is updated on NotificationUmaTracker.onNotificationPermissionRequested.
        int promptCount = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.NOTIFICATION_PERMISSION_REQUEST_COUNT, 0);

        switch (promptCount) {
            case 0:
                NotificationUmaTracker.getInstance().recordNotificationPermissionState(
                        NotificationPermissionState.DENIED_NEVER_ASKED);
                break;
            case 1:
                NotificationUmaTracker.getInstance().recordNotificationPermissionState(
                        NotificationPermissionState.DENIED_ASKED_ONCE);
                break;
            case 2:
                NotificationUmaTracker.getInstance().recordNotificationPermissionState(
                        NotificationPermissionState.DENIED_ASKED_TWICE);
                break;
            default:
                NotificationUmaTracker.getInstance().recordNotificationPermissionState(
                        NotificationPermissionState.DENIED_ASKED_MORE_THAN_TWICE);
                break;
        }
    }

    private void requestAndroidPermission() {
        String[] permissionsToRequest = {PermissionConstants.NOTIFICATION_PERMISSION};
        mAndroidPermissionDelegate.requestPermissions(permissionsToRequest,
                (permissions, grantResults)
                        -> NotificationUmaTracker.getInstance()
                                   .onNotificationPermissionRequestResult(
                                           permissions, grantResults));
    }

    /** Some heuristic based re-triggering logic. */
    private static boolean hasEnoughTimeExpiredForRetriggerSinceLastDenial() {
        long lastAndroidPermissionRequestTimestamp =
                PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp();
        long lastRationaleTimestamp = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY, 0);
        long lastRequestTimestamp =
                Math.max(lastRationaleTimestamp, lastAndroidPermissionRequestTimestamp);

        // If the pref wasn't there to begin with, we don't need to retrigger the UI.
        if (lastRequestTimestamp == 0) return false;

        long elapsedTime = System.currentTimeMillis() - lastRequestTimestamp;
        return elapsedTime > PERMISSION_REQUEST_RETRIGGER_INTERVAL;
    }

    private static boolean shouldAlwaysShowRationaleFirst() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                FIELD_TRIAL_ALWAYS_SHOW_RATIONALE_BEFORE_REQUESTING_PERMISSION, false);
    }

    private boolean wasPermissionRequestShown() {
        boolean wasAndroidPermissionShown =
                PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp() != 0;
        boolean wasRationaleShown =
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY, 0)
                != 0;
        return wasAndroidPermissionShown || wasRationaleShown;
    }
}
