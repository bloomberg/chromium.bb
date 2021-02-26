// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.autofill.AutofillManager;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Subclass of WebContentsAccessibility for P
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.P)
public class PieWebContentsAccessibility extends OWebContentsAccessibility {
    PieWebContentsAccessibility(WebContents webContents) {
        super(webContents);
        AutofillManager autofillManager = mContext.getSystemService(AutofillManager.class);
        if (autofillManager != null && autofillManager.isEnabled()) {
            // Native accessibility is usually initialized when getAccessibilityNodeProvider is
            // called, but the Autofill compatibility bridge only calls that method after it has
            // received the first accessibility events. To solve the chicken-and-egg problem,
            // always initialize the native parts when the user has an Autofill service enabled.
            refreshState();
            getAccessibilityNodeProvider();
        }
    }

    @Override
    protected void setAccessibilityNodeInfoPaneTitle(AccessibilityNodeInfo node, String title) {
        node.setPaneTitle(title);
    }
}
