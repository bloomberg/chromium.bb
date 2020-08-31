// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.view.View;

/**
 * Common interface for all the tab components in QrCodeDialog.
 */
public interface QrCodeDialogTab {
    public View getView();

    /**
     *  Called when the entire dialog is resumed.
     */
    public void onResume();

    /**
     *  Called when the entire dialog is paused.
     */
    public void onPause();

    /**
     *  Called when the dialog is destroyed. This happens when the user has navigated away from the
     *  dialog.
     */
    public void onDestroy();
}