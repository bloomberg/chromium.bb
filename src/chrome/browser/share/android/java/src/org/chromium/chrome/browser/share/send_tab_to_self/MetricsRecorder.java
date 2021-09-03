// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Class that captures all the metrics needed for Send Tab To Self on Android.
 */
@JNINamespace("send_tab_to_self")
class MetricsRecorder {
    public static void recordDeviceClickedInShareSheet() {
        MetricsRecorderJni.get().recordDeviceClickedInShareSheet();
    }

    @NativeMethods
    interface Natives {
        void recordDeviceClickedInShareSheet();
    }
}
