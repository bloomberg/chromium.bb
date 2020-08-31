// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Callback used to listen for cookie changes.
 *
 * @since 83
 */
public abstract class CookieChangedCallback {
    public abstract void onCookieChanged(@NonNull String cookie, @CookieChangeCause int cause);
}
