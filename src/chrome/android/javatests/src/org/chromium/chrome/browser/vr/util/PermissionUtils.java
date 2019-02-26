// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.content.DialogInterface;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.permissions.PermissionDialogController;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Utility class for interacting with permission prompts outside of the VR Browser. For interaction
 * in the VR Browser, see NativeUiUtils.
 */
public class PermissionUtils {
    /**
     * Blocks until a permission prompt appears.
     */
    public static void waitForPermissionPrompt() {
        CriteriaHelper.pollUiThread(() -> {
            return PermissionDialogController.getInstance().getCurrentDialogForTesting() != null;
        }, "Permission prompt did not appear in allotted time");
    }

    /**
     * Accepts the currently displayed permission prompt.
     */
    public static void acceptPermissionPrompt() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance()
                    .getCurrentDialogForTesting()
                    .getButton(DialogInterface.BUTTON_POSITIVE)
                    .performClick();
        });
    }

    /**
     * Denies the currently displayed permission prompt.
     */
    public static void denyPermissionPrompt() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController.getInstance()
                    .getCurrentDialogForTesting()
                    .getButton(DialogInterface.BUTTON_NEGATIVE)
                    .performClick();
        });
    }
}