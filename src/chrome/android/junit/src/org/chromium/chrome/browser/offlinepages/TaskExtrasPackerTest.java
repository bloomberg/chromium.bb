// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@link TaskExtrasPacker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TaskExtrasPackerTest {
    @Test
    @Feature({"OfflinePages"})
    public void testScheduledTimeExtra() {
        Bundle taskExtras = new Bundle();
        long beforeMillis = System.currentTimeMillis();
        TaskExtrasPacker.packTimeInBundle(taskExtras);
        long afterMillis = System.currentTimeMillis();
        long scheduledTimeMillis = TaskExtrasPacker.unpackTimeFromBundle(taskExtras);
        assertTrue(scheduledTimeMillis >= beforeMillis);
        assertTrue(scheduledTimeMillis <= afterMillis);
    }

    @Test
    @Feature({"OfflinePages"})
    public void testTriggerConditionsExtra() {
        Bundle taskExtras = new Bundle();
        TriggerConditions conditions1 = new TriggerConditions(true, 25, false);
        TaskExtrasPacker.packTriggerConditionsInBundle(taskExtras, conditions1);
        TriggerConditions unpackedConditions1 =
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(taskExtras);
        assertEquals(conditions1, unpackedConditions1);
        assertNotSame(conditions1, unpackedConditions1);

        // Now verify overwriting bundle with different values.
        TriggerConditions conditions2 = new TriggerConditions(false, 50, true);
        TaskExtrasPacker.packTriggerConditionsInBundle(taskExtras, conditions2);
        assertEquals(conditions2, TaskExtrasPacker.unpackTriggerConditionsFromBundle(taskExtras));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testTriggerConditionsExtraDefaults() {
        TriggerConditions unpackedConditionsFromEmptyBundle =
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(new Bundle());

        // Verify conservative defaults:
        assertTrue(unpackedConditionsFromEmptyBundle.requirePowerConnected());
        assertEquals(100, unpackedConditionsFromEmptyBundle.getMinimumBatteryPercentage());
        assertTrue(unpackedConditionsFromEmptyBundle.requireUnmeteredNetwork());
    }
}
