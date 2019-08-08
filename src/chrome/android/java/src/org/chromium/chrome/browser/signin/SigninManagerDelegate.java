// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.content.Context;

/**
 * Interface providing SigninManager access to dependencies that are not part of the SignIn
 * component. This interface interacts with //chrome features such as Policy, Sync, data wiping,
 * Google Play services.
 */
public interface SigninManagerDelegate {
    /**
     * If there is no Google Play Services available, ask the user to fix by showing either a
     * notification or a modal dialog
     * @param activity The activity used to open the dialog, or null to use notifications
     * @param cancelable Whether the dialog can be canceled
     */
    public void handleGooglePlayServicesUnavailability(Activity activity, boolean cancelable);

    /**
     * @return Whether the device has Google Play Services.
     */
    public boolean isGooglePlayServicesPresent(Context context);
}
