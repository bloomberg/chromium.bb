// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for autofill keyboard accessory.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<>();
    private final AtomicReference<ViewGroup> mContainerRef = new AtomicReference<>();

    private void loadTestPage(boolean isRtl)
            throws InterruptedException, ExecutionException, TimeoutException {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html"
                + (isRtl ? " dir=\"rtl\"" : "") + "><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"text\" id=\"fn\" autocomplete=\"given-name\" autofocus/><br>"
                + "<input type=\"text\" id=\"ln\" autocomplete=\"family-name\" /><br>"
                + "<textarea id=\"sa\" autocomplete=\"street-address\"></textarea><br>"
                + "<input type=\"text\" id=\"a1\" autocomplete=\"address-line1\" /><br>"
                + "<input type=\"text\" id=\"a2\" autocomplete=\"address-line2\" /><br>"
                + "<input type=\"text\" id=\"ct\" autocomplete=\"address-level2\" /><br>"
                + "<input type=\"text\" id=\"zc\" autocomplete=\"postal-code\" /><br>"
                + "<input type=\"text\" id=\"em\" autocomplete=\"email\" /><br>"
                + "<input type=\"text\" id=\"ph\" autocomplete=\"tel\" /><br>"
                + "<input type=\"text\" id=\"fx\" autocomplete=\"fax\" /><br>"
                + "<select id=\"co\" autocomplete=\"country\"><br>"
                + "<option value=\"BR\">Brazil</option>"
                + "<option value=\"US\">United States</option>"
                + "</select>"
                + "<input type=\"submit\" />"
                + "</form></body></html>"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "Johnathan Smithonian-Jackson", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco",
                "", "94102", "", "US", "(415) 888-9999", "john.sj@acme-mail.inc", "en"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "Jane Erika Donovanova", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "", "US", "(415) 999-0000", "donovanova.j@acme-mail.inc", "en"));
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "Marcus McSpartangregor", "Acme Inc", "1 Main\nApt A", "CA", "San Francisco", "",
                "94102", "", "US", "(415) 999-0000", "marc@acme-mail.inc", "en"));
        setRtlForTesting(isRtl);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivityTestRule.getActivity().getActivityTab();
            mWebContentsRef.set(tab.getWebContents());
            mContainerRef.set(tab.getContentView());
        });
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), "fn");
    }

    /**
     * Autofocused fields should not show a keyboard accessory.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    @DisabledTest(message = "crbug.com/854224")
    // TODO(fhorschig): Figure out why this test exists. If a keyboard is shown, the accessory
    // should be there. If there is no keyboard, there shouldn't be an accessory. Looks more like a
    // keyboard test than an accessory test.
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        Assert.assertTrue("Keyboard accessory should be hidden.", isAccessoryGone());
    }

    /**
     * Tapping on an input field should show a keyboard and its keyboard accessory.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    @DisabledTest(message = "crbug.com/854224")
    public void testTapInputFieldShowsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        DOMUtils.clickNode(mWebContentsRef.get(), "fn");

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                ()
                        -> mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                                mActivityTestRule.getActivity(), mContainerRef.get())));
        Assert.assertTrue("Keyboard accessory should be showing.", isAccessoryVisible());
    }

    /**
     * Switching fields should re-scroll the keyboard accessory to the left.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    @DisabledTest(message = "crbug.com/836027")
    public void testSwitchFieldsRescrollsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        DOMUtils.clickNode(mWebContentsRef.get(), "em");

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                ()
                        -> mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                                mActivityTestRule.getActivity(), mContainerRef.get())));

        ThreadUtils.runOnUiThreadBlocking(() -> getSuggestionsComponent().scrollToPosition(2));

        assertSuggestionsScrollState(false, "Should keep the manual scroll position.");

        DOMUtils.clickNode(mWebContentsRef.get(), "ln");
        assertSuggestionsScrollState(true, "Should be scrolled back to position 0.");
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory.
     */
    @Test
    @MediumTest
    @Feature({"keyboard-accessory"})
    @DisabledTest(message = "crbug.com/847959")
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(false);
        DOMUtils.clickNode(mWebContentsRef.get(), "fn");

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                ()
                        -> mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                                mActivityTestRule.getActivity(), mContainerRef.get())));
        Assert.assertTrue("Keyboard accessory should be visible.", isAccessoryVisible());

        ThreadUtils.runOnUiThreadBlocking(() -> getSuggestionAt(0).performClick());

        CriteriaHelper.pollUiThread(Criteria.equals(false,
                ()
                        -> mActivityTestRule.getKeyboardDelegate().isKeyboardShowing(
                                mActivityTestRule.getActivity(), mContainerRef.get())));
        Assert.assertTrue("Keyboard accessory should be hidden.", isAccessoryGone());
    }

    private void assertSuggestionsScrollState(boolean isScrollingReset, String failureReason) {
        CriteriaHelper.pollUiThread(new Criteria(failureReason) {
            @Override
            public boolean isSatisfied() {
                return isScrollingReset
                        ? getSuggestionsComponent().computeHorizontalScrollOffset() <= 0
                        : getSuggestionsComponent().computeHorizontalScrollOffset() > 0;
            }
        });
    }

    private RecyclerView getSuggestionsComponent() {
        final ViewGroup keyboardAccessory = ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory));
        if (keyboardAccessory == null) return null; // It might still be loading, so don't assert!

        final View recyclerView = keyboardAccessory.findViewById(R.id.actions_view);
        if (recyclerView == null) return null; // It might still be loading, so don't assert!

        return (RecyclerView) recyclerView;
    }

    private View getSuggestionAt(int index) {
        ViewGroup recyclerView = getSuggestionsComponent();
        if (recyclerView == null) return null; // It might still be loading, so don't assert!

        return recyclerView.getChildAt(index);
    }

    private boolean isAccessoryVisible() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> {
            LinearLayout keyboard =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory);
            return keyboard != null && keyboard.getVisibility() == View.VISIBLE;
        });
    }

    private boolean isAccessoryGone() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(() -> {
            LinearLayout keyboard =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory);
            return keyboard == null || keyboard.getVisibility() == View.GONE;
        });
    }
}
