// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * State for the header of the Autofill Assistant.
 */
@JNINamespace("autofill_assistant")
public class AssistantCollectUserDataModel extends PropertyModel {
    // TODO(crbug.com/806868): add |setAvailableProfiles| and |setAvailablePaymentMethods| from
    // native. Implement |setShippingAddress|, |setContactDetails| and |setPaymentMethod|.

    public static final WritableObjectPropertyKey<AssistantCollectUserDataDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** The web contents the payment request is associated with. */
    public static final WritableObjectPropertyKey<WebContents> WEB_CONTENTS =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** The chosen shipping address. */
    public static final WritableObjectPropertyKey<AutofillAddress> SHIPPING_ADDRESS =
            new WritableObjectPropertyKey<>();

    /** The chosen payment method (including billing address). */
    public static final WritableObjectPropertyKey<AutofillPaymentInstrument> PAYMENT_METHOD =
            new WritableObjectPropertyKey<>();

    /** The chosen contact details. */
    public static final WritableObjectPropertyKey<AutofillContact> CONTACT_DETAILS =
            new WritableObjectPropertyKey<>();

    /** The login section title. */
    public static final WritableObjectPropertyKey<String> LOGIN_SECTION_TITLE =
            new WritableObjectPropertyKey<>();

    /** The chosen login option. */
    public static final WritableObjectPropertyKey<AssistantLoginChoice> SELECTED_LOGIN =
            new WritableObjectPropertyKey<>();

    /** The status of the third party terms & conditions. */
    public static final WritableIntPropertyKey TERMS_STATUS = new WritableIntPropertyKey();

    /** The email address of the preferred profile. */
    public static final WritableObjectPropertyKey<String> DEFAULT_EMAIL =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey REQUEST_NAME = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_EMAIL = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_PHONE = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_SHIPPING_ADDRESS =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_PAYMENT =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<String> ACCEPT_TERMS_AND_CONDITIONS_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey SHOW_TERMS_AS_CHECKBOX =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey REQUEST_LOGIN_CHOICE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<List<PersonalDataManager.AutofillProfile>>
            AVAILABLE_PROFILES = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<AutofillPaymentInstrument>>
            AVAILABLE_AUTOFILL_PAYMENT_METHODS = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<List<String>> SUPPORTED_BASIC_CARD_NETWORKS =
            new WritableObjectPropertyKey<>();

    /** The available payment methods, e.g., |BASIC_CARD_METHOD_NAME|. */
    public static final WritableObjectPropertyKey<Map<String, PaymentMethodData>>
            SUPPORTED_PAYMENT_METHODS = new WritableObjectPropertyKey<>();

    /** The available login choices. */
    public static final WritableObjectPropertyKey<List<AssistantLoginChoice>> AVAILABLE_LOGINS =
            new WritableObjectPropertyKey<>();

    /** The currently expanded section (may be null). */
    public static final WritableObjectPropertyKey<AssistantVerticalExpander> EXPANDED_SECTION =
            new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey REQUIRE_BILLING_POSTAL_CODE =
            new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<String> BILLING_POSTAL_CODE_MISSING_TEXT =
            new WritableObjectPropertyKey<>();

    public AssistantCollectUserDataModel() {
        super(DELEGATE, WEB_CONTENTS, VISIBLE, SHIPPING_ADDRESS, PAYMENT_METHOD, CONTACT_DETAILS,
                LOGIN_SECTION_TITLE, SELECTED_LOGIN, TERMS_STATUS, DEFAULT_EMAIL, REQUEST_NAME,
                REQUEST_EMAIL, REQUEST_PHONE, REQUEST_SHIPPING_ADDRESS, REQUEST_PAYMENT,
                ACCEPT_TERMS_AND_CONDITIONS_TEXT, SHOW_TERMS_AS_CHECKBOX, REQUEST_LOGIN_CHOICE,
                AVAILABLE_PROFILES, AVAILABLE_AUTOFILL_PAYMENT_METHODS,
                SUPPORTED_BASIC_CARD_NETWORKS, SUPPORTED_PAYMENT_METHODS, AVAILABLE_LOGINS,
                EXPANDED_SECTION, REQUIRE_BILLING_POSTAL_CODE, BILLING_POSTAL_CODE_MISSING_TEXT);

        /**
         * Set initial state for basic type properties (others are implicitly null).
         * This is necessary to ensure that the initial UI state is consistent with the model.
         */
        set(VISIBLE, false);
        set(TERMS_STATUS, AssistantTermsAndConditionsState.NOT_SELECTED);
        set(REQUEST_NAME, false);
        set(REQUEST_EMAIL, false);
        set(REQUEST_PHONE, false);
        set(REQUEST_PAYMENT, false);
        set(REQUEST_SHIPPING_ADDRESS, false);
        set(REQUEST_LOGIN_CHOICE, false);
        set(REQUIRE_BILLING_POSTAL_CODE, false);
        set(DEFAULT_EMAIL, "");
    }

    @CalledByNative
    private void setRequestName(boolean requestName) {
        set(REQUEST_NAME, requestName);
    }

    @CalledByNative
    private void setRequestEmail(boolean requestEmail) {
        set(REQUEST_EMAIL, requestEmail);
    }

    @CalledByNative
    private void setRequestPhone(boolean requestPhone) {
        set(REQUEST_PHONE, requestPhone);
    }

    @CalledByNative
    private void setRequestShippingAddress(boolean requestShippingAddress) {
        set(REQUEST_SHIPPING_ADDRESS, requestShippingAddress);
    }

    @CalledByNative
    private void setRequestPayment(boolean requestPayment) {
        set(REQUEST_PAYMENT, requestPayment);
    }

    @CalledByNative
    private void setAcceptTermsAndConditionsText(String text) {
        set(ACCEPT_TERMS_AND_CONDITIONS_TEXT, text);
    }

    @CalledByNative
    private void setShowTermsAsCheckbox(boolean showTermsAsCheckbox) {
        set(SHOW_TERMS_AS_CHECKBOX, showTermsAsCheckbox);
    }

    @CalledByNative
    private void setRequireBillingPostalCode(boolean requireBillingPostalCode) {
        set(REQUIRE_BILLING_POSTAL_CODE, requireBillingPostalCode);
    }

    @CalledByNative
    private void setBillingPostalCodeMissingText(String text) {
        set(BILLING_POSTAL_CODE_MISSING_TEXT, text);
    }

    @CalledByNative
    private void setLoginSectionTitle(String loginSectionTitle) {
        set(LOGIN_SECTION_TITLE, loginSectionTitle);
    }

    @CalledByNative
    private void setRequestLoginChoice(boolean requestLoginChoice) {
        set(REQUEST_LOGIN_CHOICE, requestLoginChoice);
    }

    @CalledByNative
    private void setSupportedBasicCardNetworks(String[] supportedBasicCardNetworks) {
        set(SUPPORTED_BASIC_CARD_NETWORKS, Arrays.asList(supportedBasicCardNetworks));
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        set(VISIBLE, visible);
    }

    @CalledByNative
    private void setTermsStatus(@AssistantTermsAndConditionsState int termsStatus) {
        set(TERMS_STATUS, termsStatus);
    }

    @CalledByNative
    private void setWebContents(WebContents webContents) {
        set(WEB_CONTENTS, webContents);
    }

    @CalledByNative
    private void setDelegate(AssistantCollectUserDataDelegate delegate) {
        set(DELEGATE, delegate);
    }

    /** Creates an empty list of login options. */
    @CalledByNative
    private static List<AssistantLoginChoice> createLoginChoiceList() {
        return new ArrayList<>();
    }

    /** Appends a login choice to {@code logins}. */
    @CalledByNative
    private void addLoginChoice(List<AssistantLoginChoice> loginChoices, String identifier,
            String label, int priority) {
        loginChoices.add(new AssistantLoginChoice(identifier, label, priority));
    }

    /** Sets the list of available login choices. */
    @CalledByNative
    private void setLoginChoices(List<AssistantLoginChoice> loginChoices) {
        set(AVAILABLE_LOGINS, loginChoices);
    }
}
