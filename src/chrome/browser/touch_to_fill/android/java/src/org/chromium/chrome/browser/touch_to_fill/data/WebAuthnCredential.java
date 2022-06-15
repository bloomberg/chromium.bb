// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.data;

import org.chromium.base.annotations.CalledByNative;

/**
 * This class holds the data used to represent a selectable Web Authentication credential in the
 * Touch To Fill sheet.
 */
public class WebAuthnCredential {
    private final String mUsername;
    private final String mDisplayName;
    private final String mId;

    /**
     * @param username Username shown to the user.
     * @param id Unique identifier for the credential.
     */
    public WebAuthnCredential(String username, String displayName, String id) {
        mUsername = username;
        mDisplayName = displayName;
        mId = id;
    }

    @CalledByNative
    public String getUsername() {
        return mUsername;
    }

    @CalledByNative
    public String getDisplayName() {
        return mDisplayName;
    }

    @CalledByNative
    public String getId() {
        return mId;
    }
}
