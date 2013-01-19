// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.accounts.Account;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.common.collect.Lists;
import com.google.common.collect.Sets;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationController.IntentProtocol;

import java.util.List;
import java.util.Set;

/**
 * Tests for the {@link InvalidationController}.
 */
public class InvalidationControllerTest extends InstrumentationTestCase {
    private IntentSavingContext mContext;
    private InvalidationController mController;

    @Override
    protected void setUp() throws Exception {
        mContext = new IntentSavingContext(getInstrumentation().getTargetContext());
        mController = InvalidationController.newInstance(mContext);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStart() throws Exception {
        mController.start();
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertNull(intent.getExtras());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStop() throws Exception {
        mController.stop();
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(1, intent.getExtras().size());
        assertTrue(intent.hasExtra(IntentProtocol.EXTRA_STOP));
        assertTrue(intent.getBooleanExtra(IntentProtocol.EXTRA_STOP, false));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegisterForSpecificTypes() {
        Account account = new Account("test@example.com", "bogus");
        mController.setRegisteredTypes(account, false,
                Sets.newHashSet(ModelType.BOOKMARK, ModelType.SESSION));
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(IntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate account.
        Account intentAccount = intent.getParcelableExtra(IntentProtocol.EXTRA_ACCOUNT);
        assertEquals(account, intentAccount);

        // Validate registered types.
        Set<String> expectedTypes =
                Sets.newHashSet(ModelType.BOOKMARK.name(), ModelType.SESSION.name());
        Set<String> actualTypes = Sets.newHashSet();
        actualTypes.addAll(intent.getStringArrayListExtra(IntentProtocol.EXTRA_REGISTERED_TYPES));
        assertEquals(expectedTypes, actualTypes);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegisterForAllTypes() {
        Account account = new Account("test@example.com", "bogus");
        mController.setRegisteredTypes(account, true,
                Sets.newHashSet(ModelType.BOOKMARK, ModelType.SESSION));
        assertEquals(1, mContext.getNumStartedIntents());

        // Validate destination.
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(IntentProtocol.ACTION_REGISTER, intent.getAction());

        // Validate account.
        Account intentAccount = intent.getParcelableExtra(IntentProtocol.EXTRA_ACCOUNT);
        assertEquals(account, intentAccount);

        // Validate registered types.
        Set<String> expectedTypes = Sets.newHashSet(ModelType.ALL_TYPES_TYPE);
        Set<String> actualTypes = Sets.newHashSet();
        actualTypes.addAll(intent.getStringArrayListExtra(IntentProtocol.EXTRA_REGISTERED_TYPES));
        assertEquals(expectedTypes, actualTypes);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testGetContractAuthority() throws Exception {
        assertEquals(mContext.getPackageName(), mController.getContractAuthority());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testGetIntentDestination() {
        assertEquals("org.chromium.sync.notifier.TEST_VALUE",
                InvalidationController.getDestinationClassName(mContext));
    }

    /**
     * Asserts that {@code intent} is destined for the correct component.
     */
    private static void validateIntentComponent(Intent intent) {
        assertNotNull(intent.getComponent());
        assertEquals("org.chromium.sync.notifier.TEST_VALUE",
                intent.getComponent().getClassName());
    }

    /**
     * Mock context that saves all intents given to {@code startService}.
     */
    private static class IntentSavingContext extends AdvancedMockContext {
        private final List<Intent> startedIntents = Lists.newArrayList();

        IntentSavingContext(Context targetContext) {
            super(targetContext);
        }

        @Override
        public ComponentName startService(Intent intent) {
            startedIntents.add(intent);
            return new ComponentName(this, getClass());
        }

        int getNumStartedIntents() {
            return startedIntents.size();
        }

        Intent getStartedIntent(int idx) {
            return startedIntents.get(idx);
        }

        @Override
        public PackageManager getPackageManager() {
            return getBaseContext().getPackageManager();
        }
    }
}
