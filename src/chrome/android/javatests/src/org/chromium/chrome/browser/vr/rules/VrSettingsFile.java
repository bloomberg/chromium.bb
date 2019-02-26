// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * An annotation for applying VrCore settings during pre-test setup.
 *
 * This doesn't have any sort of performance benefit, but does keep the specifics of test setup
 * out of the test body.
 *
 * For example, the following would cause a test to start with the DON flow enabled:
 *     <code>
 *     @VrSettingsFile(VrSettingsFile.DDVIEW_DONENABLED)
 *     </code>
 */
@Target({ElementType.METHOD})
@Retention(RetentionPolicy.RUNTIME)
public @interface VrSettingsFile {
    /**
     * @return The settings file to apply.
     */
    public String value();
}