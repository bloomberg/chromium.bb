// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.app.Activity;
import android.app.FragmentManager;

import org.chromium.ui.base.WindowAndroid;

/**
 * Creates and represents the QrCode main UI.
 */
public class QrCodeCoordinator {
    private final QrCodeDialog mDialog;
    private final FragmentManager mFragmentManager;

    /**
     * Constructor.
     * @param activity The android activity.
     * @param url the url to be shared.
     * @param windowAndroid the WindowAndroid to access system permissions.
     */
    public QrCodeCoordinator(Activity activity, String url, WindowAndroid windowAndroid) {
        mDialog = QrCodeDialog.newInstance(url, windowAndroid);

        mFragmentManager = activity.getFragmentManager();
    }

    /**
     * Show the main dialog.
     */
    public void show() {
        mDialog.show(mFragmentManager, null);
    }

    /**
     * Dismiss the main dialog.
     */
    public void dismiss() {
        mDialog.dismiss();
    }
}
