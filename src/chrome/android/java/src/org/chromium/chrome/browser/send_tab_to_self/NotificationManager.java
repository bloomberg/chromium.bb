// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareNotificationInteraction;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareNotificationInteraction.InteractionType;
import org.chromium.components.browser_ui.notifications.ChromeNotification;
import org.chromium.components.browser_ui.notifications.ChromeNotificationBuilder;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Manages all SendTabToSelf related notifications for Android. This includes displaying, handling
 * taps, and timeouts.
 */
public class NotificationManager {
    private static final String NOTIFICATION_GUID_EXTRA = "send_tab_to_self.notification.guid";
    // Action constants for the registered BroadcastReceiver.
    private static final String NOTIFICATION_ACTION_TAP = "send_tab_to_self.tap";
    private static final String NOTIFICATION_ACTION_DISMISS = "send_tab_to_self.dismiss";
    private static final String NOTIFICATION_ACTION_TIMEOUT = "send_tab_to_self.timeout";

    /** Handles changes to notifications based on user action or timeout. */
    public static final class SendTabToSelfNotificationReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final BrowserParts parts = new EmptyBrowserParts() {
                @Override
                public void finishNativeInitialization() {
                    final String action = intent.getAction();
                    final String guid =
                            IntentUtils.safeGetStringExtra(intent, NOTIFICATION_GUID_EXTRA);
                    // If this feature ever supports incognito mode, we need to modify
                    // this method to obtain the current profile, rather than the last-used
                    // regular profile.
                    final Profile profile = Profile.getLastUsedRegularProfile();
                    switch (action) {
                        case NOTIFICATION_ACTION_TAP:
                            openUrl(intent.getData());
                            hideNotification(guid, InteractionType.OPENED);
                            SendTabToSelfAndroidBridge.deleteEntry(profile, guid);
                            break;
                        case NOTIFICATION_ACTION_DISMISS:
                            hideNotification(guid, InteractionType.DISMISSED);
                            SendTabToSelfAndroidBridge.dismissEntry(profile, guid);
                            break;
                        case NOTIFICATION_ACTION_TIMEOUT:
                            SendTabToSelfAndroidBridge.dismissEntry(profile, guid);
                            break;
                    }
                }
            };

            // Try to load native.
            ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        }
    }

    /**
     * Open the URL specified within Chrome.
     *
     * @param uri The URI to open.
     */
    private static void openUrl(Uri uri) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent()
                                .setAction(Intent.ACTION_VIEW)
                                .setData(uri)
                                .setClass(context, ChromeLauncherActivity.class)
                                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                                .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName())
                                .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }

    /**
     * Hides a notification and records an action to the Actions histogram.
     * <p>
     * If the notification is not actually visible, then no action will be taken, and the action
     * will not be recorded.
     *
     * @param guid The GUID of the notification to hide.
     */
    private static void hideNotification(@Nullable String guid, @InteractionType int type) {
        if (hideNotification(guid)) {
            SendTabToSelfShareNotificationInteraction.recordClickResult(type);
        }
    }

    /**
     * Hides a notification.
     *
     * @param guid The GUID of the notification to hide.
     * @return whether the notification was hidden. False if there is corresponding notification to
     * hide.
     */
    @CalledByNative
    private static boolean hideNotification(@Nullable String guid) {
        NotificationSharedPrefManager.ActiveNotification activeNotification =
                NotificationSharedPrefManager.findActiveNotification(guid);
        if (!NotificationSharedPrefManager.removeActiveNotification(guid)) {
            return false;
        }
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);
        manager.cancel(
                NotificationConstants.GROUP_SEND_TAB_TO_SELF, activeNotification.notificationId);
        return true;
    }

    /**
     * Displays a notification.
     *
     * @param url URL to open when the user taps on the notification.
     * @param title Title to display within the notification.
     * @param timeoutAtMillis Specifies how long until the notification should be automatically
     *            hidden.
     * @return whether the notification was successfully displayed
     */
    @CalledByNative
    private static boolean showNotification(String guid, @NonNull String url, String title,
            String deviceName, long timeoutAtMillis) {
        // A notification associated with this Share entry already exists. Don't display a new one.
        if (NotificationSharedPrefManager.findActiveNotification(guid) != null) {
            return false;
        }

        // Post notification.
        SendTabToSelfShareNotificationInteraction.recordClickResult(
                SendTabToSelfShareNotificationInteraction.InteractionType.SHOWN);
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);

        int nextId = NotificationSharedPrefManager.getNextNotificationId();
        Uri uri = Uri.parse(url);
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context, nextId,
                new Intent(context, SendTabToSelfNotificationReceiver.class)
                        .setData(uri)
                        .setAction(NOTIFICATION_ACTION_TAP)
                        .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                0);
        PendingIntentProvider deleteIntent = PendingIntentProvider.getBroadcast(context, nextId,
                new Intent(context, SendTabToSelfNotificationReceiver.class)
                        .setData(uri)
                        .setAction(NOTIFICATION_ACTION_DISMISS)
                        .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                0);
        // IDS_SEND_TAB_TO_SELF_NOTIFICATION_CONTEXT_TEXT
        Resources res = context.getResources();
        String contextText = res.getString(
                R.string.send_tab_to_self_notification_context_text, uri.getHost(), deviceName);
        // Build the notification itself.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChromeChannelDefinitions.ChannelId.SHARING,
                                null /* remoteAppPackageName */,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType
                                                .SEND_TAB_TO_SELF,
                                        NotificationConstants.GROUP_SEND_TAB_TO_SELF, nextId))
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(deleteIntent)
                        .setContentTitle(title)
                        .setContentText(contextText)
                        .setGroup(NotificationConstants.GROUP_SEND_TAB_TO_SELF)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setVibrate(new long[0])
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDefaults(Notification.DEFAULT_ALL);
        ChromeNotification notification = builder.buildChromeNotification();

        manager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SEND_TAB_TO_SELF,
                notification.getNotification());

        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(nextId, guid));

        // Set timeout.
        if (timeoutAtMillis != Long.MAX_VALUE) {
            AlarmManager alarmManager =
                    (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
            Intent timeoutIntent = new Intent(context, SendTabToSelfNotificationReceiver.class)
                                           .setData(Uri.parse(url))
                                           .setAction(NOTIFICATION_ACTION_TIMEOUT)
                                           .putExtra(NOTIFICATION_GUID_EXTRA, guid);
            alarmManager.set(AlarmManager.RTC, timeoutAtMillis,
                    PendingIntent.getBroadcast(
                            context, nextId, timeoutIntent, PendingIntent.FLAG_UPDATE_CURRENT));
        }
        return true;
    }
}
