// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.crash;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.PersistableBundle;

import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobImpl;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;

/**
 * Class that interacts with the Android JobScheduler to upload minidumps at appropriate times.
 */
@TargetApi(Build.VERSION_CODES.M)
public class ChromeMinidumpUploadJobService extends MinidumpUploadJobService {
    @Override
    protected MinidumpUploadJob createMinidumpUploadJob(PersistableBundle permissions) {
        return new MinidumpUploadJobImpl(new ChromeMinidumpUploaderDelegate(this, permissions));
    }
}
