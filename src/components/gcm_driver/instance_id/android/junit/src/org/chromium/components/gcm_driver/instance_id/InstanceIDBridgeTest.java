// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for InstanceIDBridge.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstanceIDBridgeTest {
    /**
     * Tests that lazy subscriptions are stored.
     */
    @Test
    public void testMarkSubscriptionAsLazy() {
        final String appId = "app_id";
        final String senderId = "sender_id";

        InstanceIDBridge instanceIDBridge =
                InstanceIDBridge.create(/*nativeInstanceIDAndroid=*/0L, appId);

        instanceIDBridge.storeLazinessInformation(senderId, true);
        assertEquals(true, InstanceIDBridge.isSubscriptionLazy(appId, senderId));
    }

    /**
     * Tests that unlazy subscriptions are stored.
     */
    @Test
    public void testMarkSubscriptionAsNotLazy() {
        final String appId = "app_id";
        final String senderId = "sender_id";

        InstanceIDBridge instanceIDBridge =
                InstanceIDBridge.create(/*nativeInstanceIDAndroid=*/0L, senderId);

        instanceIDBridge.storeLazinessInformation(senderId, false);
        assertEquals(false, InstanceIDBridge.isSubscriptionLazy(appId, senderId));
    }

    /**
     * Tests subscriptions are not lazy be default.
     */
    @Test
    public void testDefaultSubscriptionNotLazy() {
        final String appId = "app_id";
        final String senderId = "sender_id";
        assertEquals(false, InstanceIDBridge.isSubscriptionLazy(appId, senderId));
    }
}
