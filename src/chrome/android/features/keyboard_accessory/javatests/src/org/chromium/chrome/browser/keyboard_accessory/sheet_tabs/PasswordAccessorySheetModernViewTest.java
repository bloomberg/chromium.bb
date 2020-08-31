// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.MediumTest;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Switch;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.OptionToggle;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.ChipView;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * View tests for the password accessory sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordAccessorySheetModernViewTest {
    private final AccessorySheetTabModel mModel = new AccessorySheetTabModel();
    private AtomicReference<RecyclerView> mView = new AtomicReference<>();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AccessorySheetCoordinator accessorySheet =
                    new AccessorySheetCoordinator(mActivityTestRule.getActivity().findViewById(
                            R.id.keyboard_accessory_sheet_stub));
            accessorySheet.setTabs(new KeyboardAccessoryData.Tab[] {new KeyboardAccessoryData.Tab(
                    "Passwords", null, null, R.layout.password_accessory_sheet,
                    AccessoryTabType.ALL, new KeyboardAccessoryData.Tab.Listener() {
                        @Override
                        public void onTabCreated(ViewGroup view) {
                            mView.set((RecyclerView) view);
                            AccessorySheetTabViewBinder.initializeView(mView.get(), null);
                            PasswordAccessorySheetModernViewBinder.initializeView(
                                    mView.get(), mModel);
                        }

                        @Override
                        public void onTabShown() {}
                    })});
            accessorySheet.setHeight(
                    mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                            R.dimen.keyboard_accessory_sheet_height));
            accessorySheet.show();
        });
        CriteriaHelper.pollUiThread(Criteria.equals(true, () -> mView.get() != null));
    }

    @After
    public void tearDown() {
        mView.set(null);
    }

    @Test
    @MediumTest
    public void testAddingCaptionsToTheModelRendersThem() {
        assertThat(mView.get().getChildCount(), is(0));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(
                    new AccessorySheetDataPiece("Passwords", AccessorySheetDataPiece.Type.TITLE));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        View title = mView.get().findViewById(R.id.tab_title);
        assertThat(title, is(not(nullValue())));
        assertThat(title, instanceOf(TextView.class));
        assertThat(((TextView) title).getText(), is("Passwords"));
    }

    @Test
    @MediumTest
    public void testAddingUserInfoToTheModelRendersClickableActions() throws ExecutionException {
        final AtomicReference<Boolean> clicked = new AtomicReference<>(false);
        assertThat(mView.get().getChildCount(), is(0));

        UserInfo testInfo = new UserInfo("", false);
        testInfo.addField(new UserInfoField(
                "Name Suggestion", "Name Suggestion", "", false, item -> clicked.set(true)));
        testInfo.addField(new UserInfoField(
                "Password Suggestion", "Password Suggestion", "", true, item -> clicked.set(true)));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.add(new AccessorySheetDataPiece(
                    testInfo, AccessorySheetDataPiece.Type.PASSWORD_INFO));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));

        assertThat(getNameSuggestion().getPrimaryTextView().getText(), is("Name Suggestion"));
        assertThat(
                getPasswordSuggestion().getPrimaryTextView().getText(), is("Password Suggestion"));
        assertThat(getPasswordSuggestion().getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));

        TestThreadUtils.runOnUiThreadBlocking(getNameSuggestion()::performClick);
        assertThat(clicked.get(), is(true));
        clicked.set(false);
        TestThreadUtils.runOnUiThreadBlocking(getPasswordSuggestion()::performClick);
        assertThat(clicked.get(), is(true));
    }

    @Test
    @MediumTest
    public void testAddingUserInfoTitlesAreRenderedIfNotEmpty() {
        assertThat(mView.get().getChildCount(), is(0));
        final UserInfoField kUnusedInfoField =
                new UserInfoField("Unused Name", "Unused Password", "", false, cb -> {});

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserInfo sameOriginInfo = new UserInfo("", false);
            sameOriginInfo.addField(kUnusedInfoField);
            sameOriginInfo.addField(kUnusedInfoField);
            mModel.add(new AccessorySheetDataPiece(
                    sameOriginInfo, AccessorySheetDataPiece.Type.PASSWORD_INFO));

            UserInfo pslOriginInfo = new UserInfo("other.origin.eg", true);
            pslOriginInfo.addField(kUnusedInfoField);
            pslOriginInfo.addField(kUnusedInfoField);
            mModel.add(new AccessorySheetDataPiece(
                    pslOriginInfo, AccessorySheetDataPiece.Type.PASSWORD_INFO));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(2, () -> mView.get().getChildCount()));

        assertThat(getUserInfoAt(0).getTitle().isShown(), is(false));
        assertThat(getUserInfoAt(1).getTitle().isShown(), is(true));
        assertThat(getUserInfoAt(1).getTitle().getText(), is("other.origin.eg"));
    }

    @Test
    @MediumTest
    public void testOptionToggleRenderedIfNotEmpty() throws ExecutionException {
        assertThat(mView.get().getChildCount(), is(0));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OptionToggle toggle =
                    new OptionToggle("Save passwords for this site", false, result -> {});
            mModel.add(new AccessorySheetDataPiece(
                    toggle, AccessorySheetDataPiece.Type.OPTION_TOGGLE));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        View title = mView.get().findViewById(R.id.option_toggle_title);
        assertThat(title, is(not(nullValue())));
        assertThat(title, instanceOf(TextView.class));
        assertThat(((TextView) title).getText(), is("Save passwords for this site"));

        View subtitle = mView.get().findViewById(R.id.option_toggle_subtitle);
        assertThat(subtitle, is(not(nullValue())));
        assertThat(subtitle, instanceOf(TextView.class));
        assertThat(subtitle, withText(R.string.text_off));

        View switchView = mView.get().findViewById(R.id.option_toggle_switch);
        assertThat(switchView, is(not(nullValue())));
        assertThat(switchView, instanceOf(Switch.class));
        assertFalse(((Switch) switchView).isChecked());
    }

    @Test
    @MediumTest
    public void testClickingDisabledToggleInvokesCallbackToEnable() throws ExecutionException {
        AtomicReference<Boolean> toggleEnabled = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OptionToggle toggle =
                    new OptionToggle("Save passwords for this site", false, toggleEnabled::set);
            mModel.add(new AccessorySheetDataPiece(
                    toggle, AccessorySheetDataPiece.Type.OPTION_TOGGLE));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        TestThreadUtils.runOnUiThreadBlocking(
                mView.get().findViewById(R.id.option_toggle)::performClick);
        assertTrue(toggleEnabled.get());
    }

    @Test
    @MediumTest
    public void testClickingEnabledToggleInvokesCallbackToDisable() throws ExecutionException {
        AtomicReference<Boolean> toggleEnabled = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OptionToggle toggle =
                    new OptionToggle("Save passwords for this site", true, toggleEnabled::set);
            mModel.add(new AccessorySheetDataPiece(
                    toggle, AccessorySheetDataPiece.Type.OPTION_TOGGLE));
        });

        CriteriaHelper.pollUiThread(Criteria.equals(1, () -> mView.get().getChildCount()));
        TestThreadUtils.runOnUiThreadBlocking(
                mView.get().findViewById(R.id.option_toggle)::performClick);

        assertFalse(toggleEnabled.get());
    }

    private PasswordAccessoryInfoView getUserInfoAt(int index) {
        assertThat(mView.get().getChildCount(), is(greaterThan(index)));
        assertThat(mView.get().getChildAt(index), instanceOf(PasswordAccessoryInfoView.class));
        return (PasswordAccessoryInfoView) mView.get().getChildAt(index);
    }

    private ChipView getNameSuggestion() {
        View view = getUserInfoAt(0).findViewById(R.id.suggestion_text);
        assertThat(view, is(not(nullValue())));
        assertThat(view, instanceOf(ChipView.class));
        return (ChipView) view;
    }

    private ChipView getPasswordSuggestion() {
        View view = getUserInfoAt(0).findViewById(R.id.password_text);
        assertThat(view, is(not(nullValue())));
        assertThat(view, instanceOf(ChipView.class));
        return (ChipView) view;
    }
}
