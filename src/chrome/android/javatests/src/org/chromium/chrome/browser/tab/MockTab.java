// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

/**
 * Tab used for various testing purposes.
 */
public class MockTab extends Tab {
    /**
     * Constructor for id and incognito atrribute. Tests often need to initialize
     * these two fields only.
     */
    public MockTab(int id, boolean incognito) {
        super(id, null, incognito, null, null, null, null);
    }
}
