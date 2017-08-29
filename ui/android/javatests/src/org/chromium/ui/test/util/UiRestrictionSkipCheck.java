// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.test.util.RestrictionSkipCheck;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Checks if any restrictions exist and skip the test if it meets those restrictions.
 */
public class UiRestrictionSkipCheck extends RestrictionSkipCheck {
    public UiRestrictionSkipCheck(Context targetContext) {
        super(targetContext);
    }

    @Override
    protected boolean restrictionApplies(String restriction) {
        if (TextUtils.equals(restriction, UiRestriction.RESTRICTION_TYPE_PHONE)
                && DeviceFormFactor.isTablet()) {
            return true;
        }
        if (TextUtils.equals(restriction, UiRestriction.RESTRICTION_TYPE_TABLET)
                && !DeviceFormFactor.isTablet()) {
            return true;
        }
        return false;
    }
}
