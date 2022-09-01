// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.app.Activity;
import android.view.View;
import android.widget.AutoCompleteTextView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;

/** View tests for PasswordEditDialogWithDetailsView */
@RunWith(BaseJUnit4ClassRunner.class)
public class PasswordEditDialogWithDetailsViewTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final String INITIAL_USERNAME = "user2";
    private static final String CHANGED_USERNAME = "user21";
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String FOOTER = "Footer";
    private static final String PASSWORD_ERROR = "Enter password";

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    PasswordEditDialogWithDetailsView mDialogView;
    AutoCompleteTextView mUsernamesView;
    TextInputEditText mPasswordView;
    TextInputLayout mPasswordInputLayout;
    TextView mFooterView;
    String mUsername;
    String mCurrentPassword;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity());
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogView = (PasswordEditDialogWithDetailsView) sActivity.getLayoutInflater().inflate(
                    R.layout.password_edit_dialog_with_details, null);
            mUsernamesView = (AutoCompleteTextView) mDialogView.findViewById(R.id.username_view);
            mFooterView = (TextView) mDialogView.findViewById(R.id.footer);
            sActivity.setContentView(mDialogView);
            mPasswordView = (TextInputEditText) mDialogView.findViewById(R.id.password);
            mPasswordInputLayout =
                    (TextInputLayout) mDialogView.findViewById(R.id.password_text_input_layout);
        });
    }

    void handleUsernameSelection(String username) {
        mUsername = username;
    }

    void handlePasswordChanged(String password) {
        mCurrentPassword = password;
    }

    PropertyModel.Builder populateDialogPropertiesBuilder() {
        return new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                .with(PasswordEditDialogProperties.USERNAMES, Arrays.asList(USERNAMES))
                .with(PasswordEditDialogProperties.USERNAME, INITIAL_USERNAME)
                .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                .with(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK,
                        this::handleUsernameSelection)
                .with(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK,
                        this::handlePasswordChanged);
    }

    /** Tests that all the properties propagated correctly. */
    @Test
    @MediumTest
    public void testProperties() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder()
                                          .with(PasswordEditDialogProperties.FOOTER, FOOTER)
                                          .build();
            PropertyModelChangeProcessor.create(
                    model, mDialogView, PasswordEditDialogViewBinder::bind);
        });
        Assert.assertEquals("Username doesn't match the initial one", INITIAL_USERNAME,
                mUsernamesView.getText().toString());
        Assert.assertEquals(
                "Password doesn't match", INITIAL_PASSWORD, mPasswordView.getText().toString());
        Assert.assertEquals("Footer should be visible", View.VISIBLE, mFooterView.getVisibility());
    }

    /** Tests password changed callback. */
    @Test
    @MediumTest
    public void testPasswordEditing() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(
                    model, mDialogView, PasswordEditDialogViewBinder::bind);
            mPasswordView.setText(INITIAL_PASSWORD);
        });
        CriteriaHelper.pollUiThread(() -> mCurrentPassword.equals(INITIAL_PASSWORD));
        TestThreadUtils.runOnUiThreadBlocking(() -> mPasswordView.setText(CHANGED_PASSWORD));
        CriteriaHelper.pollUiThread(() -> mCurrentPassword.equals(CHANGED_PASSWORD));
    }

    /**
     * Tests that when the footer property is empty footer view is hidden.
     */
    @Test
    @MediumTest
    public void testEmptyFooter() {
        // Test with null footer property.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel nullModel = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(
                    nullModel, mDialogView, PasswordEditDialogViewBinder::bind);
        });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());

        // Test with footer property containing empty string.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel emptyModel = populateDialogPropertiesBuilder()
                                               .with(PasswordEditDialogProperties.FOOTER, "")
                                               .build();
            PropertyModelChangeProcessor.create(
                    emptyModel, mDialogView, PasswordEditDialogViewBinder::bind);
        });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());
    }

    /** Tests username selected callback. */
    @Test
    @MediumTest
    public void testUsernameSelection() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(
                    model, mDialogView, PasswordEditDialogViewBinder::bind);
            mUsernamesView.setText(CHANGED_USERNAME);
        });
        CriteriaHelper.pollUiThread(() -> mUsername.equals(CHANGED_USERNAME));
        TestThreadUtils.runOnUiThreadBlocking(() -> { mUsernamesView.setText(INITIAL_USERNAME); });
        CriteriaHelper.pollUiThread(() -> mUsername.equals(INITIAL_USERNAME));
    }

    /** Tests if the password error is displayed */
    @Test
    @MediumTest
    public void testPasswordError() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model =
                    populateDialogPropertiesBuilder()
                            .with(PasswordEditDialogProperties.PASSWORD_ERROR, PASSWORD_ERROR)
                            .build();
            PropertyModelChangeProcessor.create(
                    model, mDialogView, PasswordEditDialogViewBinder::bind);
        });
        Assert.assertEquals("Should display password error",
                mPasswordInputLayout.getError().toString(), PASSWORD_ERROR);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder()
                                          .with(PasswordEditDialogProperties.PASSWORD_ERROR, null)
                                          .build();
            PropertyModelChangeProcessor.create(
                    model, mDialogView, PasswordEditDialogViewBinder::bind);
        });
        Assert.assertTrue(
                "Password error should be reset now", mPasswordInputLayout.getError() == null);
    }
}
