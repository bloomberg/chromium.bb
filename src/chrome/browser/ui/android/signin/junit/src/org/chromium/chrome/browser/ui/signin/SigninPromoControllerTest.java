// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.text.format.DateUtils;

import com.google.common.base.Optional;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * Tests for {@link SigninPromoController}..
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS})
public class SigninPromoControllerTest {
    private static final int TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS = 100;
    private static final long TIME_SINCE_FIRST_SHOWN_LIMIT_MS =
            TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS * DateUtils.HOUR_IN_MILLIS;
    private static final int RESET_AFTER_HOURS = 10;
    private static final long RESET_AFTER_MS = RESET_AFTER_HOURS * DateUtils.HOUR_IN_MILLIS;
    private static final int MAX_SIGN_IN_PROMO_IMPRESSIONS = 10;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock
    private IdentityManager mIdentityManagerMock;

    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mock(Profile.class));
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(Profile.getLastUsedRegularProfile()))
                .thenReturn(mIdentityManagerMock);
        mSharedPreferencesManager.writeInt(SigninPromoController.getPromoShowCountPreferenceName(
                                                   SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                0);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, 0L);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, 0L);
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNoAccountOnDevice() {
        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCannotOfferSyncPromos() {
        final Account account =
                AccountUtils.createAccountFromName("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount(account);
        mAccountManagerTestRule.addAccount("test.account.secondary@gmail.com");
        doReturn(Optional.of(false))
                .when(mFakeAccountManagerFacade)
                .canOfferExtendedSyncPromos(account);

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCapabilityIsNotFetched() {
        final Account account =
                AccountUtils.createAccountFromName("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount(account);
        mAccountManagerTestRule.addAccount("test.account.secondary@gmail.com");

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenSecondaryAccountCannotOfferSyncPromos() {
        final Account secondAccount =
                AccountUtils.createAccountFromName("test.account.secondary@gmail.com");
        doAnswer(invocation -> {
            final Account account0 = invocation.getArgument(0);
            return Optional.of(!account0.equals(secondAccount));
        })
                .when(mFakeAccountManagerFacade)
                .canOfferExtendedSyncPromos(any());
        mAccountManagerTestRule.addAccount("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount(secondAccount);

        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowNTPSyncPromoWhenCountLimitIsNotExceeded() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
                "MaxSigninPromoImpressions", Integer.toString(MAX_SIGN_IN_PROMO_IMPRESSIONS));
        testValues.addFeatureFlagOverride(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD, true);
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS, false);
        FeatureList.setTestValues(testValues);
        mSharedPreferencesManager.writeInt(SigninPromoController.getPromoShowCountPreferenceName(
                                                   SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                MAX_SIGN_IN_PROMO_IMPRESSIONS - 1);

        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideNTPSyncPromoWhenCountLimitIsExceeded() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
                "MaxSigninPromoImpressions", Integer.toString(MAX_SIGN_IN_PROMO_IMPRESSIONS));
        testValues.addFeatureFlagOverride(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD, true);
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS, false);
        FeatureList.setTestValues(testValues);
        mSharedPreferencesManager.writeInt(SigninPromoController.getPromoShowCountPreferenceName(
                                                   SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                MAX_SIGN_IN_PROMO_IMPRESSIONS);

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowNTPSyncPromoWithoutTimeSinceFirstShownLimit() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                -1);

        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowNTPSyncPromoWhenTimeSinceFirstShownLimitIsNotExceeded() {
        final long firstShownTime = System.currentTimeMillis();
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS);

        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideNTPSyncPromoWhenTimeSinceFirstShownLimitIsExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS);

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldNotResetLimitsWithoutResetTimeLimit() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis() - RESET_AFTER_MS - 1;
        setNTPSyncPromoLimitsAndValues(
                firstShownTime, lastShownTime, /*signinPromoResetAfterHours=*/-1);
        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));

        SigninPromoController.resetNTPSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
        Assert.assertEquals(firstShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(lastShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(MAX_SIGN_IN_PROMO_IMPRESSIONS,
                SharedPreferencesManager.getInstance().readInt(
                        SigninPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)));
    }

    @Test
    public void shouldNotResetLimitsWhenResetTimeLimitIsNotExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis();
        setNTPSyncPromoLimitsAndValues(firstShownTime, lastShownTime, RESET_AFTER_HOURS);
        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));

        SigninPromoController.resetNTPSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
        Assert.assertEquals(firstShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(lastShownTime,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(MAX_SIGN_IN_PROMO_IMPRESSIONS,
                SharedPreferencesManager.getInstance().readInt(
                        SigninPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)));
    }

    @Test
    public void shouldResetLimitsWhenResetTimeLimitIsExceeded() {
        final long firstShownTime =
                System.currentTimeMillis() - TIME_SINCE_FIRST_SHOWN_LIMIT_MS - 1;
        final long lastShownTime = System.currentTimeMillis() - RESET_AFTER_MS - 1;
        setNTPSyncPromoLimitsAndValues(firstShownTime, lastShownTime, RESET_AFTER_HOURS);
        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));

        SigninPromoController.resetNTPSyncPromoLimitsIfHiddenForTooLong();

        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
        Assert.assertEquals(0L,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME));
        Assert.assertEquals(0L,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME));
        Assert.assertEquals(0,
                SharedPreferencesManager.getInstance().readInt(
                        SigninPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS)));
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNotDismissed() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, false);

        Assert.assertTrue(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDismissed() {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_DISMISSED, true);

        Assert.assertFalse(
                SigninPromoController.canShowSyncPromo(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    private void setNTPSyncPromoLimitsAndValues(
            long firstShownTime, long lastShownTime, int signinPromoResetAfterHours) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
                "MaxSigninPromoImpressions", Integer.toString(MAX_SIGN_IN_PROMO_IMPRESSIONS));
        testValues.addFeatureFlagOverride(ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD, true);
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS, false);
        FeatureList.setTestValues(testValues);

        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_SINCE_FIRST_TIME_SHOWN_LIMIT_HOURS.setForTesting(
                TIME_SINCE_FIRST_SHOWN_LIMIT_HOURS);
        StartSurfaceConfiguration.SIGNIN_PROMO_NTP_RESET_AFTER_HOURS.setForTesting(
                signinPromoResetAfterHours);

        SharedPreferencesManager.getInstance().writeInt(
                SigninPromoController.getPromoShowCountPreferenceName(
                        SigninAccessPoint.NTP_CONTENT_SUGGESTIONS),
                MAX_SIGN_IN_PROMO_IMPRESSIONS);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_FIRST_SHOWN_TIME, firstShownTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_LAST_SHOWN_TIME, lastShownTime);
    }
}
