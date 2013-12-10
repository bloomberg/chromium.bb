// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import java.util.Arrays;

/** Tests for the {@link InvalidationClientNameProvider} */
public class InvalidationClientNameProviderTest extends InstrumentationTestCase {
    private InvalidationClientNameProvider mProvider;

    @Override
    protected void setUp() {
        mProvider = new InvalidationClientNameProvider();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testFallbackClientId() {
        // Test that the InvalidationController consistently returns the same ID even when it has to
        // resort to its "fallback" ID generation code.
        byte[] id1 = mProvider.getInvalidatorClientName();
        byte[] id2 = mProvider.getInvalidatorClientName();

        // We expect the returned IDs to be consistent in every call.
        assertTrue("Expected returned IDs to be consistent", Arrays.equals(id1, id2));

        // Even if initialize the generator late, the ID will remain consistent.
        registerHardCodedGenerator(mProvider);

        // IDs should still be consistent, even if we change the generator.
        // (In the real program, the generator should be set before anyone invokes the
        // getInvalidatorClientName() and never change afterwards.  We test this anyway to make sure
        // nothing will blow up if someone accidentally violates that constraint.)
        byte[] id3 = mProvider.getInvalidatorClientName();
        assertTrue("Changing generators should not affect returned ID consistency",
            Arrays.equals(id2, id3));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPreRegisteredGenerator() {
        registerHardCodedGenerator(mProvider);

        byte[] id = mProvider.getInvalidatorClientName();
        byte[] id2 = mProvider.getInvalidatorClientName();

        // Expect that consistent IDs are maintained when using a custom generator, too.
        assertTrue("Custom generators should return consistent IDs", Arrays.equals(id, id2));
    }

    private static void registerHardCodedGenerator(InvalidationClientNameProvider provider) {
        provider.setPreferredClientNameGenerator(
            new InvalidationClientNameGenerator() {
                public byte[] generateInvalidatorClientName() {
                    return "Testable ID".getBytes();
                }
            });
    }
}
