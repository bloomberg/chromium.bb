// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;

/** Fake implementation of {@link ViewDepthProvider}. */
public final class FakeViewDepthProvider implements ViewDepthProvider {
    /*@Nullable*/ private String mChildViewDepth = null;

    public FakeViewDepthProvider() {}

    @Override
    /*@Nullable*/
    public String getChildViewDepth() {
        return mChildViewDepth;
    }

    public FakeViewDepthProvider setChildViewDepth(String childViewDepth) {
        this.mChildViewDepth = childViewDepth;
        return this;
    }
}
