// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatMinidumpUploadJobService;

/** See {@link ChromeMinidumpUploadJobServiceImpl}. */
public class ChromeMinidumpUploadJobService extends SplitCompatMinidumpUploadJobService {
    @IdentifierNameString
    private static final String IMPL_CLASS_NAME =
            "org.chromium.chrome.browser.crash.ChromeMinidumpUploadJobServiceImpl";

    public ChromeMinidumpUploadJobService() {
        super(IMPL_CLASS_NAME);
    }
}
