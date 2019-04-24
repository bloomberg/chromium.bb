// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.sync.FakeProfileSyncService;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.sync.ModelType;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.HashSet;

/**
 * Test suite for the SendTabToSelfShare Activity
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.SEND_TAB_TO_SELF})
public class SendTabToSelfShareActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private FakeProfileSyncService mProfileSyncService;
    private EmbeddedTestServer mTestServer;
    private Tab mTab;

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";
    private static final String ABOUT_BLANK = "about:blank";
    private static final String NATIVE_PAGE = "chrome://newtab";

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.overrideForTests(new FakeProfileSyncService());
                mProfileSyncService = (FakeProfileSyncService) ProfileSyncService.get();
            }
        });
    }

    @After
    public void tearDown() throws Exception {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    // Test enabling of share activity
    @Test
    @LargeTest
    @Feature({"SendTabToSelf"})
    public void testEnabled() throws Exception {
        setChosenTypes(ModelType.SEND_TAB_TO_SELF);
        mProfileSyncService.setNumberOfSyncedDevices(2);
        loadUrlInTab(TEST_PAGE);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                boolean result = SendTabToSelfShareActivity.featureIsAvailable(mTab);
                Assert.assertTrue(
                        "SendTabToSelfShareActivity disabled because of unsupported type", result);
            }
        });
    }

    // Test disabling of share activity because sync model type is not supported.
    @Test
    @LargeTest
    @Feature({"SendTabToSelf"})
    public void testDisabledUnsupportedSyncModelType() throws Exception {
        setChosenTypes(null);
        mProfileSyncService.setNumberOfSyncedDevices(2);
        loadUrlInTab(TEST_PAGE);
        assertFeatureIsDisabled("SendTabToSelfShareActivity disabled because of unsupported type");
    }

    // Test disabling of share activity because <2 devices are being synced.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledInsufficientSyncedDevices() throws Exception {
        mProfileSyncService.setNumberOfSyncedDevices(0);
        setChosenTypes(ModelType.SEND_TAB_TO_SELF);
        loadUrlInTab(TEST_PAGE);
        assertFeatureIsDisabled("SendTabToSelfShareActivity disabled because of insufficient "
                + "synced devices");

        mProfileSyncService.setNumberOfSyncedDevices(1);
        loadUrlInTab(TEST_PAGE);
        assertFeatureIsDisabled("SendTabToSelfShareActivity disabled because of insufficient "
                + "synced devices");
    }

    // Test disabling of share feature because of URL
    @Test
    @LargeTest
    @Feature({"SendTabToSelf"})
    public void testDisableWrongUrl() throws Exception {
        setChosenTypes(ModelType.SEND_TAB_TO_SELF);
        mProfileSyncService.setNumberOfSyncedDevices(2);
        assertFeatureIsDisabled("SendTabToSelfShareActivity disabled because of unsupported url");
    }

    // Test enabling of share activity
    @Test
    @LargeTest
    @Feature({"SendTabToSelf"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.SEND_TAB_TO_SELF)
    public void testFeatureDisabled() throws Exception {
        setChosenTypes(ModelType.SEND_TAB_TO_SELF);
        mProfileSyncService.setNumberOfSyncedDevices(2);
        loadUrlInTab(TEST_PAGE);
        assertFeatureIsDisabled("SendTabToSelfShareActivity disabled because of unsupported type");
    }

    private void assertFeatureIsDisabled(final String errorMessage) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                boolean result = SendTabToSelfShareActivity.featureIsAvailable(mTab);
                Assert.assertFalse(errorMessage, result);
            }
        });
    }

    private void setChosenTypes(Integer type) {
        HashSet<Integer> chosenTypes = new HashSet<Integer>();
        if (type != null) {
            chosenTypes.add(type);
        }
        mProfileSyncService.setChosenDataTypes(false, chosenTypes);
    }

    private void loadUrlInTab(String url) throws Exception {
        String testServerUrl = mTestServer.getURL(url);
        new TabLoadObserver(mTab).fullyLoadUrl(testServerUrl);
    }
}