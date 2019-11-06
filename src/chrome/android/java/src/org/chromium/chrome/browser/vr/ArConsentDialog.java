// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.support.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides a consent dialog shown before entering an immersive AR session.
 *
 * <p>For the duration of the session, the site will get ARCore world understanding
 * data such as hit tests or plane detection, and also camera movement tracking via
 * 6DoF poses. The browser process separately receives the camera image and composites
 * that image with the application-drawn AR image.</p>
 *
 * <p>This is different from typical camera permission usage since the web page
 * will NOT get access to camera images. The user consent is only valid for
 * the duration of one session and is not persistent.</p>
 *
 * <p>The browser needs Android-level camera access for using ARCore, this
 * is requested if needed after the user has granted consent for the AR session.</p>
 */
public class ArConsentDialog implements ModalDialogProperties.Controller {
    private static final String TAG = "ArConsentDialog";
    private static final boolean DEBUG_LOGS = false;

    private ModalDialogManager mModalDialogManager;
    private ArCoreJavaUtils mArCoreJavaUtils;
    private ChromeActivity mActivity;

    public static void showDialog(ChromeActivity activity, ArCoreJavaUtils caller) {
        ArConsentDialog dialog = new ArConsentDialog();
        dialog.show(activity, caller);
    }

    public void show(@NonNull ChromeActivity activity, @NonNull ArCoreJavaUtils caller) {
        mArCoreJavaUtils = caller;
        mActivity = activity;

        Resources resources = activity.getResources();
        PropertyModel model = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                                      .with(ModalDialogProperties.CONTROLLER, this)
                                      .with(ModalDialogProperties.TITLE, resources,
                                              R.string.ar_immersive_mode_consent_title)
                                      .with(ModalDialogProperties.MESSAGE, resources,
                                              R.string.ar_immersive_mode_consent_message)
                                      .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, resources,
                                              R.string.ar_immersive_mode_consent_button)
                                      .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                              R.string.cancel)
                                      .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                                      .build();
        mModalDialogManager = activity.getModalDialogManager();
        mModalDialogManager.showDialog(model, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        } else {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            onConsentGranted();
        } else {
            onConsentDenied();
        }
    }

    private void onConsentGranted() {
        if (DEBUG_LOGS) Log.i(TAG, "onConsentGranted");

        WindowAndroid window = mActivity.getWindowAndroid();
        if (!window.hasPermission(android.Manifest.permission.CAMERA)) {
            // The user has agreed to proceed with the AR session, but the browser
            // application doesn't have the prerequisite Android-level camera permission
            // needed for using ARCore internally. Show the system permission prompt.
            requestCameraPermission();
            return;
        }
        startSession();
    }

    private void requestCameraPermission() {
        PermissionCallback callback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (DEBUG_LOGS) Log.i(TAG, "onRequestPermissionsResult");
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    if (DEBUG_LOGS) Log.i(TAG, "onRequestPermissionsResult=granted");
                    startSession();
                } else {
                    // Didn't get permission :-(
                    if (DEBUG_LOGS) Log.i(TAG, "onRequestPermissionsResult=denied");
                    endSession();
                }
            }
        };

        WindowAndroid window = mActivity.getWindowAndroid();
        window.requestPermissions(new String[] {android.Manifest.permission.CAMERA}, callback);
    }

    private void onConsentDenied() {
        if (DEBUG_LOGS) Log.i(TAG, "onConsentDenied");
        endSession();
    }

    private void startSession() {
        if (DEBUG_LOGS) Log.i(TAG, "startSession");
        // We have user consent to start the session.
        mArCoreJavaUtils.onStartSession(mActivity);
    }

    private void endSession() {
        if (DEBUG_LOGS) Log.i(TAG, "endSession");
        mArCoreJavaUtils.onDrawingSurfaceDestroyed();
    }
}
