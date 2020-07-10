// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.announcement;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.support.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.util.IntentUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Sends announcement notification for information update of Chrome.
 * When to show notification is controlled by Finch.
 */
public class AnnouncementNotificationManager {
    private static final String ANNOUNCEMENT_NOTIFICATION_TAG = "announcement_notification";
    private static final int ANNOUNCEMENT_NOTIFICATION_ID = 100;
    private static final String EXTRA_INTENT_TYPE =
            "org.chromium.chrome.browser.announcement.EXTRA_INTENT_TYPE";
    private static final String EXTRA_URL = "org.chromium.chrome.browser.announcement.EXTRA_URL";

    @IntDef({IntentType.UNKNOWN, IntentType.CLICK, IntentType.CLOSE, IntentType.ACK,
            IntentType.OPEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IntentType {
        int UNKNOWN = 0;
        int CLICK = 1;
        int CLOSE = 2;
        int ACK = 3;
        int OPEN = 4;
    }

    /**
     * Receive announcement notification click event.
     */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final BrowserParts parts = new EmptyBrowserParts() {
                @Override
                public void finishNativeInitialization() {
                    @IntentType
                    int intentType = IntentUtils.safeGetIntExtra(
                            intent, EXTRA_INTENT_TYPE, IntentType.UNKNOWN);
                    String url = IntentUtils.safeGetStringExtra(intent, EXTRA_URL);
                    switch (intentType) {
                        case IntentType.UNKNOWN:
                            break;
                        case IntentType.CLICK:
                            recordHistogram(AnnouncementNotificationEvent.CLICK);
                            openUrl(context, url);
                            break;
                        case IntentType.CLOSE:
                            recordHistogram(AnnouncementNotificationEvent.CLOSE);
                            break;
                        case IntentType.ACK:
                            recordHistogram(AnnouncementNotificationEvent.ACK);
                            close();
                            break;
                        case IntentType.OPEN:
                            recordHistogram(AnnouncementNotificationEvent.OPEN);
                            openUrl(context, url);
                            close();
                            break;
                    }
                }
            };

            // Try to load native.
            ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        }
    }

    private static PendingIntentProvider createIntent(
            Context context, @IntentType int intentType, String url) {
        Intent intent = new Intent(context, AnnouncementNotificationManager.Receiver.class);
        intent.putExtra(EXTRA_INTENT_TYPE, intentType);
        intent.putExtra(EXTRA_URL, url);
        return PendingIntentProvider.getBroadcast(
                context, intentType /*requestCode*/, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private static void openUrl(Context context, String url) {
        // Open the announcement URL, fallback to default URL if remote URL is empty.
        CustomTabActivity.showInfoPage(context, url);
    }

    private static void close() {
        // Dismiss the notification.
        new NotificationManagerProxyImpl(ContextUtils.getApplicationContext())
                .cancel(ANNOUNCEMENT_NOTIFICATION_TAG, ANNOUNCEMENT_NOTIFICATION_ID);
    }

    private static void recordHistogram(@AnnouncementNotificationEvent int event) {
        RecordHistogram.recordEnumeratedHistogram("Notifications.Announcement.Events", event,
                AnnouncementNotificationEvent.MAX_VALUE + 1);
    }

    @CalledByNative
    private static void showNotification(String url) {
        Context context = ContextUtils.getApplicationContext();
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChannelDefinitions.ChannelId.ANNOUNCEMENT,
                                null /* remoteAppPackageName */,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.ANNOUNCEMENT,
                                        ANNOUNCEMENT_NOTIFICATION_TAG,
                                        ANNOUNCEMENT_NOTIFICATION_ID))
                        .setContentTitle(context.getString(R.string.tos_notification_title))
                        .setContentIntent(createIntent(context, IntentType.CLICK, url))
                        .setDeleteIntent(createIntent(context, IntentType.CLOSE, url))
                        .setContentText(context.getString(R.string.tos_notification_body_text))
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setShowWhen(false)
                        .setAutoCancel(true)
                        .setLocalOnly(true);
        builder.addAction(0, context.getString(R.string.tos_notification_ack_button_text),
                createIntent(context, IntentType.ACK, url),
                NotificationUmaTracker.ActionType.ANNOUNCEMENT_ACK);
        builder.addAction(0, context.getString(R.string.tos_notification_review_button_text),
                createIntent(context, IntentType.OPEN, url),
                NotificationUmaTracker.ActionType.ANNOUNCEMENT_OPEN);

        NotificationManagerProxy nm = new NotificationManagerProxyImpl(context);
        ChromeNotification notification = builder.buildChromeNotification();
        nm.notify(notification);

        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.ANNOUNCEMENT,
                notification.getNotification());

        recordHistogram(AnnouncementNotificationEvent.SHOWN);
    }

    @CalledByNative
    private static boolean isFirstRun() {
        return !FirstRunStatus.getFirstRunFlowComplete() || FirstRunStatus.isFirstRunTriggered();
    }
}
