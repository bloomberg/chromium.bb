// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

/**
 * Enables custom implementation for the notification site settings category, similar to
 * {@link LocationCategory}.
 */
public class NotificationCategory extends SiteSettingsCategory {
    NotificationCategory(BrowserContextHandle browserContextHandle) {
        // Android does not treat notifications as a 'permission', i.e. notification status cannot
        // be checked via Context#checkPermission(). Hence we pass an empty string here and override
        // #enabledForChrome() to use the notification-status checking API instead.
        super(browserContextHandle, Type.NOTIFICATIONS, "" /* androidPermission*/);
    }

    @Override
    protected boolean enabledForChrome(Context context) {
        if (!SiteSettingsFeatureList.isEnabled(
                    SiteSettingsFeatureList.APP_NOTIFICATION_STATUS_MESSAGING)) {
            return super.enabledForChrome(context);
        }
        NotificationManagerCompat manager = NotificationManagerCompat.from(context);
        return manager.areNotificationsEnabled();
    }
}
