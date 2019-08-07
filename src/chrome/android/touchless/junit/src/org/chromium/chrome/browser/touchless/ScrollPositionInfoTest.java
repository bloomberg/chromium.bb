// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.v7.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for the ScrollPositionInfo class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScrollPositionInfoTest {
    @Test
    public void roundTripSimple() {
        ScrollPositionInfo original = new ScrollPositionInfo(12, 34);
        ScrollPositionInfo roundTripped = ScrollPositionInfo.deserialize(original.serialize());
        Assert.assertEquals(original.index, roundTripped.index);
        Assert.assertEquals(original.offset, roundTripped.offset);
    }

    @Test
    public void roundTripNegative() {
        ScrollPositionInfo original = new ScrollPositionInfo(-12, -34);
        ScrollPositionInfo roundTripped = ScrollPositionInfo.deserialize(original.serialize());
        Assert.assertEquals(original.index, roundTripped.index);
        Assert.assertEquals(original.offset, roundTripped.offset);
    }

    @Test
    public void roundTripZero() {
        ScrollPositionInfo original = new ScrollPositionInfo(0, 0);
        ScrollPositionInfo roundTripped = ScrollPositionInfo.deserialize(original.serialize());
        Assert.assertEquals(original.index, roundTripped.index);
        Assert.assertEquals(original.offset, roundTripped.offset);
    }

    @Test
    public void deserializeFailure() {
        ScrollPositionInfo position = ScrollPositionInfo.deserialize("invalid");
        Assert.assertEquals(RecyclerView.NO_POSITION, position.index);
        Assert.assertEquals(0, position.offset);
    }

    @Test
    public void deserializeEmpty() {
        ScrollPositionInfo position = ScrollPositionInfo.deserialize("");
        Assert.assertEquals(RecyclerView.NO_POSITION, position.index);
        Assert.assertEquals(0, position.offset);
    }

    @Test
    public void deserializeNull() {
        ScrollPositionInfo position = ScrollPositionInfo.deserialize(null);
        Assert.assertEquals(RecyclerView.NO_POSITION, position.index);
        Assert.assertEquals(0, position.offset);
    }
}