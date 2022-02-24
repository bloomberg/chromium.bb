// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import static java.util.Arrays.asList;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AutoSignInCancelButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;

/**
 * View tests for the Account Selection component ensure that model changes are reflected in the
 * sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccountSelectionViewTest {
    private static final GURL TEST_PROFILE_PIC = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

    private static final Account ANA =
            new Account("Ana", "ana@email.example", "Ana Doe", "Ana", TEST_PROFILE_PIC, true);
    private static final Account NO_ONE =
            new Account("", "", "No Subject", "", TEST_PROFILE_PIC, true);
    private static final Account BOB = new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, true);

    @Mock
    private Callback<Account> mAccountCallback;
    private Runnable mAutoSignInCancelCallback;

    private BlankUiTestActivity mActivity;
    private ModelList mSheetItems;
    private View mContentView;
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems = new ModelList();
            mContentView = AccountSelectionCoordinator.setupContentView(mActivity, mSheetItems);
            mActivity.setContentView(mContentView);
        });
    }

    @Test
    @MediumTest
    public void testSignInTitleDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.add(new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.SIGN_IN)
                            .with(HeaderProperties.FORMATTED_RP_URL, "www.example.org")
                            .with(HeaderProperties.FORMATTED_IDP_URL, "www.idp.org")
                            .build()));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.header_title);

        assertEquals("Incorrect title",
                mActivity.getString(R.string.account_selection_sheet_title_explicit,
                        "www.example.org", "www.idp.org"),
                title.getText());
    }

    @Test
    @MediumTest
    public void testVerifyingTitleDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.add(new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.VERIFY)
                            .with(HeaderProperties.FORMATTED_RP_URL, "www.example.org")
                            .with(HeaderProperties.FORMATTED_IDP_URL, "www.idp.org")
                            .build()));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.header_title);

        assertEquals("Incorrect title", mActivity.getString(R.string.verify_sheet_title),
                title.getText());
    }

    @Test
    @MediumTest
    public void testAccountsChangedByModel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.addAll(
                    asList(buildAccountItem(ANA), buildAccountItem(NO_ONE), buildAccountItem(BOB)));
        });

        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        assertEquals("Incorrect account count", 3, getAccounts().getChildCount());
        assertEquals("Incorrect name", ANA.getName(), getAccountNameAt(0).getText());
        assertEquals("Incorrect email", ANA.getEmail(), getAccountEmailAt(0).getText());
        assertEquals("Incorrect name", NO_ONE.getName(), getAccountNameAt(1).getText());
        assertEquals("Incorrect email", NO_ONE.getEmail(), getAccountEmailAt(1).getText());
        assertEquals("Incorrect name", BOB.getName(), getAccountNameAt(2).getText());
        assertEquals("Incorrect email", BOB.getEmail(), getAccountEmailAt(2).getText());
    }

    @Test
    @MediumTest
    public void testAccountsAreClickable() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSheetItems.addAll(Collections.singletonList(buildAccountItem(ANA))); });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));

        TouchCommon.singleClickView(getAccounts().getChildAt(0));

        waitForEvent(mAccountCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testSingleAccountHasClickableButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Create an account with no callback to ensure the button callback
            // is the one that gets invoked.
            final MVCListAdapter.ListItem account_without_callback =
                    new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.ACCOUNT,
                            new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                    .with(AccountProperties.ACCOUNT, ANA)
                                    .with(AccountProperties.ON_CLICK_LISTENER, null)
                                    .build());

            mSheetItems.addAll(asList(account_without_callback, buildContinueButton(ANA, null)));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));
        assertNotNull(getAccounts().getChildAt(1));

        TouchCommon.singleClickView(getAccounts().getChildAt(1));

        waitForEvent(mAccountCallback).onResult(eq(ANA));
    }

    @Test
    @MediumTest
    public void testAutoSignInCancellButtonDisplayed() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Create an account with no callback to ensure the button callback
            // is the one that gets invoked.
            final MVCListAdapter.ListItem account_without_callback =
                    new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.ACCOUNT,
                            new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                                    .with(AccountProperties.ACCOUNT, ANA)
                                    .with(AccountProperties.ON_CLICK_LISTENER, null)
                                    .build());

            mSheetItems.addAll(asList(account_without_callback, buildCancelButton()));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));
        assertNotNull(getAccounts().getChildAt(1));
    }

    @Test
    @MediumTest
    public void testHeaderDisplayedForAutoSignIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.add(new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.HEADER,
                    new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                            .with(HeaderProperties.TYPE, HeaderType.AUTO_SIGN_IN)
                            .with(HeaderProperties.FORMATTED_RP_URL, "www.example.org")
                            .with(HeaderProperties.FORMATTED_IDP_URL, "www.idp.org")
                            .build()));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        TextView title = mContentView.findViewById(R.id.header_title);

        assertEquals("Incorrect title",
                mActivity.getString(R.string.account_selection_sheet_title_auto, "www.example.org",
                        "www.idp.org"),
                title.getText());
    }

    @Test
    @MediumTest
    public void testDataSharingConsentDisplayed() {
        final String idpUrl = "www.idp.org";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetItems.addAll(Collections.singletonList(buildDataSharingConsentItem(idpUrl)));
        });
        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);
        assertNotNull(getAccounts().getChildAt(0));
        TextView consent = mContentView.findViewById(R.id.user_data_sharing_consent);
        String expectedSharingConsentText =
                mActivity.getString(R.string.account_selection_data_sharing_consent, "www.idp.org");
        expectedSharingConsentText = expectedSharingConsentText.replaceAll("<[^>]*>", "");
        // We use toString() here because otherwise getText() returns a
        // Spanned, which is not equal to the string we get from the resources.
        assertEquals("Incorrect data sharing consent text", expectedSharingConsentText,
                consent.getText().toString());
        Spanned spannedString = (Spanned) consent.getText();
        ClickableSpan[] spans =
                spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
        assertEquals("Expected two clickable links", 2, spans.length);
    }

    /**
     * Tests that the brand foreground and the brand icon are used in the "Continue" button.
     */
    @Test
    @MediumTest
    public void testContinueButtonBranding() {
        final int expectedTextColor = Color.BLUE;
        final int expectedIconColor = Color.RED;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Bitmap brandIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
            brandIcon.eraseColor(expectedIconColor);
            IdentityProviderMetadata idpMetadata = new IdentityProviderMetadata(
                    expectedTextColor, /*brandBackgroundColor*/ Color.GREEN, brandIcon);

            mSheetItems.addAll(Collections.singletonList(buildContinueButton(ANA, idpMetadata)));
        });

        pollUiThread(() -> mContentView.getVisibility() == View.VISIBLE);

        assertNotNull(getAccounts().getChildAt(0));
        TextView continueButton = mContentView.findViewById(R.id.account_selection_continue_btn);

        assertEquals(expectedTextColor, continueButton.getTextColors().getDefaultColor());
    }

    private RecyclerView getAccounts() {
        return mContentView.findViewById(R.id.sheet_item_list);
    }

    private TextView getAccountNameAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.title);
    }

    private TextView getAccountEmailAt(int index) {
        return getAccounts().getChildAt(index).findViewById(R.id.description);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private MVCListAdapter.ListItem buildAccountItem(Account account) {
        return new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.ACCOUNT,
                new PropertyModel.Builder(AccountProperties.ALL_KEYS)
                        .with(AccountProperties.ACCOUNT, account)
                        .with(AccountProperties.ON_CLICK_LISTENER, mAccountCallback)
                        .build());
    }

    private MVCListAdapter.ListItem buildContinueButton(
            Account account, IdentityProviderMetadata idpMetadata) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ContinueButtonProperties.ALL_KEYS)
                        .with(ContinueButtonProperties.ACCOUNT, account)
                        .with(ContinueButtonProperties.ON_CLICK_LISTENER, mAccountCallback);
        if (idpMetadata != null) {
            modelBuilder.with(ContinueButtonProperties.IDP_METADATA, idpMetadata);
        }

        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ItemType.CONTINUE_BUTTON, modelBuilder.build());
    }

    private MVCListAdapter.ListItem buildCancelButton() {
        return new MVCListAdapter.ListItem(
                AccountSelectionProperties.ItemType.AUTO_SIGN_IN_CANCEL_BUTTON,
                new PropertyModel.Builder(AutoSignInCancelButtonProperties.ALL_KEYS)
                        .with(AutoSignInCancelButtonProperties.ON_CLICK_LISTENER,
                                mAutoSignInCancelCallback)
                        .build());
    }

    private MVCListAdapter.ListItem buildDataSharingConsentItem(String idpUrl) {
        DataSharingConsentProperties.Properties properties =
                new DataSharingConsentProperties.Properties();
        properties.mFormattedIdpUrl = idpUrl;
        properties.mTermsOfServiceUrl = "https://rp.com/tos";
        properties.mPrivacyPolicyUrl = "https://rp.com/privacy";

        return new MVCListAdapter.ListItem(AccountSelectionProperties.ItemType.DATA_SHARING_CONSENT,
                new PropertyModel.Builder(DataSharingConsentProperties.ALL_KEYS)
                        .with(DataSharingConsentProperties.PROPERTIES, properties)
                        .build());
    }
}
