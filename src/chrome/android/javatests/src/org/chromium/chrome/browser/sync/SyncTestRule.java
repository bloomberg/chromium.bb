// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.Nullable;
import androidx.preference.TwoStatePreference;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.protocol.AutofillWalletSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.sync.protocol.WalletMaskedCreditCard;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * TestRule for common functionality between sync tests.
 */
public class SyncTestRule extends ChromeActivityTestRule<ChromeActivity> {
    private static final String TAG = "SyncTestBase";

    private static final String CLIENT_ID = "Client_ID";

    private static final Set<Integer> USER_SELECTABLE_TYPES =
            new HashSet<Integer>(Arrays.asList(new Integer[] {
                    ModelType.AUTOFILL, ModelType.BOOKMARKS, ModelType.PASSWORDS,
                    ModelType.PREFERENCES, ModelType.PROXY_TABS, ModelType.TYPED_URLS,
            }));

    /**
     * Simple activity that mimics a trusted vault key retrieval flow that succeeds immediately.
     */
    public static class DummyKeyRetrievalActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setResult(RESULT_OK);
            FakeTrustedVaultClientBackend.get().startPopulateKeys();
            finish();
        }
    };

    /**
     * A fake implementation of TrustedVaultClient.Backend. Allows to specify keys to be fetched.
     * Keys aren't populated through fetchKeys() unless startPopulateKeys() is called.
     * startPopulateKeys() is called by DummyKeyRetrievalActivity before its completion to mimic
     * real TrustedVaultClient.Backend implementation.
     */
    public static class FakeTrustedVaultClientBackend implements TrustedVaultClient.Backend {
        private static FakeTrustedVaultClientBackend sInstance;
        private boolean mPopulateKeys;
        private @Nullable List<byte[]> mKeys;

        public FakeTrustedVaultClientBackend() {
            mPopulateKeys = false;
        }

        public static FakeTrustedVaultClientBackend get() {
            if (sInstance == null) {
                sInstance = new FakeTrustedVaultClientBackend();
            }
            return sInstance;
        }

        @Override
        public Promise<List<byte[]>> fetchKeys(CoreAccountInfo accountInfo) {
            if (mKeys == null || !mPopulateKeys) {
                return Promise.rejected();
            }
            return Promise.fulfilled(mKeys);
        }

        @Override
        public Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo) {
            Context context = InstrumentationRegistry.getContext();
            Intent intent = new Intent(context, DummyKeyRetrievalActivity.class);
            return Promise.fulfilled(
                    PendingIntent.getActivity(context, 0 /* requestCode */, intent, 0 /* flags */));
        }

        @Override
        public Promise<Boolean> markKeysAsStale(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        public void setKeys(List<byte[]> keys) {
            mKeys = Collections.unmodifiableList(keys);
        }

        public void startPopulateKeys() {
            mPopulateKeys = true;
        }
    }

    private Context mContext;
    private FakeServerHelper mFakeServerHelper;
    private ProfileSyncService mProfileSyncService;
    private MockSyncContentResolverDelegate mSyncContentResolver;

    private void ruleSetUp() {
        // This must be called before super.setUp() in order for test authentication to work.
        SigninTestUtil.setUpAuthForTesting();
    }

    private void ruleTearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSyncService.requestStop();
            FakeServerHelper.deleteFakeServer();
        });
        SigninTestUtil.tearDownAuthForTesting();
    }

    public SyncTestRule() {
        super(ChromeActivity.class);
    }

    /**Getters for Test variables */
    public Context getTargetContext() {
        return mContext;
    }

    public FakeServerHelper getFakeServerHelper() {
        return mFakeServerHelper;
    }

    public ProfileSyncService getProfileSyncService() {
        return mProfileSyncService;
    }

    public MockSyncContentResolverDelegate getSyncContentResolver() {
        return mSyncContentResolver;
    }

    public void startMainActivityForSyncTest() throws Exception {
        // Start the activity by opening about:blank. This URL is ideal because it is not synced as
        // a typed URL. If another URL is used, it could interfere with test data.
        startMainActivityOnBlankPage();
    }

    public Account setUpTestAccount() {
        Account account = SigninTestUtil.addTestAccount();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        return account;
    }

    /**
     * Set up a test account, sign in and enable sync. FirstSetupComplete bit will be set after
     * this. For most purposes this function should be used as this emulates the basic sign in flow.
     * @return the test account that is signed in.
     */
    public Account setUpAccountAndSignInForTesting() {
        Account account = setUpTestAccount();
        signinAndEnableSync(account);
        return account;
    }

    /**
     * Set up a test account, sign in but don't mark sync setup complete.
     * @return the test account that is signed in.
     */
    public Account setUpTestAccountAndSignInWithSyncSetupAsIncomplete() {
        Account account = setUpTestAccount();
        signinAndEnableSyncInternal(account, false);
        return account;
    }

    public void startSync() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfileSyncService.requestStart(); });
    }

    public void startSyncAndWait() {
        startSync();
        SyncTestUtil.waitForSyncActive();
    }

    public void stopSync() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfileSyncService.requestStop(); });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    public void signinAndEnableSync(final Account account) {
        signinAndEnableSyncInternal(account, true);
    }

    public void signOut() throws InterruptedException {
        final Semaphore s = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.get().getSigninManager().signOut(
                    SignoutReason.SIGNOUT_TEST, s::release, false);
        });
        Assert.assertTrue(s.tryAcquire(SyncTestUtil.TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertNull(SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }

    public void clearServerData() {
        mFakeServerHelper.clearServerData();
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(
                Criteria.equals(false, () -> ProfileSyncService.get().isSyncRequested()),
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    /*
     * Enables the |modelType| Sync data type, which must be in USER_SELECTABLE_TYPES.
     */
    public void enableDataType(final int modelType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> chosenTypes = mProfileSyncService.getChosenDataTypes();
            chosenTypes.add(modelType);
            mProfileSyncService.setChosenDataTypes(false, chosenTypes);
        });
    }

    /*
     * Enables the |chosenDataTypes|, which must be in USER_SELECTABLE_TYPES.
     */
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> chosenDataTypes) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mProfileSyncService.setChosenDataTypes(syncEverything, chosenDataTypes); });
    }

    /*
     * Sets payments integration to |enabled|.
     */
    public void setPaymentsIntegrationEnabled(final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.setPaymentsIntegrationEnabled(enabled));
    }

    /*
     * Disables the |modelType| Sync data type, which must be in USER_SELECTABLE_TYPES.
     */
    public void disableDataType(final int modelType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> chosenTypes = mProfileSyncService.getChosenDataTypes();
            chosenTypes.remove(modelType);
            mProfileSyncService.setChosenDataTypes(false, chosenTypes);
        });
    }

    @Deprecated // TODO(tedchoc): Remove this method once Criteria.equals returns a Runnable.
    public void pollInstrumentationThread(Criteria criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    public void pollInstrumentationThread(Runnable criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    public void pollInstrumentationThread(Callable<Boolean> criteria, String reason) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, reason, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    @Override
    public Statement apply(final Statement statement, final Description desc) {
        final Statement base = super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                mSyncContentResolver = new MockSyncContentResolverDelegate();
                AndroidSyncSettingsTestUtils.setUpAndroidSyncSettingsForTesting(
                        mSyncContentResolver);

                TrustedVaultClient.setInstanceForTesting(
                        new TrustedVaultClient(FakeTrustedVaultClientBackend.get()));

                startMainActivityForSyncTest();
                mContext = InstrumentationRegistry.getTargetContext();

                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    // Ensure SyncController is registered with the new AndroidSyncSettings.
                    AndroidSyncSettings.get().registerObserver(SyncController.get());
                    mFakeServerHelper = FakeServerHelper.get();
                });
                FakeServerHelper.useFakeServer(mContext);
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    ProfileSyncService profileSyncService = createProfileSyncService();
                    if (profileSyncService != null) {
                        ProfileSyncService.overrideForTests(profileSyncService);
                    }
                    mProfileSyncService = ProfileSyncService.get();
                });

                UniqueIdentificationGeneratorFactory.registerGenerator(
                        UuidBasedUniqueIdentificationGenerator.GENERATOR_ID,
                        new UniqueIdentificationGenerator() {
                            @Override
                            public String getUniqueId(String salt) {
                                return CLIENT_ID;
                            }
                        },
                        true);
                statement.evaluate();
            }
        }, desc);
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ruleSetUp();
                base.evaluate();
                ruleTearDown();
            }
        };
    }

    /*
     * Adds a credit card to server for autofill.
     */
    public void addServerAutofillCreditCard() {
        final String serverId = "025eb937c022489eb8dc78cbaa969218";
        WalletMaskedCreditCard card =
                WalletMaskedCreditCard.newBuilder()
                        .setId(serverId)
                        .setStatus(WalletMaskedCreditCard.WalletCardStatus.VALID)
                        .setNameOnCard("Jon Doe")
                        .setType(WalletMaskedCreditCard.WalletCardType.UNKNOWN)
                        .setLastFour("1111")
                        .setExpMonth(11)
                        .setExpYear(2020)
                        .build();
        AutofillWalletSpecifics wallet_specifics =
                AutofillWalletSpecifics.newBuilder()
                        .setType(AutofillWalletSpecifics.WalletInfoType.MASKED_CREDIT_CARD)
                        .setMaskedCard(card)
                        .build();
        EntitySpecifics specifics =
                EntitySpecifics.newBuilder().setAutofillWallet(wallet_specifics).build();
        SyncEntity entity = SyncEntity.newBuilder()
                                    .setName(serverId)
                                    .setIdString(serverId)
                                    .setSpecifics(specifics)
                                    .build();
        getFakeServerHelper().setWalletData(entity);
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    /*
     * Checks if server has any credit card information to autofill.
     */
    public boolean hasServerAutofillCreditCards() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            List<CreditCard> cards = PersonalDataManager.getInstance().getCreditCardsForSettings();
            for (int i = 0; i < cards.size(); i++) {
                if (!cards.get(i).getIsLocal()) return true;
            }
            return false;
        });
    }

    // UI interaction convenience methods.
    public void togglePreference(final TwoStatePreference pref) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean newValue = !pref.isChecked();
            pref.getOnPreferenceChangeListener().onPreferenceChange(pref, newValue);
            pref.setChecked(newValue);
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Returns an instance of ProfileSyncService that can be overridden by subclasses.
     */
    protected ProfileSyncService createProfileSyncService() {
        return null;
    }

    private void signinAndEnableSyncInternal(final Account account, boolean setFirstSetupComplete) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.get().getSigninManager().signIn(
                    SigninAccessPoint.UNKNOWN, account, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            if (setFirstSetupComplete) {
                                mProfileSyncService.setFirstSetupComplete(
                                        SyncFirstSetupCompleteSource.BASIC_FLOW);
                            }
                        }

                        @Override
                        public void onSignInAborted() {
                            Assert.fail("Sign-in was aborted");
                        }
                    });
            // Outside of tests, URL-keyed anonymized data collection is enabled by sign-in UI.
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), true);
        });
        if (setFirstSetupComplete) {
            SyncTestUtil.waitForSyncActive();
            SyncTestUtil.triggerSyncAndWaitForCompletion();
        } else {
            SyncTestUtil.waitForSyncTransportActive();
        }
        Assert.assertEquals(account, SigninTestUtil.getCurrentAccount());
    }
}
