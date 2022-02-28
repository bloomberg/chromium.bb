// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for the AppLocalUtils class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AppLocaleUtilsTest {
    // Reset the application override language after each test.
    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, null);
    }

    // Test getAppLanguagePref.
    @Test
    @SmallTest
    public void testGetAppLanguagePref() {
        String lang = AppLocaleUtils.getAppLanguagePref();
        Assert.assertEquals(null, lang);

        AppLocaleUtils.setAppLanguagePref("en-US");
        lang = AppLocaleUtils.getAppLanguagePref();
        Assert.assertEquals("en-US", lang);
    }

    // Test setAppLanguagePref.
    @Test
    @SmallTest
    public void testSetAppLanguagePref() {
        assertLanguagePrefEquals(null);

        AppLocaleUtils.setAppLanguagePref("en-US");
        assertLanguagePrefEquals("en-US");

        AppLocaleUtils.setAppLanguagePref("fr");
        assertLanguagePrefEquals("fr");
    }

    // Test isAppLanguagePref.
    @Test
    @SmallTest
    public void testIsAppLanguagePref() {
        Assert.assertFalse(AppLocaleUtils.isAppLanguagePref("en"));

        AppLocaleUtils.setAppLanguagePref("en-US");
        Assert.assertTrue(AppLocaleUtils.isAppLanguagePref("en-US"));

        Assert.assertFalse(AppLocaleUtils.isAppLanguagePref("en"));
    }

    @Test
    @SmallTest
    public void testIsFollowSystemLanguage() {
        Assert.assertTrue(AppLocaleUtils.isFollowSystemLanguage(null));
        Assert.assertTrue(AppLocaleUtils.isFollowSystemLanguage(
                AppLocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE));
        Assert.assertFalse(AppLocaleUtils.isFollowSystemLanguage("en"));
    }

    @Test
    @SmallTest
    public void testIsAvailableBaseUiLanguage() {
        // Base languages that there are no UI translations for.
        List<String> notAvailableBaseLanguages =
                Arrays.asList("cy", "ga", "ia", "yo", "aa-not-a-language");
        for (String language : notAvailableBaseLanguages) {
            Assert.assertFalse(String.format("Language %s", language),
                    AppLocaleUtils.isSupportedUiLanguage(language));
        }

        // Base languages that have UI translations.
        List<String> avaliableBaseLanguages =
                Arrays.asList("af-ZA", "en", "en-US", "en-non-a-language", "fr-CA", "fr-FR",
                        "zu-ZA", AppLocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE);
        for (String language : avaliableBaseLanguages) {
            Assert.assertTrue(String.format("Language %s", language),
                    AppLocaleUtils.isSupportedUiLanguage(language));
        }
    }

    @Test
    @SmallTest
    public void testIsAvailableExactUiLanguage() {
        // Languages for which there is no exact matching UI language.
        List<String> notAvailableExactLanguages = Arrays.asList(
                "en", "pt", "zh", "cy", "ga", "ia", "en-AU", "en-ZA", "fr-CM", "en-not-a-language");
        for (String language : notAvailableExactLanguages) {
            Assert.assertFalse(String.format("Language %s", language),
                    AppLocaleUtils.isAvailableExactUiLanguage(language));
        }

        // Languages that have an exact matching UI language.
        List<String> avaliableExactLanguages = Arrays.asList("en-US", "pt-BR", "fr", "fr-CA",
                "zh-CN", AppLocaleUtils.APP_LOCALE_USE_SYSTEM_LANGUAGE);
        for (String language : avaliableExactLanguages) {
            Assert.assertTrue(String.format("Language %s", language),
                    AppLocaleUtils.isAvailableExactUiLanguage(language));
        }
    }

    @Test
    @SmallTest
    public void testHasMultipleUiLanguageVariants() {
        Assert.assertTrue(AppLocaleUtils.hasMultipleUiLanguageVariants("en-US"));
        Assert.assertTrue(AppLocaleUtils.hasMultipleUiLanguageVariants("en-AU"));
        Assert.assertTrue(AppLocaleUtils.hasMultipleUiLanguageVariants("pt-BR"));
        Assert.assertTrue(AppLocaleUtils.hasMultipleUiLanguageVariants("es"));

        Assert.assertFalse(AppLocaleUtils.hasMultipleUiLanguageVariants("af"));
        Assert.assertFalse(AppLocaleUtils.hasMultipleUiLanguageVariants("af-ZA"));
        Assert.assertFalse(AppLocaleUtils.hasMultipleUiLanguageVariants("fil"));
        Assert.assertFalse(AppLocaleUtils.hasMultipleUiLanguageVariants("zu"));

        // Non existent UI language is false.
        Assert.assertFalse(AppLocaleUtils.hasMultipleUiLanguageVariants("dne"));
    }

    // Helper function to manually get and check AppLanguagePref.
    private void assertLanguagePrefEquals(String language) {
        Assert.assertEquals(language,
                SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.APPLICATION_OVERRIDE_LANGUAGE, null));
    }
}
