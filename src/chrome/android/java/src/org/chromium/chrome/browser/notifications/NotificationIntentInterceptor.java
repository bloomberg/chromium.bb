// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Random;

/**
 * Class to intercept {@link PendingIntent}s from notifications, including
 * {@link Notification#contentIntent}, {@link Notification.Action#actionIntent} and
 * {@link Notification#deleteIntent} with broadcast receivers.
 */
public class NotificationIntentInterceptor {
    private static final String TAG = "IntentInterceptor";
    private static final String EXTRA_PENDING_INTENT =
            "notifications.NotificationIntentInterceptor.EXTRA_PENDING_INTENT";
    private static final String EXTRA_INTENT_TYPE =
            "notifications.NotificationIntentInterceptor.EXTRA_INTENT_TYPE";

    /**
     * Enum that defines type of notification intent.
     */
    @IntDef({IntentType.CONTENT_INTENT, IntentType.ACTION_INTENT, IntentType.DELETE_INTENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IntentType {
        int CONTENT_INTENT = 0;
        int ACTION_INTENT = 1;
        int DELETE_INTENT = 2;
    }

    /**
     * Receives the event when the user taps on the notification body, notification action, or
     * dismiss notification.
     * {@link Notification#contentIntent}, {@link Notification#deleteIntent}
     * {@link Notification.Action#actionIntent} will be delivered to this broadcast receiver.
     */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // TODO(xingliu): Record notification CTR UMA.
            forwardPendingIntent(intent);
        }
    }

    private NotificationIntentInterceptor() {}

    /**
     * Wraps the notification {@link PendingIntent }into another PendingIntent, to intercept clicks
     * and dismiss events for metrics purpose.
     * @param intentType The type of the pending intent to intercept.
     * @param pendingIntent The {@link PendingIntent} of the notification that performs actual task.
     */
    public static PendingIntent createInterceptPendingIntent(
            @IntentType int intentType, PendingIntent pendingIntent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        Intent intent = new Intent(applicationContext, Receiver.class);
        intent.putExtra(EXTRA_PENDING_INTENT, pendingIntent);
        intent.putExtra(EXTRA_INTENT_TYPE, intentType);
        // TODO(xingliu): Add more extras to track notification type and action type. Use the
        // combination of notification type and id as the request code.
        int requestCode = new Random().nextInt(Integer.MAX_VALUE);

        // This flag ensures the broadcast is delivered with foreground priority to speed up the
        // broadcast delivery.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            intent.addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        }
        return PendingIntent.getBroadcast(
                applicationContext, requestCode, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    }

    // Launches the notification's pending intent, which will perform Chrome feature related tasks.
    private static void forwardPendingIntent(Intent intent) {
        if (intent == null) {
            Log.e(TAG, "Intent to forward is null.");
            return;
        }

        PendingIntent pendingIntent =
                (PendingIntent) (intent.getParcelableExtra(EXTRA_PENDING_INTENT));
        if (pendingIntent == null) {
            Log.d(TAG, "The notification's PendingIntent is null.");
            return;
        }

        try {
            pendingIntent.send();
        } catch (PendingIntent.CanceledException e) {
            Log.e(TAG, "The PendingIntent to fire is canceled.");
            e.printStackTrace();
        }
    }
}
