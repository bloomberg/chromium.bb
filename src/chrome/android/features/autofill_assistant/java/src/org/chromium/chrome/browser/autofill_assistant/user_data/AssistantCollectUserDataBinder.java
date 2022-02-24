// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.autofill_assistant.AssistantEditorFactory;
import org.chromium.chrome.browser.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.chrome.browser.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.chrome.browser.autofill_assistant.AssistantOptionModel.PaymentInstrumentModel;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataModel.LoginChoiceModel;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSection.Delegate;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionContainer;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is responsible for pushing updates to the Autofill Assistant UI for requesting user
 * data. These updates are pulled from the {@link AssistantCollectUserDataModel} when a notification
 * of an update is received.
 */
class AssistantCollectUserDataBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantCollectUserDataModel,
                AssistantCollectUserDataBinder.ViewHolder, PropertyKey> {

    /**
     * A wrapper class that holds the different views of the CollectUserData request.
     */
    static class ViewHolder {
        private final View mRootView;
        private final AssistantVerticalExpanderAccordion mPaymentRequestExpanderAccordion;
        private final int mSectionToSectionPadding;
        private final AssistantLoginSection mLoginSection;
        private final AssistantContactDetailsSection mContactDetailsSection;
        private final AssistantPhoneNumberSection mPhoneNumberSection;
        private final AssistantPaymentMethodSection mPaymentMethodSection;
        private final AssistantShippingAddressSection mShippingAddressSection;
        private final AssistantTermsSection mTermsSection;
        private final AssistantTermsSection mTermsAsCheckboxSection;
        private final AssistantInfoSection mInfoSection;
        private final AssistantAdditionalSectionContainer mPrependedSections;
        private final AssistantAdditionalSectionContainer mAppendedSections;
        private final ViewGroup mGenericUserInterfaceContainerPrepended;
        private final ViewGroup mGenericUserInterfaceContainerAppended;
        private final Object mDividerTag;
        private final Activity mActivity;
        private final AssistantEditorFactory mEditorFactory;
        private final WindowAndroid mWindowAndroid;

        public ViewHolder(View rootView, AssistantVerticalExpanderAccordion accordion,
                int sectionPadding, AssistantLoginSection loginSection,
                AssistantContactDetailsSection contactDetailsSection,
                AssistantPhoneNumberSection phoneNumberSection,
                AssistantPaymentMethodSection paymentMethodSection,
                AssistantShippingAddressSection shippingAddressSection,
                AssistantTermsSection termsSection, AssistantTermsSection termsAsCheckboxSection,
                AssistantInfoSection infoSection,
                AssistantAdditionalSectionContainer prependedSections,
                AssistantAdditionalSectionContainer appendedSections,
                ViewGroup genericUserInterfaceContainerPrepended,
                ViewGroup genericUserInterfaceContainerAppended, Object dividerTag,
                Activity activity, AssistantEditorFactory editorFactory,
                WindowAndroid windowAndroid) {
            mRootView = rootView;
            mPaymentRequestExpanderAccordion = accordion;
            mSectionToSectionPadding = sectionPadding;
            mLoginSection = loginSection;
            mContactDetailsSection = contactDetailsSection;
            mPhoneNumberSection = phoneNumberSection;
            mPaymentMethodSection = paymentMethodSection;
            mShippingAddressSection = shippingAddressSection;
            mTermsSection = termsSection;
            mTermsAsCheckboxSection = termsAsCheckboxSection;
            mInfoSection = infoSection;
            mPrependedSections = prependedSections;
            mAppendedSections = appendedSections;
            mGenericUserInterfaceContainerPrepended = genericUserInterfaceContainerPrepended;
            mGenericUserInterfaceContainerAppended = genericUserInterfaceContainerAppended;
            mDividerTag = dividerTag;
            mActivity = activity;
            mEditorFactory = editorFactory;
            mWindowAndroid = windowAndroid;
        }
    }

    @Override
    public void bind(
            AssistantCollectUserDataModel model, ViewHolder view, PropertyKey propertyKey) {
        boolean handled = updateEditors(model, propertyKey, view);
        handled = updateRootVisibility(model, propertyKey, view) || handled;
        handled = updateSectionVisibility(model, propertyKey, view) || handled;
        handled = updateSectionTitles(model, propertyKey, view) || handled;
        handled = updateSectionContents(model, propertyKey, view) || handled;
        handled = updateSectionSelectedItem(model, propertyKey, view) || handled;
        /* Update section paddings *after* updating section visibility. */
        handled = updateSectionPaddings(model, propertyKey, view) || handled;

        if (propertyKey == AssistantCollectUserDataModel.DELEGATE) {
            AssistantCollectUserDataDelegate collectUserDataDelegate =
                    model.get(AssistantCollectUserDataModel.DELEGATE);

            AssistantTermsSection.Delegate termsDelegate =
                    collectUserDataDelegate == null ? null : new AssistantTermsSection.Delegate() {
                        @Override
                        public void onStateChanged(@AssistantTermsAndConditionsState int state) {
                            collectUserDataDelegate.onTermsAndConditionsChanged(state);
                        }

                        @Override
                        public void onLinkClicked(int link) {
                            // TODO(b/143128544) refactor to do this the right way.
                            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                                view.mTermsSection.setTermsStatus(
                                        AssistantTermsAndConditionsState.NOT_SELECTED);
                                view.mTermsAsCheckboxSection.setTermsStatus(
                                        AssistantTermsAndConditionsState.NOT_SELECTED);
                            });
                            collectUserDataDelegate.onTextLinkClicked(link);
                        }
                    };
            view.mTermsSection.setDelegate(termsDelegate);
            view.mTermsAsCheckboxSection.setDelegate(termsDelegate);
            view.mInfoSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onTextLinkClicked
                            : null);
            view.mContactDetailsSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onContactInfoChanged);
            view.mPhoneNumberSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onPhoneNumberChanged);
            view.mPaymentMethodSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onPaymentMethodChanged);
            view.mShippingAddressSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onShippingAddressChanged);
            view.mLoginSection.setDelegate(collectUserDataDelegate == null
                            ? null
                            : collectUserDataDelegate::onLoginChoiceChanged);
            view.mPrependedSections.setDelegate(collectUserDataDelegate != null
                            ? getAdditionalSectionsDelegate(collectUserDataDelegate)
                            : null);
            view.mAppendedSections.setDelegate(collectUserDataDelegate != null
                            ? getAdditionalSectionsDelegate(collectUserDataDelegate)
                            : null);
        } else {
            assert handled : "Unhandled property detected in AssistantCollectUserDataBinder!";
        }
    }

    private Delegate getAdditionalSectionsDelegate(
            AssistantCollectUserDataDelegate collectUserDataDelegate) {
        return new Delegate() {
            @Override
            public void onValueChanged(String key, AssistantValue value) {
                collectUserDataDelegate.onKeyValueChanged(key, value);
            }

            @Override
            public void onInputTextFocusChanged(boolean isFocused) {
                collectUserDataDelegate.onInputTextFocusChanged(isFocused);
            }
        };
    }

    private boolean shouldShowContactDetails(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_NAME)
                || model.get(AssistantCollectUserDataModel.REQUEST_PHONE)
                || model.get(AssistantCollectUserDataModel.REQUEST_EMAIL);
    }

    private boolean shouldShowPhoneNumberSection(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY);
    }

    private boolean updateSectionTitles(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.CONTACT_SECTION_TITLE) {
            view.mContactDetailsSection.setTitle(
                    model.get(AssistantCollectUserDataModel.CONTACT_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PHONE_NUMBER_SECTION_TITLE) {
            view.mPhoneNumberSection.setTitle(
                    model.get(AssistantCollectUserDataModel.PHONE_NUMBER_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.LOGIN_SECTION_TITLE) {
            view.mLoginSection.setTitle(
                    model.get(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SHIPPING_SECTION_TITLE) {
            view.mShippingAddressSection.setTitle(
                    model.get(AssistantCollectUserDataModel.SHIPPING_SECTION_TITLE));
            return true;
        }
        return false;
    }

    /**
     * Updates the available items for each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionContents(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                view.mPaymentMethodSection.onAvailablePaymentMethodsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_CONTACTS) {
            if (shouldShowContactDetails(model)) {
                view.mContactDetailsSection.onContactsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_CONTACTS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS) {
            if (shouldShowPhoneNumberSection(model)) {
                view.mPhoneNumberSection.onPhoneNumbersChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                view.mShippingAddressSection.onAddressesChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_BILLING_ADDRESSES) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                view.mPaymentMethodSection.onAddressesChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_BILLING_ADDRESSES));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_LOGINS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
                List<AssistantLoginChoice> loginChoices =
                        model.get(AssistantCollectUserDataModel.AVAILABLE_LOGINS);
                if (loginChoices != null) {
                    List<LoginChoiceModel> loginChoiceModels = new ArrayList<>();
                    for (AssistantLoginChoice loginChoice : loginChoices) {
                        loginChoiceModels.add(new LoginChoiceModel(loginChoice));
                    }
                    view.mLoginSection.onLoginsChanged(loginChoiceModels);
                }
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PREPENDED_SECTIONS) {
            view.mPrependedSections.setSections(
                    model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.APPENDED_SECTIONS) {
            view.mAppendedSections.setSections(
                    model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT) {
            view.mTermsSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            view.mTermsAsCheckboxSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT) {
            view.mTermsSection.setTermsRequireReviewText(
                    model.get(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT));
            view.mTermsAsCheckboxSection.setTermsRequireReviewText(
                    model.get(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.INFO_SECTION_TEXT) {
            view.mInfoSection.setText(model.get(AssistantCollectUserDataModel.INFO_SECTION_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.INFO_SECTION_TEXT_CENTER) {
            view.mInfoSection.setCenter(
                    model.get(AssistantCollectUserDataModel.INFO_SECTION_TEXT_CENTER));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT) {
            view.mTermsSection.setPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT));
            view.mTermsAsCheckboxSection.setPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED) {
            view.mGenericUserInterfaceContainerPrepended.removeAllViews();
            if (model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED) != null) {
                view.mGenericUserInterfaceContainerPrepended.addView(
                        model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED) {
            view.mGenericUserInterfaceContainerAppended.removeAllViews();
            if (model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED) != null) {
                view.mGenericUserInterfaceContainerAppended.addView(
                        model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED));
            }
            return true;
        } else if (propertyKey
                == AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS) {
            view.mContactDetailsSection.setContactSummaryOptions(
                    model.get(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS) {
            view.mContactDetailsSection.setContactFullOptions(
                    model.get(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS));
            return true;
        }

        return false;
    }

    /**
     * Updates visibility of PR sections.
     * @return whether the property key was handled.
     */
    private boolean updateSectionVisibility(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey == AssistantCollectUserDataModel.REQUEST_NAME)
                || (propertyKey == AssistantCollectUserDataModel.REQUEST_EMAIL)
                || (propertyKey == AssistantCollectUserDataModel.REQUEST_PHONE)) {
            view.mContactDetailsSection.setVisible(shouldShowContactDetails(model));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY) {
            view.mPhoneNumberSection.setVisible(
                    model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS) {
            view.mShippingAddressSection.setVisible(
                    model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_PAYMENT) {
            view.mPaymentMethodSection.setVisible(
                    (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX) {
            if (model.get(AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX)) {
                view.mTermsSection.getView().setVisibility(View.GONE);
                view.mTermsAsCheckboxSection.getView().setVisibility(View.VISIBLE);
            } else {
                view.mTermsSection.getView().setVisibility(View.VISIBLE);
                view.mTermsAsCheckboxSection.getView().setVisibility(View.GONE);
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE) {
            view.mLoginSection.setVisible(
                    model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE));
            return true;
        }
        return false;
    }

    /**
     * Updates visibility of the root widget.
     * @return whether the property key was handled.
     */
    private boolean updateRootVisibility(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey != AssistantCollectUserDataModel.VISIBLE) {
            return false;
        }
        int visibility =
                model.get(AssistantCollectUserDataModel.VISIBLE) ? View.VISIBLE : View.GONE;
        if (view.mRootView.getVisibility() != visibility) {
            view.mRootView.setVisibility(visibility);
        }
        return true;
    }

    /**
     * Updates the currently selected item in each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionSelectedItem(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        // These changes are sent by the controller, do not notify it when selecting the added item.
        // This prevents creating a loop.
        if (propertyKey == AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                return true;
            }
            AddressModel shippingAddress =
                    model.get(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS);
            if (shippingAddress != null) {
                view.mShippingAddressSection.addOrUpdateItem(
                        shippingAddress, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                return true;
            }
            PaymentInstrumentModel paymentInstrument =
                    model.get(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT);
            if (paymentInstrument != null) {
                view.mPaymentMethodSection.addOrUpdateItem(
                        paymentInstrument, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS) {
            if (!shouldShowContactDetails(model)) {
                return true;
            }
            ContactModel contact =
                    model.get(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS);
            if (contact != null) {
                view.mContactDetailsSection.addOrUpdateItem(
                        contact, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY)) {
                return true;
            }
            ContactModel phone_number =
                    model.get(AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER);
            if (phone_number != null) {
                view.mPhoneNumberSection.addOrUpdateItem(
                        phone_number, /* select= */ true, /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.TERMS_STATUS) {
            int termsStatus = model.get(AssistantCollectUserDataModel.TERMS_STATUS);
            view.mTermsSection.setTermsStatus(termsStatus);
            view.mTermsAsCheckboxSection.setTermsStatus(termsStatus);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_LOGIN) {
            if (!model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
                return true;
            }
            LoginChoiceModel loginChoice = model.get(AssistantCollectUserDataModel.SELECTED_LOGIN);
            if (loginChoice != null) {
                view.mLoginSection.addOrUpdateItem(loginChoice,
                        /* select= */ true,
                        /* notify= */ false);
            }
            // No need to reset selection if null, this will be handled by setItems().
            return true;
        }
        return false;
    }

    /**
     * Updates the paddings between sections and section dividers.
     * @return whether the property key was handled.
     */
    private boolean updateSectionPaddings(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey != AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_NAME)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_EMAIL)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_PHONE)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_PAYMENT)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)
                && (propertyKey != AssistantCollectUserDataModel.EXPANDED_SECTION)
                && (propertyKey != AssistantCollectUserDataModel.PREPENDED_SECTIONS)
                && (propertyKey != AssistantCollectUserDataModel.APPENDED_SECTIONS)) {
            return false;
        }

        // Update section paddings such that the first and last section are flush to the top/bottom,
        // and all other sections have the same amount of padding in-between them.

        if (!model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS).isEmpty()) {
            view.mPrependedSections.setPaddings(
                    0, view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mLoginSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mContactDetailsSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPhoneNumberSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
            view.mLoginSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mContactDetailsSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPhoneNumberSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (shouldShowContactDetails(model)) {
            view.mContactDetailsSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mPhoneNumberSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY)) {
            view.mPhoneNumberSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
            view.mPaymentMethodSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
            view.mShippingAddressSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (!model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS).isEmpty()) {
            view.mAppendedSections.setPaddings(
                    0, view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        }
        view.mTermsSection.setPaddings(view.mSectionToSectionPadding, 0);
        // Do not set padding to the view.mTermsAsCheckboxSection, it already has "padding" from
        // its checkbox (that coincidentally matches the padding of mSectionToSectionPadding).
        view.mInfoSection.setPaddings(view.mSectionToSectionPadding, 0);

        // Hide dividers for currently invisible sections and after the expanded section, if any.
        boolean prevSectionIsExpandedOrInvisible = false;
        for (int i = 0; i < view.mPaymentRequestExpanderAccordion.getChildCount(); i++) {
            View child = view.mPaymentRequestExpanderAccordion.getChildAt(i);
            if (child instanceof AssistantVerticalExpander) {
                prevSectionIsExpandedOrInvisible = child.getVisibility() != View.VISIBLE
                        || (child instanceof AssistantVerticalExpander
                                && ((AssistantVerticalExpander) child).isExpanded());
            } else if (child.getTag() == view.mDividerTag) {
                child.setVisibility(prevSectionIsExpandedOrInvisible ? View.GONE : View.VISIBLE);
            } else {
                prevSectionIsExpandedOrInvisible = false;
            }
        }
        return true;
    }

    /**
     * Updates/recreates section editors.
     * @return whether the property key was handled.
     */
    private boolean updateEditors(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey != AssistantCollectUserDataModel.WEB_CONTENTS
                && propertyKey != AssistantCollectUserDataModel.REQUEST_NAME
                && propertyKey != AssistantCollectUserDataModel.REQUEST_EMAIL
                && propertyKey != AssistantCollectUserDataModel.REQUEST_PHONE
                && propertyKey != AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS
                && propertyKey != AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES
                && propertyKey != AssistantCollectUserDataModel.USE_GMS_CORE_EDIT_DIALOGS
                && propertyKey != AssistantCollectUserDataModel.ACCOUNT_EMAIL) {
            return false;
        }

        WebContents webContents = model.get(AssistantCollectUserDataModel.WEB_CONTENTS);
        if (webContents == null) {
            view.mContactDetailsSection.setEditor(null);
            view.mPaymentMethodSection.setEditor(null);
            view.mShippingAddressSection.setEditor(null);
            return true;
        }

        boolean shouldStoreChanges =
                model.get(AssistantCollectUserDataModel.SHOULD_STORE_USER_DATA_CHANGES);

        if (shouldShowContactDetails(model)) {
            updateContactEditors(model, view, webContents, shouldStoreChanges);
        }

        view.mShippingAddressSection.setEditor(view.mEditorFactory.createAddressEditor(
                webContents, view.mActivity, shouldStoreChanges));

        view.mPaymentMethodSection.setEditor(
                view.mEditorFactory.createPaymentInstrumentEditor(webContents, view.mActivity,
                        model.get(AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS),
                        shouldStoreChanges));

        return true;
    }

    private void updateContactEditors(AssistantCollectUserDataModel model, ViewHolder view,
            WebContents webContents, boolean shouldStoreChanges) {
        view.mContactDetailsSection.setRequestReloadOnChange(
                model.get(AssistantCollectUserDataModel.USE_GMS_CORE_EDIT_DIALOGS));
        if (model.get(AssistantCollectUserDataModel.USE_GMS_CORE_EDIT_DIALOGS)) {
            view.mContactDetailsSection.setEditor(
                    view.mEditorFactory.createAccountEditor(view.mActivity, view.mWindowAndroid,
                            model.get(AssistantCollectUserDataModel.ACCOUNT_EMAIL),
                            model.get(AssistantCollectUserDataModel.REQUEST_EMAIL)));
        } else {
            view.mContactDetailsSection.setEditor(view.mEditorFactory.createContactEditor(
                    webContents, view.mActivity,
                    model.get(AssistantCollectUserDataModel.REQUEST_NAME),
                    model.get(AssistantCollectUserDataModel.REQUEST_PHONE),
                    model.get(AssistantCollectUserDataModel.REQUEST_EMAIL), shouldStoreChanges));
        }
    }
}
