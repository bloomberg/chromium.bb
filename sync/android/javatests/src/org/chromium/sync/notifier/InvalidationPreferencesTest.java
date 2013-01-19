// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.accounts.Account;
import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.common.collect.Sets;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationPreferences;

import java.util.HashSet;
import java.util.Set;

/**
 * Tests for the {@link InvalidationPreferences}.
 *
 * @author dsmyers@google.com (Daniel Myers)
 */
public class InvalidationPreferencesTest extends InstrumentationTestCase {
    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = new AdvancedMockContext(getInstrumentation().getContext());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testTranslateBasicSyncTypes() throws Exception {
        /*
         * Test plan: convert three strings to model types, one of which is invalid. Verify that
         * the two valid strings are properly converted and that the invalid string is dropped.
         */
        HashSet<ModelType> expectedTypes = Sets.newHashSet(ModelType.BOOKMARK, ModelType.SESSION);
        Set<ModelType> actualTypes = ModelType.syncTypesToModelTypes(
                Sets.newHashSet("BOOKMARK", "SESSION", "0!!!INVALID"));
        assertEquals(expectedTypes, actualTypes);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testTranslateAllSyncTypes() {
        /*
         * Test plan: convert the special all-types type to model types. Verify that it is
         * properly expanded.
         */
        HashSet<ModelType> expectedTypes = Sets.newHashSet(ModelType.values());
        Set<ModelType> actualTypes = ModelType.syncTypesToModelTypes(
                Sets.newHashSet(ModelType.ALL_TYPES_TYPE));
        assertEquals(expectedTypes, actualTypes);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testReadMissingData() {
        /*
         * Test plan: read saved state from empty preferences. Verify that null is returned.
         */
        InvalidationPreferences invPreferences = new InvalidationPreferences(mContext);
        assertNull(invPreferences.getSavedSyncedAccount());
        assertNull(invPreferences.getSavedSyncedTypes());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testReadWriteAndReadData() {
        /*
         * Test plan: write and read back saved state. Verify that the returned state is what
         * was written.
         */
        InvalidationPreferences invPreferences = new InvalidationPreferences(mContext);
        InvalidationPreferences.EditContext editContext = invPreferences.edit();

        // We should never write both a real type and the all-types type in practice, but we test
        // with them here to ensure that preferences are not interpreting the written data.
        Set<String> syncTypes = Sets.newHashSet("BOOKMARK", ModelType.ALL_TYPES_TYPE);
        Account account = new Account("test@example.com", "bogus");
        invPreferences.setSyncTypes(editContext, syncTypes);
        invPreferences.setAccount(editContext, account);

        // Nothing should yet have been written.
        assertNull(invPreferences.getSavedSyncedAccount());
        assertNull(invPreferences.getSavedSyncedTypes());

        // Write the new data and verify that they are correctly read back.
        invPreferences.commit(editContext);
        assertEquals(account, invPreferences.getSavedSyncedAccount());
        assertEquals(syncTypes, invPreferences.getSavedSyncedTypes());
    }
}
