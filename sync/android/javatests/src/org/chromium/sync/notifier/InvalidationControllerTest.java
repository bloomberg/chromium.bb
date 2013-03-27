// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import android.accounts.Account;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import com.google.common.collect.Lists;
import com.google.common.collect.Sets;

import org.chromium.base.ActivityStatus;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.InvalidationController.IntentProtocol;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the {@link InvalidationController}.
 */
public class InvalidationControllerTest extends InstrumentationTestCase {
    private IntentSavingContext mContext;
    private InvalidationController mController;

    @Override
    protected void setUp() throws Exception {
        mContext = new IntentSavingContext(getInstrumentation().getTargetContext());
        mController = InvalidationController.get(mContext);
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
    public void testResumingMainActivity() throws Exception {
        // Resuming main activity should trigger a start if sync is enabled.
        setupSync(true);
        mController.onActivityStateChange(ActivityStatus.RESUMED);
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertNull(intent.getExtras());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testResumingMainActivityWithSyncDisabled() throws Exception {
        // Resuming main activity should NOT trigger a start if sync is disabled.
        setupSync(false);
        mController.onActivityStateChange(ActivityStatus.RESUMED);
        assertEquals(0, mContext.getNumStartedIntents());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPausingMainActivity() throws Exception {
        // Resuming main activity should trigger a stop if sync is enabled.
        setupSync(true);
        mController.onActivityStateChange(ActivityStatus.PAUSED);
        assertEquals(1, mContext.getNumStartedIntents());
        Intent intent = mContext.getStartedIntent(0);
        validateIntentComponent(intent);
        assertEquals(1, intent.getExtras().size());
        assertTrue(intent.hasExtra(IntentProtocol.EXTRA_STOP));
        assertTrue(intent.getBooleanExtra(IntentProtocol.EXTRA_STOP, false));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testPausingMainActivityWithSyncDisabled() throws Exception {
        // Resuming main activity should NOT trigger a stop if sync is disabled.
        setupSync(false);
        mController.onActivityStateChange(ActivityStatus.PAUSED);
        assertEquals(0, mContext.getNumStartedIntents());
    }

    private void setupSync(boolean syncEnabled) {
        MockSyncContentResolverDelegate contentResolver = new MockSyncContentResolverDelegate();
        // Android master sync can safely always be on.
        contentResolver.setMasterSyncAutomatically(true);
        // We don't want to use the system content resolver, so we override it.
        SyncStatusHelper.overrideSyncStatusHelperForTests(mContext, contentResolver);
        Account account = AccountManagerHelper.createAccountFromName("test@gmail.com");
        ChromeSigninController chromeSigninController = ChromeSigninController.get(mContext);
        chromeSigninController.setSignedInAccountName(account.name);
        SyncStatusHelper syncStatusHelper = SyncStatusHelper.get(mContext);
        if (syncEnabled) {
            syncStatusHelper.enableAndroidSync(account);
        } else {
            syncStatusHelper.disableAndroidSync(account);
        }
    }

    @SmallTest
    @Feature({"Sync"})
    public void testEnsureConstructorRegistersListener() throws Exception {
        final AtomicBoolean listenerCallbackCalled = new AtomicBoolean();

        // Create instance.
        new InvalidationController(mContext) {
            @Override
            public void onActivityStateChange(int newState) {
                listenerCallbackCalled.set(true);
            }
        };

        // Ensure initial state is correct.
        assertFalse(listenerCallbackCalled.get());

        // Ensure we get a callback, which means we have registered for them.
        ActivityStatus.onStateChange(new Activity(), ActivityStatus.RESUMED);
        assertTrue(listenerCallbackCalled.get());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRegisterForSpecificTypes() {
        InvalidationController controller = new InvalidationController(mContext);
        Account account = new Account("test@example.com", "bogus");
        controller.setRegisteredTypes(account, false,
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
    public void testRefreshShouldReadValuesFromDiskWithSpecificTypes() {
        // Store some preferences for ModelTypes and account. We are using the helper class
        // for this, so we don't have to deal with low-level details such as preference keys.
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        InvalidationPreferences.EditContext edit = invalidationPreferences.edit();
        Set<String> storedModelTypes = new HashSet<String>();
        storedModelTypes.add(ModelType.BOOKMARK.name());
        storedModelTypes.add(ModelType.TYPED_URL.name());
        Set<ModelType> refreshedTypes = new HashSet<ModelType>();
        refreshedTypes.add(ModelType.BOOKMARK);
        refreshedTypes.add(ModelType.TYPED_URL);
        invalidationPreferences.setSyncTypes(edit, storedModelTypes);
        Account storedAccount = AccountManagerHelper.createAccountFromName("test@gmail.com");
        invalidationPreferences.setAccount(edit, storedAccount);
        invalidationPreferences.commit(edit);

        // Ensure all calls to {@link InvalidationController#setRegisteredTypes} store values
        // we can inspect in the test.
        final AtomicReference<Account> resultAccount = new AtomicReference<Account>();
        final AtomicBoolean resultAllTypes = new AtomicBoolean();
        final AtomicReference<Set<ModelType>> resultTypes = new AtomicReference<Set<ModelType>>();
        InvalidationController controller = new InvalidationController(mContext) {
            @Override
            public void setRegisteredTypes(
                    Account account, boolean allTypes, Set<ModelType> types) {
                resultAccount.set(account);
                resultAllTypes.set(allTypes);
                resultTypes.set(types);
            }
        };

        // Execute the test.
        controller.refreshRegisteredTypes(refreshedTypes);

        // Validate the values.
        assertEquals(storedAccount, resultAccount.get());
        assertEquals(false, resultAllTypes.get());
        assertEquals(ModelType.syncTypesToModelTypes(storedModelTypes), resultTypes.get());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testRefreshShouldReadValuesFromDiskWithAllTypes() {
        // Store preferences for the ModelType.ALL_TYPES_TYPE and account. We are using the
        // helper class for this, so we don't have to deal with low-level details such as preference
        // keys.
        InvalidationPreferences invalidationPreferences = new InvalidationPreferences(mContext);
        InvalidationPreferences.EditContext edit = invalidationPreferences.edit();
        List<String> storedModelTypes = new ArrayList<String>();
        storedModelTypes.add(ModelType.ALL_TYPES_TYPE);
        invalidationPreferences.setSyncTypes(edit, storedModelTypes);
        Account storedAccount = AccountManagerHelper.createAccountFromName("test@gmail.com");
        invalidationPreferences.setAccount(edit, storedAccount);
        invalidationPreferences.commit(edit);

        // Ensure all calls to {@link InvalidationController#setRegisteredTypes} store values
        // we can inspect in the test.
        final AtomicReference<Account> resultAccount = new AtomicReference<Account>();
        final AtomicBoolean resultAllTypes = new AtomicBoolean();
        final AtomicReference<Set<ModelType>> resultTypes = new AtomicReference<Set<ModelType>>();
        InvalidationController controller = new InvalidationController(mContext) {
            @Override
            public void setRegisteredTypes(
                    Account account, boolean allTypes, Set<ModelType> types) {
                resultAccount.set(account);
                resultAllTypes.set(allTypes);
                resultTypes.set(types);
            }
        };

        // Execute the test.
        controller.refreshRegisteredTypes(new HashSet<ModelType>());

        // Validate the values.
        assertEquals(storedAccount, resultAccount.get());
        assertEquals(true, resultAllTypes.get());
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
