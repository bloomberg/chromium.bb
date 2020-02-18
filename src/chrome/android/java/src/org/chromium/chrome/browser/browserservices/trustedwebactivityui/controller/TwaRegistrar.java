// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.content.Context;

import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.permissiondelegation.NotificationPermissionUpdater;

import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;
import javax.inject.Named;

import dagger.Lazy;

/**
 * Records in all the appropriate places that a TWA has successfully been verified.
 */
public class TwaRegistrar {
    private final Context mAppContext;
    private final NotificationPermissionUpdater mNotificationPermissionUpdater;
    private final Lazy<ClientAppDataRecorder> mClientAppDataRecorder;

    // These origins have already been registered so we don't need to do so again.
    private final Set<Origin> mRegisteredOrigins = new HashSet<>();

    @Inject
    public TwaRegistrar(@Named(APP_CONTEXT) Context appContext,
            NotificationPermissionUpdater permissionUpdater,
            Lazy<ClientAppDataRecorder> clientAppDataRecorder) {
        mAppContext = appContext;

        mNotificationPermissionUpdater = permissionUpdater;
        mClientAppDataRecorder = clientAppDataRecorder;
    }

    /**
     * Registers to various stores that the client app is linked with the origin.
     *
     * We do this here, when the Trusted Web Activity UI is shown instead of in OriginVerifier when
     * verification completes because when an origin is being verified, we don't know whether it is
     * for the purposes of Trusted Web Activities or for Post Message (where this behaviour is not
     * required).
     *
     * Additionally we do it on every page navigation because an app can be verified for more than
     * one Origin, eg:
     * 1) App verifies with https://www.myfirsttwa.com/.
     * 2) App verifies with https://www.mysecondtwa.com/.
     * 3) App launches a TWA to https://www.myfirsttwa.com/.
     * 4) App navigates to https://www.mysecondtwa.com/.
     *
     * At step 2, we don't know why the app is verifying with that origin (it could be for TWAs or
     * for PostMessage). Only at step 4 do we know that Chrome should associate the browsing data
     * for that origin with that app.
     */
    public void registerClient(String packageName, Origin origin) {
        if (mRegisteredOrigins.contains(origin)) return;

        // Register that we should wipe data for this origin when the client app is uninstalled.
        mClientAppDataRecorder.get().register(packageName, origin);
        // Register that we trust the client app to handle notifications for this origin and update
        // Chrome's permission for the origin.
        mNotificationPermissionUpdater.onOriginVerified(origin, packageName);

        mRegisteredOrigins.add(origin);
    }
}
