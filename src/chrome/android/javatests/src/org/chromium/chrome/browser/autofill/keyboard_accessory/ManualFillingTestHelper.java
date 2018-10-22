// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;

import static org.hamcrest.core.AllOf.allOf;

import static org.chromium.chrome.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_INVISIBLE;
import static org.chromium.chrome.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.support.design.widget.TabLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.PerformException;
import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.ViewInteraction;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.ViewGroup;

import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.DropdownPopupWindowInterface;
import org.chromium.ui.UiUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helpers in this class simplify interactions with the Keyboard Accessory and the sheet below it.
 */
public class ManualFillingTestHelper {
    private final ChromeTabbedActivityTestRule mActivityTestRule;
    private final AtomicReference<WebContents> mWebContentsRef = new AtomicReference<>();

    ManualFillingTestHelper(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    public void loadTestPage(boolean isRtl) throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(UrlUtils.encodeHtmlDataUri("<html"
                + (isRtl ? " dir=\"rtl\"" : "") + "><head>"
                + "<meta name=\"viewport\""
                + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
                + "<body><form method=\"POST\">"
                + "<input type=\"password\" id=\"password\"/><br>"
                + "<input type=\"text\" id=\"email\" autocomplete=\"email\" /><br>"
                + "<input type=\"submit\" id=\"submit\" />"
                + "</form></body></html>"));
        setRtlForTesting(isRtl);
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mWebContentsRef.set(
                                mActivityTestRule.getActivity().getActivityTab().getWebContents()));
        DOMUtils.waitForNonZeroNodeBounds(mWebContentsRef.get(), "password");
    }

    public void waitForKeyboard() {
        CriteriaHelper.pollUiThread(
                ()
                        -> UiUtils.isKeyboardShowing(InstrumentationRegistry.getContext(),
                                mActivityTestRule.getActivity().getCurrentFocus()));
    }

    public void waitForKeyboardToDisappear() {
        CriteriaHelper.pollUiThread(
                ()
                        -> !UiUtils.isKeyboardShowing(InstrumentationRegistry.getContext(),
                                mActivityTestRule.getActivity().getCurrentFocus()));
    }

    public void clickPasswordField() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "password");
    }

    public void clickEmailField() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "email");
    }

    public DropdownPopupWindowInterface waitForAutofillPopup()
            throws InterruptedException, ExecutionException, TimeoutException {
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();
        final ViewGroup view = webContents.getViewAndroidDelegate().getContainerView();
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                return view.findViewById(R.id.dropdown_popup_window) != null;
            }
        });
        View anchorView = view.findViewById(R.id.dropdown_popup_window);

        Assert.assertTrue(anchorView.getTag() instanceof DropdownPopupWindowInterface);
        final DropdownPopupWindowInterface popup =
                (DropdownPopupWindowInterface) anchorView.getTag();
        CriteriaHelper.pollUiThread(new Criteria("Autofill Popup anchor view was never added.") {
            @Override
            public boolean isSatisfied() {
                // Wait until the popup is showing and onLayout() has happened.
                return popup.isShowing() && popup.getListView() != null
                        && popup.getListView().getHeight() != 0;
            }
        });

        return popup;
    }

    /**
     * Although the submit button has no effect, it takes the focus from the input field and should
     * hide the keyboard.
     */
    public void clickSubmit() throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mWebContentsRef.get(), "submit");
    }

    /**
     * Creates and adds an empty tab without listener to keyboard accessory and sheet.
     */
    public void createTestTab() {
        mActivityTestRule.getActivity().getManualFillingController().getMediatorForTesting().addTab(
                new KeyboardAccessoryData.Tab(
                        AppCompatResources.getDrawable(InstrumentationRegistry.getContext(),
                                android.R.drawable.ic_lock_lock),
                        "TestTabDescription", R.layout.empty_accessory_sheet, AccessoryTabType.ALL,
                        null));
    }

    /**
     * Use in a |onView().perform| action to select the tab at |tabIndex| for the found tab layout.
     * @param tabIndex The index to be selected.
     * @return The action executed by |perform|.
     */
    static public ViewAction selectTabAtPosition(int tabIndex) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return allOf(isDisplayed(), isAssignableFrom(TabLayout.class));
            }

            @Override
            public String getDescription() {
                return "with tab at index " + tabIndex;
            }

            @Override
            public void perform(UiController uiController, View view) {
                TabLayout tabLayout = (TabLayout) view;
                if (tabLayout.getTabAt(tabIndex) == null) {
                    throw new PerformException.Builder()
                            .withCause(new Throwable("No tab at index " + tabIndex))
                            .build();
                }
                tabLayout.getTabAt(tabIndex).select();
            }
        };
    }

    /**
     * Use like {@link android.support.test.espresso.Espresso#onView}. It waits for a view matching
     * the given |matcher| to be displayed and allows to chain checks/performs on the result.
     * @param matcher The matcher matching exactly the view that is expected to be displayed.
     * @return An interaction on the view matching |matcher.
     */
    public static ViewInteraction whenDisplayed(Matcher<View> matcher) {
        onView(isRoot()).check((r, e) -> waitForView((ViewGroup) r, allOf(matcher, isDisplayed())));
        return onView(matcher);
    }

    public void waitToBeHidden(Matcher<View> matcher) {
        onView(isRoot()).check((r, e) -> {
            waitForView((ViewGroup) r, matcher, VIEW_INVISIBLE | VIEW_NULL | VIEW_GONE);
        });
    }
}
