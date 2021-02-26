// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(crbug.com/1108163): This file should be generated with all feature policy enum values.
@IntDef({FeaturePolicyFeature.PAYMENT, FeaturePolicyFeature.WEB_SHARE})
@Retention(RetentionPolicy.SOURCE)
public @interface FeaturePolicyFeature {
    int PAYMENT = org.chromium.blink.mojom.FeaturePolicyFeature.PAYMENT;
    int WEB_SHARE = org.chromium.blink.mojom.FeaturePolicyFeature.WEB_SHARE;
}
