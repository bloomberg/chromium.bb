// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_STATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_STATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/content/service_worker_payment_instrument.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
class PersonalDataManager;
class RegionDataLoader;
}  // namespace autofill

namespace payments {

class ContentPaymentRequestDelegate;
class PaymentInstrument;

// Keeps track of the information currently selected by the user and whether the
// user is ready to pay. Uses information from the PaymentRequestSpec, which is
// what the merchant has specified, as input into the "is ready to pay"
// computation.
//
// The initialization state is observed by PaymentRequestDialogView for showing
// a "Loading..." spinner.
class PaymentRequestState : public PaymentResponseHelper::Delegate,
                            public PaymentRequestSpec::Observer,
                            public InitializationTask {
 public:
  // Any class call add itself as Observer via AddObserver() and receive
  // notification about the state changing.
  class Observer {
   public:
    // Called when finished getting all available payment instruments.
    virtual void OnGetAllPaymentInstrumentsFinished() = 0;

    // Called when the information (payment method, address/contact info,
    // shipping option) changes.
    virtual void OnSelectedInformationChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  class Delegate {
   public:
    // Called when the PaymentResponse is available.
    virtual void OnPaymentResponseAvailable(
        mojom::PaymentResponsePtr response) = 0;

    // Called when the invoked payment app failed.
    virtual void OnPaymentResponseError(const std::string& error_message) = 0;

    // Called when the shipping option has changed to |shipping_option_id|.
    virtual void OnShippingOptionIdSelected(std::string shipping_option_id) = 0;

    // Called when the shipping address has changed to |address|.
    virtual void OnShippingAddressSelected(
        mojom::PaymentAddressPtr address) = 0;

    virtual void OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Shows the status of the selected section. For example if the user changes
  // the shipping address section from A to B then the SectionSelectionStatus
  // will be kSelected. If the user edits an item like B from the shipping
  // address section before selecting it, then the SectionSelectionStatus will
  // be kEditedSelected. Finally if the user decides to add a new item like C to
  // the shipping address section, then the SectionSelectionStatus will be
  // kAddedSelected. IncrementSelectionStatus uses SectionSelectionStatus to log
  // the number of times that the user has decided to change, edit, or add the
  // selected item in any of the sections during payment process.
  enum class SectionSelectionStatus {
    // The newly selected section is neither edited nor added.
    kSelected = 1,
    // The newly selected section is edited before selection.
    kEditedSelected = 2,
    // The newly selected section is added before selection.
    kAddedSelected = 3,
  };

  using StatusCallback = base::OnceCallback<void(bool)>;
  using MethodsSupportedCallback =
      base::OnceCallback<void(bool methods_supported,
                              const std::string& error_message)>;

  PaymentRequestState(
      content::WebContents* web_contents,
      const GURL& top_level_origin,
      const GURL& frame_origin,
      PaymentRequestSpec* spec,
      Delegate* delegate,
      const std::string& app_locale,
      autofill::PersonalDataManager* personal_data_manager,
      ContentPaymentRequestDelegate* payment_request_delegate,
      base::WeakPtr<ServiceWorkerPaymentInstrument::IdentityObserver>
          sw_identity_observer,
      JourneyLogger* journey_logger);
  ~PaymentRequestState() override;

  // PaymentResponseHelper::Delegate
  void OnPaymentResponseReady(
      mojom::PaymentResponsePtr payment_response) override;
  void OnPaymentResponseError(const std::string& error_message) override;

  // PaymentRequestSpec::Observer
  void OnStartUpdating(PaymentRequestSpec::UpdateReason reason) override {}
  void OnSpecUpdated() override;

  // Checks whether support for the specified payment methods exist, either
  // because the user has a registered payment handler or because the browser
  // can do just-in-time registration for a suitable payment handler.
  void CanMakePayment(StatusCallback callback);

  // Checks whether the user has at least one instrument that satisfies the
  // specified supported payment methods asynchronously.
  void HasEnrolledInstrument(StatusCallback callback);

  // Checks if the payment methods that the merchant website have
  // requested are supported asynchronously. For example, may return true for
  // "basic-card", but false for "https://bobpay.com".
  void AreRequestedMethodsSupported(MethodsSupportedCallback callback);

  // Returns authenticated user email, or empty string.
  std::string GetAuthenticatedEmail() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Initiates the generation of the PaymentResponse. Callers should check
  // |is_ready_to_pay|, which is inexpensive.
  void GeneratePaymentResponse();

  // Cancels the generation of the PaymentResponse.
  void OnPaymentAppWindowClosed();

  // Record the use of the data models that were used in the Payment Request.
  void RecordUseStats();

  // Gets the Autofill Profile representing the shipping address or contact
  // information currently selected for this PaymentRequest flow. Can return
  // null.
  autofill::AutofillProfile* selected_shipping_profile() const {
    return selected_shipping_profile_;
  }
  // If |spec()->selected_shipping_option_error()| is not empty, this contains
  // the profile for which the error is about.
  autofill::AutofillProfile* selected_shipping_option_error_profile() const {
    return selected_shipping_option_error_profile_;
  }
  autofill::AutofillProfile* selected_contact_profile() const {
    return selected_contact_profile_;
  }
  autofill::AutofillProfile* invalid_shipping_profile() const {
    return invalid_shipping_profile_;
  }
  autofill::AutofillProfile* invalid_contact_profile() const {
    return invalid_contact_profile_;
  }
  // Returns the currently selected instrument for this PaymentRequest flow.
  // It's not guaranteed to be complete. Returns nullptr if there is no selected
  // instrument.
  PaymentInstrument* selected_instrument() const {
    return selected_instrument_;
  }

  // Returns the appropriate Autofill Profiles for this user. The profiles
  // returned are owned by the PaymentRequestState.
  const std::vector<autofill::AutofillProfile*>& shipping_profiles() {
    return shipping_profiles_;
  }
  const std::vector<autofill::AutofillProfile*>& contact_profiles() {
    return contact_profiles_;
  }
  const std::vector<std::unique_ptr<PaymentInstrument>>&
  available_instruments() {
    return available_instruments_;
  }

  // Creates and adds an AutofillPaymentInstrument, which makes a copy of
  // |card|. |selected| indicates if the newly-created instrument should be
  // selected, after which observers will be notified.
  void AddAutofillPaymentInstrument(bool selected,
                                    const autofill::CreditCard& card);

  // Creates and adds an AutofillProfile as a shipping profile, which makes a
  // copy of |profile|. |selected| indicates if the newly-created shipping
  // profile should be selected, after which observers will be notified.
  void AddAutofillShippingProfile(bool selected,
                                  const autofill::AutofillProfile& profile);

  // Creates and adds an AutofillProfile as a contact profile, which makes a
  // copy of |profile|. |selected| indicates if the newly-created shipping
  // profile should be selected, after which observers will be notified.
  void AddAutofillContactProfile(bool selected,
                                 const autofill::AutofillProfile& profile);

  // Setters to change the selected information. Will have the side effect of
  // recomputing "is ready to pay" and notify observers.
  void SetSelectedShippingOption(const std::string& shipping_option_id);
  void SetSelectedShippingProfile(autofill::AutofillProfile* profile,
                                  SectionSelectionStatus selection_status);
  void SetSelectedContactProfile(autofill::AutofillProfile* profile,
                                 SectionSelectionStatus selection_status);
  void SetSelectedInstrument(PaymentInstrument* instrument,
                             SectionSelectionStatus selection_status);

  bool is_ready_to_pay() { return is_ready_to_pay_; }

  // Checks whether getting all available instruments is finished.
  bool is_get_all_instruments_finished() {
    return get_all_instruments_finished_;
  }

  // Returns true after is_get_all_instruments_finished() is true and supported
  // payment method are found. Should not be called before
  // is_get_all_instruments_finished() is true.
  bool are_requested_methods_supported() const {
    return are_requested_methods_supported_;
  }

  const std::string& GetApplicationLocale();
  autofill::PersonalDataManager* GetPersonalDataManager();
  autofill::RegionDataLoader* GetRegionDataLoader();

  Delegate* delegate() { return delegate_; }

  PaymentsProfileComparator* profile_comparator() {
    return &profile_comparator_;
  }

  // Returns true if the payment app has been invoked and the payment response
  // generation has begun. False otherwise.
  bool IsPaymentAppInvoked() const;

  autofill::AddressNormalizer* GetAddressNormalizer();

  // InitializationTask:
  bool IsInitialized() const override;

  // Selects the default shipping address.
  void SelectDefaultShippingAddressAndNotifyObservers();

  base::WeakPtr<PaymentRequestState> AsWeakPtr();

 private:
  // Fetches the Autofill Profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this PaymentRequestState, in
  // profile_cache_.
  void PopulateProfileCache();

  // Sets the initial selections for instruments and profiles, and notifies
  // observers.
  void SetDefaultProfileSelections();

  // Uses the user-selected information as well as the merchant spec to update
  // |is_ready_to_pay_| with the current state, by validating that all the
  // required information is available. Will notify observers.
  void UpdateIsReadyToPayAndNotifyObservers();

  // Notifies all observers that getting all payment instruments is finished.
  void NotifyOnGetAllPaymentInstrumentsFinished();

  // Notifies all observers that selected information has changed.
  void NotifyOnSelectedInformationChanged();

  // Returns whether the selected data satisfies the PaymentDetails requirements
  // (payment methods).
  bool ArePaymentDetailsSatisfied();

  // Returns whether the selected data satisfies the PaymentOptions requirements
  // (contact info, shipping address).
  bool ArePaymentOptionsSatisfied();

  // The PaymentAppProvider::GetAllPaymentAppsCallback.
  void GetAllPaymentAppsCallback(
      content::WebContents* web_contents,
      const GURL& top_level_origin,
      const GURL& frame_origin,
      content::PaymentAppProvider::PaymentApps apps,
      ServiceWorkerPaymentAppFactory::InstallablePaymentApps installable_apps,
      const std::string& error_message);

  // The ServiceWorkerPaymentInstrument::ValidateCanMakePaymentCallback.
  void OnSWPaymentInstrumentValidated(
      ServiceWorkerPaymentInstrument* instrument,
      bool result);
  void FinishedGetAllSWPaymentInstruments();

  // Checks whether support for the specified payment methods exists and call
  // the |callback| to return the result.
  void CheckCanMakePayment(StatusCallback callback);

  // Checks whether the user has at least one instrument that satisfies the
  // specified supported payment methods and call the |callback| to return the
  // result.
  void CheckHasEnrolledInstrument(StatusCallback callback);

  // Checks if the payment methods that the merchant website have
  // requested are supported and call the |callback| to return the result.
  void CheckRequestedMethodsSupported(MethodsSupportedCallback callback);

  void OnAddressNormalized(bool success,
                           const autofill::AutofillProfile& normalized_profile);

  void IncrementSelectionStatus(JourneyLogger::Section section,
                                SectionSelectionStatus selection_status);

  // True when the requested autofill data (shipping address and/or contact
  // information) and payment data (either autofill or service worker) are
  // complete, valid, and selected.
  bool is_ready_to_pay_;

  // True when the requested autofill data (shipping address and/or contact
  // information) is complete and valid, even if not selected. This variable is
  // not affected by payment instruments.
  bool is_requested_autofill_data_available_ = true;

  bool get_all_instruments_finished_;

  // Whether the data is currently being validated by the merchant.
  bool is_waiting_for_merchant_validation_;

  const std::string app_locale_;

  // Not owned. Never null. Will outlive this object.
  PaymentRequestSpec* spec_;
  Delegate* delegate_;
  autofill::PersonalDataManager* personal_data_manager_;
  JourneyLogger* journey_logger_;

  StatusCallback can_make_payment_callback_;
  StatusCallback has_enrolled_instrument_callback_;
  MethodsSupportedCallback are_requested_methods_supported_callback_;
  bool are_requested_methods_supported_;
  std::string get_all_payment_apps_error_;

  autofill::AutofillProfile* selected_shipping_profile_;
  autofill::AutofillProfile* selected_shipping_option_error_profile_;
  autofill::AutofillProfile* selected_contact_profile_;
  autofill::AutofillProfile* invalid_shipping_profile_;
  autofill::AutofillProfile* invalid_contact_profile_;
  PaymentInstrument* selected_instrument_;

  // Number of pending service worker payment instruments waiting for
  // validation.
  int number_of_pending_sw_payment_instruments_;

  // Profiles may change due to (e.g.) sync events, so profiles are cached after
  // loading and owned here. They are populated once only, and ordered by
  // frecency.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;
  std::vector<autofill::AutofillProfile*> shipping_profiles_;
  std::vector<autofill::AutofillProfile*> contact_profiles_;

  // Credit cards are directly owned by the instruments in this list.
  std::vector<std::unique_ptr<PaymentInstrument>> available_instruments_;

  ContentPaymentRequestDelegate* payment_request_delegate_;
  base::WeakPtr<ServiceWorkerPaymentInstrument::IdentityObserver>
      sw_identity_observer_;

  std::unique_ptr<PaymentResponseHelper> response_helper_;

  PaymentsProfileComparator profile_comparator_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<PaymentRequestState> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestState);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_STATE_H_
