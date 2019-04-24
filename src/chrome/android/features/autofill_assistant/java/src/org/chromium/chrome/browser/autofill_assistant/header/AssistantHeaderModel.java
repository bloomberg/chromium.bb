// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantHeaderModel extends PropertyModel {
    @VisibleForTesting
    public static final WritableObjectPropertyKey<String> STATUS_MESSAGE =
            new WritableObjectPropertyKey<>();

    // TODO(crbug.com/806868): Change visibility to package-private once this is only set through
    // native calls.
    public static final WritableBooleanPropertyKey FEEDBACK_VISIBLE =
            new WritableBooleanPropertyKey();

    static final WritableIntPropertyKey PROGRESS = new WritableIntPropertyKey();

    @VisibleForTesting
    public static final WritableBooleanPropertyKey PROGRESS_VISIBLE =
            new WritableBooleanPropertyKey();

    static final WritableBooleanPropertyKey SPIN_POODLE = new WritableBooleanPropertyKey();

    static final WritableObjectPropertyKey<Runnable> FEEDBACK_BUTTON_CALLBACK =
            new WritableObjectPropertyKey<>();

    public AssistantHeaderModel() {
        super(STATUS_MESSAGE, FEEDBACK_VISIBLE, PROGRESS, PROGRESS_VISIBLE, SPIN_POODLE,
                FEEDBACK_BUTTON_CALLBACK);
    }

    @CalledByNative
    private void setStatusMessage(String statusMessage) {
        set(STATUS_MESSAGE, statusMessage);
    }

    @CalledByNative
    private void setProgress(int progress) {
        set(PROGRESS, progress);
    }

    @CalledByNative
    private void setProgressVisible(boolean visible) {
        set(PROGRESS_VISIBLE, visible);
    }

    @CalledByNative
    private void setSpinPoodle(boolean enabled) {
        set(SPIN_POODLE, enabled);
    }

    @CalledByNative
    private void setDelegate(AssistantHeaderDelegate delegate) {
        set(FEEDBACK_BUTTON_CALLBACK, delegate::onFeedbackButtonClicked);
    }
}
