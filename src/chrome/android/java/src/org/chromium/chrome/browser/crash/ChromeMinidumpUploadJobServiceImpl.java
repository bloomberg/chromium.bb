// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.os.Build;
import android.os.PersistableBundle;

import androidx.annotation.RequiresApi;

import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobImpl;

/**
 * Class that interacts with the Android JobScheduler to upload minidumps at appropriate times.
 */
@RequiresApi(Build.VERSION_CODES.M)
public class ChromeMinidumpUploadJobServiceImpl extends ChromeMinidumpUploadJobService.Impl {
    @Override
    protected MinidumpUploadJob createMinidumpUploadJob(PersistableBundle permissions) {
        return new MinidumpUploadJobImpl(
                new ChromeMinidumpUploaderDelegate(getService(), permissions));
    }
}
