// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.support.v4.app.NotificationManagerCompat;

import java.util.List;

/**
 * Default implementation of the NotificationManagerProxy, which passes through all calls to the
 * normal Android Notification Manager.
 */
public class NotificationManagerProxyImpl implements NotificationManagerProxy {
    private final Context mContext;
    private final NotificationManager mNotificationManager;

    public NotificationManagerProxyImpl(Context context) {
        mContext = context;
        mNotificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
    }

    @Override
    public boolean areNotificationsEnabled() {
        return NotificationManagerCompat.from(mContext).areNotificationsEnabled();
    }

    @Override
    public void cancel(int id) {
        mNotificationManager.cancel(id);
    }

    @Override
    public void cancel(String tag, int id) {
        mNotificationManager.cancel(tag, id);
    }

    @Override
    public void cancelAll() {
        mNotificationManager.cancelAll();
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public void createNotificationChannel(NotificationChannel channel) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        // Suppress the notification dot/number that may appear on the browser app launcher. We
        // suppress this because showing it may imply that tapping the launch icon will lead
        // to some way of dismissing the dot, which is generally not the case. We don't want to
        // show a number either because users may have notifications from various websites, so an
        // aggregate figure is probably not useful.
        channel.setShowBadge(false);
        mNotificationManager.createNotificationChannel(channel);
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public void createNotificationChannelGroup(NotificationChannelGroup channelGroup) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.createNotificationChannelGroup(channelGroup);
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public List<NotificationChannel> getNotificationChannels() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return mNotificationManager.getNotificationChannels();
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public List<NotificationChannelGroup> getNotificationChannelGroups() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return mNotificationManager.getNotificationChannelGroups();
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public void deleteNotificationChannel(String id) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.deleteNotificationChannel(id);
    }

    @Override
    public void notify(int id, Notification notification) {
        mNotificationManager.notify(id, notification);
    }

    @Override
    public void notify(String tag, int id, Notification notification) {
        mNotificationManager.notify(tag, id, notification);
    }

    @Override
    public void notify(ChromeNotification notification) {
        assert notification.getMetadata() != null;
        mNotificationManager.notify(notification.getMetadata().tag, notification.getMetadata().id,
                notification.getNotification());
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public NotificationChannel getNotificationChannel(String channelId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        return mNotificationManager.getNotificationChannel(channelId);
    }

    @TargetApi(Build.VERSION_CODES.O)
    @Override
    public void deleteNotificationChannelGroup(String groupId) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        mNotificationManager.deleteNotificationChannelGroup(groupId);
    }
}
