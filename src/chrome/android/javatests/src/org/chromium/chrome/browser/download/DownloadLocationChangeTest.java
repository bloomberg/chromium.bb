// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.support.test.espresso.Espresso.onData;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.equalTo;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.download.settings.DownloadDirectoryAdapter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;

/**
 * Test to verify download location change feature behaviors.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DownloadLocationChangeTest implements CustomMainActivityStart {
    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private EmbeddedTestServer mTestServer;
    private static final String TEST_DATA_DIRECTORY = "/chrome/test/data/android/download/";
    private static final String TEST_FILE = "test.gzip";
    private static final long STORAGE_SIZE = 1024000;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        // Show the location dialog for the first time.
        promptDownloadLocationDialog(DownloadPromptStatus.SHOW_INITIAL);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    // CustomMainActivityStart implementation.
    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Ensures the default download location dialog is shown to the user with SD card inserted.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Features.EnableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testDefaultDialogPositiveButtonClickThrough() {
        startDownload(/*hasSDCard=*/true);

        // Ensure the dialog is being shown.
        CriteriaHelper.pollUiThread(Criteria.equals(
                true, () -> mDownloadTestRule.getActivity().getModalDialogManager().isShowing()));

        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();

        // Click the button to start download.
        Espresso.onView(withId(R.id.positive_button)).perform(click());

        // Ensure download is done.
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));

        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    /**
     * Matches the {@link DirectoryOption} used in the {@link DownloadDirectoryAdapter}.
     */
    private static class DirectoryOptionMatcher extends TypeSafeMatcher<DirectoryOption> {
        private Matcher<String> mNameMatcher;

        public DirectoryOptionMatcher(Matcher<String> nameMatcher) {
            mNameMatcher = nameMatcher;
        }

        @Override
        protected boolean matchesSafely(DirectoryOption directoryOption) {
            return mNameMatcher.matches(directoryOption.name);
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("has DirectoryOption with name: ");
            description.appendDescriptionOf(mNameMatcher);
        }
    }

    /**
     * Ensures the default download location dialog has two download location options in the drop
     * down spinner.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Features.EnableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testDefaultDialogShowSpinner() {
        startDownload(/*hasSDCard=*/true);

        // Ensure the dialog is being shown.
        CriteriaHelper.pollUiThread(Criteria.equals(
                true, () -> mDownloadTestRule.getActivity().getModalDialogManager().isShowing()));

        // Open the spinner inside the dialog to show download location options.
        Espresso.onView(withId(R.id.file_location)).perform(click());

        // Wait for data to feed into the DownloadDirectoryAdapter.
        String defaultOptionName =
                InstrumentationRegistry.getTargetContext().getString(R.string.menu_downloads);
        String sdCardOptionName = InstrumentationRegistry.getTargetContext().getString(
                R.string.downloads_location_sd_card);
        onData(new DirectoryOptionMatcher(equalTo(defaultOptionName))).atPosition(0);
        onData(new DirectoryOptionMatcher(equalTo(sdCardOptionName))).atPosition(1);
    }

    /**
     * Ensures no default download location dialog is shown to the user without SD card inserted.
     */
    @Test
    @MediumTest
    @Feature({"Downloads"})
    @Features.EnableFeatures(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)
    public void testNoDialogWithoutSDCard() {
        int currentCallCount = mDownloadTestRule.getChromeDownloadCallCount();
        startDownload(/*hasSDCard=*/false);

        // Ensure download is done, no download location dialog should show to interact with user.
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(currentCallCount));
        mDownloadTestRule.deleteFilesInDownloadDirectory(new String[] {TEST_FILE});
    }

    /**
     * Starts a download, the download location dialog will show afterward.
     * @param hasSDCard Whether the SD card download option is valid.
     */
    private void startDownload(boolean hasSDCard) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(DownloadPromptStatus.SHOW_INITIAL,
                    DownloadLocationDialogBridge.getPromptForDownloadAndroid());

            simulateDownloadDirectories(hasSDCard);

            // Trigger the download through navigation.
            LoadUrlParams params =
                    new LoadUrlParams(mTestServer.getURL(TEST_DATA_DIRECTORY + TEST_FILE));
            mDownloadTestRule.getActivity().getActivityTab().loadUrl(params);
        });
    }

    /**
     * Provides default download directory and SD card directory.
     * @param hasSDCard Whether to simulate SD card inserted.
     */
    private void simulateDownloadDirectories(boolean hasSDCard) {
        ArrayList<DirectoryOption> dirs = new ArrayList<>();

        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            dirs.add(buildDirectoryOption(DirectoryOption.DownloadLocationDirectoryType.DEFAULT,
                    PathUtils.getExternalStorageDirectory()));
            if (hasSDCard) {
                dirs.add(buildDirectoryOption(
                        DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL,
                        PathUtils.getDataDirectory()));
            }
        }

        DownloadDirectoryProvider.getInstance().setDirectoryProviderForTesting(
                new TestDownloadDirectoryProvider(dirs));
    }

    private DirectoryOption buildDirectoryOption(
            @DirectoryOption.DownloadLocationDirectoryType int type, String directoryPath) {
        return new DirectoryOption("Download", directoryPath, STORAGE_SIZE, STORAGE_SIZE, type);
    }

    private void promptDownloadLocationDialog(@DownloadPromptStatus int promptStatus) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { DownloadLocationDialogBridge.setPromptForDownloadAndroid(promptStatus); });
    }
}
