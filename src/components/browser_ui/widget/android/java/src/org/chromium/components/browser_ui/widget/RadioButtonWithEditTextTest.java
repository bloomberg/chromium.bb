// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.InputType;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Unit tests for {@link RadioButtonWithEditText}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class RadioButtonWithEditTextTest extends DummyUiActivityTestCase {
    private class TestListener implements RadioButtonWithEditText.OnTextChangeListener {
        private CharSequence mCurrentText;
        private int mNumberOfTimesTextChanged;

        TestListener() {
            mNumberOfTimesTextChanged = 0;
        }

        /**
         * Will be called when the text edit has a value change.
         */
        @Override
        public void onTextChanged(CharSequence newText) {
            mCurrentText = newText;
            mNumberOfTimesTextChanged += 1;
        }

        void setCurrentText(CharSequence currentText) {
            mCurrentText = currentText;
        }

        /**
         * Get the current text stored inside
         * @return current text updated by RadioButtonWithEditText
         */
        CharSequence getCurrentText() {
            return mCurrentText;
        }

        int getTimesCalled() {
            return mNumberOfTimesTextChanged;
        }
    }

    private TestListener mListener;
    private Activity mActivity;

    private RadioButtonWithEditText mRadioButtonWithEditText;
    private RadioButton mButton;
    private EditText mEditText;

    private Button mDummyButton;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        mActivity = getActivity();
        mListener = new TestListener();

        setupViewsForTest();
    }

    private void setupViewsForTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View layout = LayoutInflater.from(mActivity).inflate(
                    R.layout.radio_button_with_edit_text_test, null, false);
            mActivity.setContentView(layout);

            mRadioButtonWithEditText =
                    (RadioButtonWithEditText) layout.findViewById(R.id.test_radio_button);
            mDummyButton = (Button) layout.findViewById(R.id.dummy_button);
            Assert.assertNotNull(mRadioButtonWithEditText);
            Assert.assertNotNull(mDummyButton);

            mButton = layout.findViewById(R.id.radio_button);
            mEditText = layout.findViewById(R.id.edit_text);

            Assert.assertNotNull("Radio Button should not be null", mButton);
            Assert.assertNotNull("Edit Text should not be null", mEditText);
        });
    }

    @Test
    @SmallTest
    public void testViewSetup() {
        Assert.assertFalse("Button should not be set checked after init.", mButton.isChecked());
        Assert.assertTrue(
                "Text entry should be empty after init.", TextUtils.isEmpty(mEditText.getText()));

        // Test if apply attr works
        int textUriInputType = InputType.TYPE_TEXT_VARIATION_URI | InputType.TYPE_CLASS_TEXT;
        Assert.assertEquals("EditText input type is different than attr setting.", textUriInputType,
                mEditText.getInputType());
        Assert.assertEquals("EditText input hint is different than attr setting.",
                mActivity.getResources().getString(R.string.test_uri), mEditText.getHint());

        TextView description = mActivity.findViewById(R.id.description);
        Assert.assertNotNull("Description should not be null", description);
        Assert.assertEquals("Description is different than attr setting.",
                mActivity.getResources().getString(R.string.test_string), description.getText());
    }

    @Test
    @SmallTest
    public void testSetHint() {
        final CharSequence hintMsg = "Text hint";
        final String resourceString = mActivity.getResources().getString(R.string.test_string);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRadioButtonWithEditText.setHint(hintMsg);
            Assert.assertEquals("Hint message set from string is different from test setting",
                    hintMsg.toString(), mEditText.getHint().toString());

            mRadioButtonWithEditText.setHint(R.string.test_string);
            Assert.assertEquals("Hint message set from resource id is different from test setting",
                    resourceString, mEditText.getHint().toString());
        });
    }

    @Test
    @SmallTest
    public void testSetInputType() {
        int[] commonInputTypes = {
                InputType.TYPE_CLASS_DATETIME,
                InputType.TYPE_CLASS_NUMBER,
                InputType.TYPE_CLASS_PHONE,
                InputType.TYPE_CLASS_TEXT,
                InputType.TYPE_TEXT_VARIATION_URI,
                InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS,
                InputType.TYPE_DATETIME_VARIATION_DATE,
        };

        for (int type : commonInputTypes) {
            mRadioButtonWithEditText.setInputType(type);
            Assert.assertEquals(mEditText.getInputType(), type);
        }
    }

    @Test
    @SmallTest
    public void testChangeEditText() {
        final CharSequence str1 = "First string";
        final CharSequence str2 = "SeConD sTrINg";

        CharSequence origStr = mRadioButtonWithEditText.getPrimaryText();

        // Test if changing the text edit will result in changing of listener
        mRadioButtonWithEditText.addTextChangeListener(mListener);
        mListener.setCurrentText(origStr);
        int timesCalled = mListener.getTimesCalled();

        // Test changes for edit text
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRadioButtonWithEditText.setPrimaryText(str1); });

        Assert.assertEquals("New String value should be updated", str1.toString(),
                mRadioButtonWithEditText.getPrimaryText().toString());
        Assert.assertEquals("Text message in listener should be updated accordingly",
                str1.toString(), mListener.getCurrentText().toString());
        Assert.assertEquals("TestListener#OnTextChanged should be called once", timesCalled + 1,
                mListener.getTimesCalled());

        // change to another text from View
        timesCalled = mListener.getTimesCalled();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mEditText.setText(str2); });

        Assert.assertEquals("New String value should be updated", str2.toString(),
                mRadioButtonWithEditText.getPrimaryText().toString());
        Assert.assertEquals("Text message in listener should be updated accordingly",
                str2.toString(), mListener.getCurrentText().toString());
        Assert.assertEquals("TestListener#OnTextChanged should be called once", timesCalled + 1,
                mListener.getTimesCalled());

        // change to another text from View
        mRadioButtonWithEditText.removeTextChangeListener(mListener);
        timesCalled = mListener.getTimesCalled();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRadioButtonWithEditText.setPrimaryText(str1); });

        Assert.assertEquals("New String value should be updated", str1.toString(),
                mRadioButtonWithEditText.getPrimaryText().toString());
        Assert.assertEquals("Text message in listener should not be updated.", str2.toString(),
                mListener.getCurrentText().toString());
        Assert.assertEquals("TestListener#OnTextChanged should not be called any more", timesCalled,
                mListener.getTimesCalled());
    }

    @Test
    @SmallTest
    public void testFocusChange() {
        Assert.assertFalse(mRadioButtonWithEditText.hasFocus());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mRadioButtonWithEditText.setChecked(true); });
        Assert.assertFalse("Edit text should not gain focus when radio button is checked",
                mEditText.hasFocus());
        Assert.assertFalse("Cursor in EditText should be hidden", mEditText.isCursorVisible());

        // Test requesting focus on the EditText
        TestThreadUtils.runOnUiThreadBlocking(() -> { mEditText.requestFocus(); });
        Assert.assertTrue("Cursor in EditText should be visible", mEditText.isCursorVisible());

        // Requesting focus elsewhere
        TestThreadUtils.runOnUiThreadBlocking(() -> { mDummyButton.requestFocus(); });
        Assert.assertFalse("Cursor in EditText should be visible", mEditText.isCursorVisible());
        assertIsKeyboardShowing(false);

        // Click EditText to show keyboard and checked the radio button.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRadioButtonWithEditText.setChecked(false); });
        TouchCommon.singleClickView(mEditText);
        Assert.assertTrue("Cursor in EditText should be visible", mEditText.isCursorVisible());
        assertIsKeyboardShowing(true);
        Assert.assertTrue("RadioButton should be checked after EditText gains focus.",
                mRadioButtonWithEditText.isChecked());

        // Test editor action
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_ENTER);
        Assert.assertFalse("Cursor in EditText should be visible", mEditText.isCursorVisible());
        assertIsKeyboardShowing(false);
    }

    @Test
    @SmallTest
    public void testSetEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mRadioButtonWithEditText.setEnabled(false); });
        Assert.assertFalse("Primary TextView should be set to disabled.",
                mRadioButtonWithEditText.getPrimaryTextView().isEnabled());
        Assert.assertFalse("Description TextView should be set to disabled.",
                mRadioButtonWithEditText.getDescriptionTextView().isEnabled());
        Assert.assertFalse("RadioButton should be set to disabled.",
                mRadioButtonWithEditText.getRadioButtonView().isEnabled());

        TestThreadUtils.runOnUiThreadBlocking(() -> { mRadioButtonWithEditText.setEnabled(true); });
        Assert.assertTrue("Primary TextView should be set to enabled.",
                mRadioButtonWithEditText.getPrimaryTextView().isEnabled());
        Assert.assertTrue("Description TextView should be set to enabled.",
                mRadioButtonWithEditText.getDescriptionTextView().isEnabled());
        Assert.assertTrue("RadioButton should be set to enabled.",
                mRadioButtonWithEditText.getRadioButtonView().isEnabled());
    }

    private void assertIsKeyboardShowing(boolean isShowing) {
        CriteriaHelper.pollUiThread(
                new Criteria("Keyboard visibility does not consist with test setting.") {
                    @Override
                    public boolean isSatisfied() {
                        return KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                                       mActivity, mEditText)
                                == isShowing;
                    }
                });
    }
}
