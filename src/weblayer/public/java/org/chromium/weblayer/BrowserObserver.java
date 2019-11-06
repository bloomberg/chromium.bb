// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

/**
 * Informed of interesting events that happen during the lifetime of a BrowserController.
 */
public abstract class BrowserObserver {
    /**
     * The Uri that should be displayed in the url-bar has updated.
     */
    public void displayURLChanged(Uri url) {}
}
