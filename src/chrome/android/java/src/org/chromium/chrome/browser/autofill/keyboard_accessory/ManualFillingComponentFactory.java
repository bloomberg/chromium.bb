// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

/**
 * Use {@link #createComponent()} to instantiate a {@link ManualFillingComponent}.
 */
public class ManualFillingComponentFactory {
    private ManualFillingComponentFactory() {}

    /**
     * Creates a {@link ManualFillingComponent} if the implementation is available. If it isn't,
     * null is returned instead.
     * @return A {@link ManualFillingComponent}.
     */
    public static ManualFillingComponent createComponent() {
        return new ManualFillingCoordinator();
    }
}