// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.content.Context;

import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;

/**
 * This implementation of {@link SigninManagerDelegate} provides {@link SigninManager} access to
 * //chrome/browser level dependencies.
 */
public class ChromeSigninManagerDelegate implements SigninManagerDelegate {
    @Override
    public void handleGooglePlayServicesUnavailability(Activity activity, boolean cancelable) {
        UserRecoverableErrorHandler errorHandler = activity != null
                ? new UserRecoverableErrorHandler.ModalDialog(activity, cancelable)
                : new UserRecoverableErrorHandler.SystemNotification();
        ExternalAuthUtils.getInstance().canUseGooglePlayServices(errorHandler);
    }

    @Override
    public boolean isGooglePlayServicesPresent(Context context) {
        return !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(context);
    }
}
