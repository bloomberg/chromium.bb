// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

/**
 * Abstraction over Notification.Builder and NotificationCompat.Builder interfaces.
 */
public interface ChromeNotificationBuilder {
    ChromeNotificationBuilder setAutoCancel(boolean autoCancel);

    @Deprecated
    ChromeNotificationBuilder setContentIntent(PendingIntent contentIntent);

    ChromeNotificationBuilder setContentIntent(PendingIntentProvider contentIntent);

    ChromeNotificationBuilder setContentTitle(CharSequence title);

    ChromeNotificationBuilder setContentText(CharSequence text);

    ChromeNotificationBuilder setSmallIcon(int icon);

    ChromeNotificationBuilder setSmallIcon(Icon icon);

    ChromeNotificationBuilder setColor(int argb);

    ChromeNotificationBuilder setTicker(CharSequence text);

    ChromeNotificationBuilder setLocalOnly(boolean localOnly);

    ChromeNotificationBuilder setGroup(String group);

    ChromeNotificationBuilder setGroupSummary(boolean isGroupSummary);

    ChromeNotificationBuilder addExtras(Bundle extras);

    ChromeNotificationBuilder setOngoing(boolean ongoing);

    ChromeNotificationBuilder setVisibility(int visibility);

    ChromeNotificationBuilder setShowWhen(boolean showWhen);

    @Deprecated
    ChromeNotificationBuilder addAction(int icon, CharSequence title, PendingIntent intent);

    /**
     * @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}.
     */
    ChromeNotificationBuilder addAction(
            int icon, CharSequence title, PendingIntentProvider intent, int actionType);

    @Deprecated
    ChromeNotificationBuilder addAction(Notification.Action action);

    /**
     * @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}.
     */
    ChromeNotificationBuilder addAction(Notification.Action action, int flags, int actionType);

    @Deprecated
    ChromeNotificationBuilder setDeleteIntent(PendingIntent intent);

    ChromeNotificationBuilder setDeleteIntent(PendingIntentProvider intent);

    /**
     * Sets the priority of single notification on Android versions prior to Oreo.
     * (From Oreo onwards, priority is instead determined by channel importance.)
     */
    ChromeNotificationBuilder setPriorityBeforeO(int pri);

    ChromeNotificationBuilder setProgress(int max, int percentage, boolean indeterminate);

    ChromeNotificationBuilder setSubText(CharSequence text);

    ChromeNotificationBuilder setContentInfo(String info);

    ChromeNotificationBuilder setWhen(long time);

    ChromeNotificationBuilder setLargeIcon(Bitmap icon);

    ChromeNotificationBuilder setVibrate(long[] vibratePattern);

    ChromeNotificationBuilder setSound(Uri sound);

    ChromeNotificationBuilder setDefaults(int defaults);

    ChromeNotificationBuilder setOnlyAlertOnce(boolean onlyAlertOnce);

    ChromeNotificationBuilder setPublicVersion(Notification publicNotification);

    ChromeNotificationBuilder setContent(RemoteViews views);

    ChromeNotificationBuilder setStyle(Notification.BigPictureStyle style);

    ChromeNotificationBuilder setStyle(Notification.BigTextStyle bigTextStyle);

    ChromeNotificationBuilder setMediaStyle(MediaSessionCompat session, int[] actions,
            PendingIntent intent, boolean showCancelButton);

    ChromeNotificationBuilder setCategory(String category);

    ChromeNotification buildWithBigContentView(RemoteViews bigView);

    ChromeNotification buildWithBigTextStyle(String bigText);

    @Deprecated
    Notification build();

    ChromeNotification buildChromeNotification();
}
