// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/content/autofill_payment_app_factory.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_app_service_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/core/autofill_card_validation.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_experimental_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"

namespace payments {
namespace {

// Invokes the |callback| with |status|.
void CallStatusCallback(PaymentRequestState::StatusCallback callback,
                        bool status) {
  std::move(callback).Run(status);
}

// Posts the |callback| to be invoked with |status| asynchronously.
void PostStatusCallback(PaymentRequestState::StatusCallback callback,
                        bool status) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CallStatusCallback, std::move(callback), status));
}

}  // namespace

PaymentRequestState::PaymentRequestState(
    content::WebContents* web_contents,
    content::RenderFrameHost* initiator_render_frame_host,
    const GURL& top_level_origin,
    const GURL& frame_origin,
    const url::Origin& frame_security_origin,
    PaymentRequestSpec* spec,
    Delegate* delegate,
    const std::string& app_locale,
    autofill::PersonalDataManager* personal_data_manager,
    ContentPaymentRequestDelegate* payment_request_delegate,
    const ServiceWorkerPaymentApp::IdentityCallback& sw_identity_callback,
    JourneyLogger* journey_logger)
    : web_contents_(web_contents),
      initiator_render_frame_host_(initiator_render_frame_host),
      top_origin_(top_level_origin),
      frame_origin_(frame_origin),
      frame_security_origin_(frame_security_origin),
      app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      personal_data_manager_(personal_data_manager),
      journey_logger_(journey_logger),
      are_requested_methods_supported_(
          !spec_->supported_card_networks().empty()),
      payment_request_delegate_(payment_request_delegate),
      sw_identity_callback_(sw_identity_callback),
      profile_comparator_(app_locale, *spec) {
  PopulateProfileCache();

  // |web_contents_| is null in unit tests.
  PaymentAppService* service = PaymentAppServiceFactory::GetForContext(
      web_contents_ ? web_contents_->GetBrowserContext() : nullptr);
  number_of_payment_app_factories_ = service->GetNumberOfFactories();
  service->Create(weak_ptr_factory_.GetWeakPtr());

  spec_->AddObserver(this);
}

PaymentRequestState::~PaymentRequestState() {}

content::WebContents* PaymentRequestState::GetWebContents() {
  return web_contents_;
}

ContentPaymentRequestDelegate* PaymentRequestState::GetPaymentRequestDelegate()
    const {
  return payment_request_delegate_;
}

PaymentRequestSpec* PaymentRequestState::GetSpec() const {
  return spec_;
}

const GURL& PaymentRequestState::GetTopOrigin() {
  return top_origin_;
}

const GURL& PaymentRequestState::GetFrameOrigin() {
  return frame_origin_;
}

const url::Origin& PaymentRequestState::GetFrameSecurityOrigin() {
  return frame_security_origin_;
}

content::RenderFrameHost* PaymentRequestState::GetInitiatorRenderFrameHost()
    const {
  return initiator_render_frame_host_;
}

const std::vector<mojom::PaymentMethodDataPtr>&
PaymentRequestState::GetMethodData() const {
  return GetSpec()->method_data();
}

scoped_refptr<PaymentManifestWebDataService>
PaymentRequestState::GetPaymentManifestWebDataService() const {
  return GetPaymentRequestDelegate()->GetPaymentManifestWebDataService();
}

const std::vector<autofill::AutofillProfile*>&
PaymentRequestState::GetBillingProfiles() {
  return shipping_profiles_;
}

bool PaymentRequestState::IsRequestedAutofillDataAvailable() {
  return is_requested_autofill_data_available_;
}

bool PaymentRequestState::MayCrawlForInstallablePaymentApps() {
  return PaymentsExperimentalFeatures::IsEnabled(
             features::kAlwaysAllowJustInTimePaymentApp) ||
         !spec_->supports_basic_card();
}

void PaymentRequestState::OnPaymentAppInstalled(const url::Origin& origin,
                                                int64_t registration_id) {
  sw_identity_callback_.Run(origin, registration_id);
}

void PaymentRequestState::OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) {
  if (app->type() == PaymentApp::Type::AUTOFILL) {
    journey_logger_->SetEventOccurred(
        JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  } else if (base::Contains(app->GetAppMethodNames(), methods::kGooglePay) ||
             base::Contains(app->GetAppMethodNames(), methods::kAndroidPay)) {
    journey_logger_->SetEventOccurred(
        JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  } else {
    journey_logger_->SetEventOccurred(
        JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
  }
  available_apps_.emplace_back(std::move(app));
}

void PaymentRequestState::OnPaymentAppCreationError(
    const std::string& error_message) {
  get_all_payment_apps_error_ = error_message;
}

bool PaymentRequestState::SkipCreatingNativePaymentApps() const {
  return false;
}

void PaymentRequestState::OnCreatingNativePaymentAppsSkipped(
    content::PaymentAppProvider::PaymentApps unused_apps,
    ServiceWorkerPaymentAppFinder::InstallablePaymentApps
        unused_installable_apps) {
  NOTREACHED();
}

void PaymentRequestState::OnDoneCreatingPaymentApps() {
  DCHECK_NE(0U, number_of_payment_app_factories_);
  if (--number_of_payment_app_factories_ > 0U)
    return;

  SetDefaultProfileSelections();

  get_all_apps_finished_ = true;
  has_enrolled_instrument_ =
      std::any_of(available_apps_.begin(), available_apps_.end(),
                  [](const auto& app) { return app->HasEnrolledInstrument(); });
  are_requested_methods_supported_ |= !available_apps_.empty();
  NotifyOnGetAllPaymentAppsFinished();
  NotifyInitialized();

  // Fulfill the pending CanMakePayment call.
  if (can_make_payment_callback_)
    std::move(can_make_payment_callback_).Run(are_requested_methods_supported_);

  // Fulfill the pending HasEnrolledInstrument call.
  if (has_enrolled_instrument_callback_)
    std::move(has_enrolled_instrument_callback_).Run(has_enrolled_instrument_);

  // Fulfill the pending AreRequestedMethodsSupported call.
  if (are_requested_methods_supported_callback_)
    CheckRequestedMethodsSupported(
        std::move(are_requested_methods_supported_callback_));
}

void PaymentRequestState::OnPaymentResponseReady(
    mojom::PaymentResponsePtr payment_response) {
  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

void PaymentRequestState::OnPaymentResponseError(
    const std::string& error_message) {
  delegate_->OnPaymentResponseError(error_message);
}

void PaymentRequestState::OnSpecUpdated() {
  autofill::AutofillProfile* selected_shipping_profile =
      selected_shipping_profile_;
  autofill::AutofillProfile* selected_contact_profile =
      selected_contact_profile_;

  if (spec_->current_update_reason() ==
      PaymentRequestSpec::UpdateReason::RETRY) {
    if (spec_->has_shipping_address_error() && selected_shipping_profile) {
      invalid_shipping_profile_ = selected_shipping_profile;
      selected_shipping_profile_ = nullptr;
    }

    if (spec_->has_payer_error() && selected_contact_profile) {
      invalid_contact_profile_ = selected_contact_profile;
      selected_contact_profile_ = nullptr;
    }
  }

  if (spec_->selected_shipping_option_error().empty()) {
    selected_shipping_option_error_profile_ = nullptr;
  } else {
    selected_shipping_option_error_profile_ = selected_shipping_profile;
    selected_shipping_profile_ = nullptr;
    if (spec_->has_shipping_address_error() && selected_shipping_profile) {
      invalid_shipping_profile_ = selected_shipping_profile;
    }
  }

  is_waiting_for_merchant_validation_ = false;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::CanMakePayment(StatusCallback callback) {
  if (!get_all_apps_finished_) {
    DCHECK(!can_make_payment_callback_);
    can_make_payment_callback_ = std::move(callback);
    return;
  }

  PostStatusCallback(std::move(callback), are_requested_methods_supported_);
}

void PaymentRequestState::HasEnrolledInstrument(StatusCallback callback) {
  if (!get_all_apps_finished_) {
    DCHECK(!has_enrolled_instrument_callback_);
    has_enrolled_instrument_callback_ = std::move(callback);
    return;
  }

  PostStatusCallback(std::move(callback), has_enrolled_instrument_);
}

void PaymentRequestState::AreRequestedMethodsSupported(
    MethodsSupportedCallback callback) {
  if (!get_all_apps_finished_) {
    are_requested_methods_supported_callback_ = std::move(callback);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PaymentRequestState::CheckRequestedMethodsSupported,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentRequestState::OnAbort() {
  // Reset supported method callback when the merchant calls abort before
  // OnDoneCreatingPaymentApps().
  if (are_requested_methods_supported_callback_)
    are_requested_methods_supported_callback_.Reset();
}

void PaymentRequestState::CheckRequestedMethodsSupported(
    MethodsSupportedCallback callback) {
  DCHECK(get_all_apps_finished_);

  // Don't modify the value of |are_requested_methods_supported_|, because it's
  // used for canMakePayment().
  bool supported = are_requested_methods_supported_;
  if (supported && is_show_user_gesture_ &&
      base::Contains(spec_->payment_method_identifiers_set(),
                     methods::kBasicCard) &&
      !has_non_autofill_app_ && !has_enrolled_instrument_ &&
      PaymentsExperimentalFeatures::IsEnabled(
          features::kStrictHasEnrolledAutofillInstrument)) {
    supported = false;
    get_all_payment_apps_error_ = errors::kStrictBasicCardShowReject;
  }

  std::move(callback).Run(supported, get_all_payment_apps_error_);
}

std::string PaymentRequestState::GetAuthenticatedEmail() const {
  return payment_request_delegate_->GetAuthenticatedEmail();
}

void PaymentRequestState::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PaymentRequestState::GeneratePaymentResponse() {
  DCHECK(is_ready_to_pay());

  // Once the response is ready, will call back into OnPaymentResponseReady.
  response_helper_ = std::make_unique<PaymentResponseHelper>(
      app_locale_, spec_, selected_app_, payment_request_delegate_,
      selected_shipping_profile_, selected_contact_profile_, this);
}

void PaymentRequestState::OnPaymentAppWindowClosed() {
  DCHECK(selected_app_);
  response_helper_.reset();
  selected_app_->OnPaymentAppWindowClosed();
}

void PaymentRequestState::RecordUseStats() {
  if (ShouldShowShippingSection()) {
    DCHECK(selected_shipping_profile_);
    personal_data_manager_->RecordUseOf(*selected_shipping_profile_);
  }

  if (ShouldShowContactSection()) {
    DCHECK(selected_contact_profile_);

    // If the same address was used for both contact and shipping, the stats
    // should only be updated once.
    if (!ShouldShowShippingSection() || (selected_shipping_profile_->guid() !=
                                         selected_contact_profile_->guid())) {
      personal_data_manager_->RecordUseOf(*selected_contact_profile_);
    }
  }

  selected_app_->RecordUse();
}

void PaymentRequestState::SetAvailablePaymentAppForRetry() {
  DCHECK(selected_app_);
  base::EraseIf(available_apps_, [this](const auto& payment_app) {
    // Remove the app if it is not selected.
    return payment_app.get() != selected_app_;
  });
  is_retry_called_ = true;
}

void PaymentRequestState::AddAutofillPaymentApp(
    bool selected,
    const autofill::CreditCard& card) {
  auto app =
      AutofillPaymentAppFactory::ConvertCardToPaymentAppIfSupportedNetwork(
          card, weak_ptr_factory_.GetWeakPtr());
  if (!app)
    return;

  available_apps_.push_back(std::move(app));
  journey_logger_->SetEventOccurred(
      JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);

  if (selected) {
    SetSelectedApp(available_apps_.back().get(),
                   SectionSelectionStatus::kAddedSelected);
  }
}

void PaymentRequestState::AddAutofillShippingProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));
  // TODO(tmartino): Implement deduplication rules specific to shipping
  // profiles.
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  shipping_profiles_.push_back(new_cached_profile);

  if (selected) {
    SetSelectedShippingProfile(new_cached_profile,
                               SectionSelectionStatus::kAddedSelected);
  }
}

void PaymentRequestState::AddAutofillContactProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  contact_profiles_.push_back(new_cached_profile);

  if (selected) {
    SetSelectedContactProfile(new_cached_profile,
                              SectionSelectionStatus::kAddedSelected);
  }
}

void PaymentRequestState::SetSelectedShippingOption(
    const std::string& shipping_option_id) {
  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_OPTION);
  // This will inform the merchant and will lead to them calling updateWith with
  // new PaymentDetails.
  delegate_->OnShippingOptionIdSelected(shipping_option_id);
}

void PaymentRequestState::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile,
    SectionSelectionStatus selection_status) {
  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_ADDRESS);
  selected_shipping_profile_ = profile;

  // Changing the shipping address clears shipping address validation errors
  // from retry().
  invalid_shipping_profile_ = nullptr;

  // The user should not be able to click on pay until the callback from the
  // merchant.
  is_waiting_for_merchant_validation_ = true;

  // Start the normalization of the shipping address.
  payment_request_delegate_->GetAddressNormalizer()->NormalizeAddressAsync(
      *selected_shipping_profile_, /*timeout_seconds=*/2,
      base::BindOnce(&PaymentRequestState::OnAddressNormalized,
                     weak_ptr_factory_.GetWeakPtr()));
  IncrementSelectionStatus(JourneyLogger::Section::SECTION_SHIPPING_ADDRESS,
                           selection_status);
}

void PaymentRequestState::SetSelectedContactProfile(
    autofill::AutofillProfile* profile,
    SectionSelectionStatus selection_status) {
  selected_contact_profile_ = profile;

  // Changing the contact information clears contact information validation
  // errors from retry().
  invalid_contact_profile_ = nullptr;

  UpdateIsReadyToPayAndNotifyObservers();

  if (IsPaymentAppInvoked()) {
    delegate_->OnPayerInfoSelected(
        response_helper_->GeneratePayerDetail(profile));
  }
  IncrementSelectionStatus(JourneyLogger::Section::SECTION_CONTACT_INFO,
                           selection_status);
}

void PaymentRequestState::SetSelectedApp(
    PaymentApp* app,
    SectionSelectionStatus selection_status) {
  selected_app_ = app;
  UpdateIsReadyToPayAndNotifyObservers();
  IncrementSelectionStatus(JourneyLogger::Section::SECTION_PAYMENT_METHOD,
                           selection_status);
}

void PaymentRequestState::IncrementSelectionStatus(
    JourneyLogger::Section section,
    SectionSelectionStatus selection_status) {
  switch (selection_status) {
    case SectionSelectionStatus::kSelected:
      journey_logger_->IncrementSelectionChanges(section);
      break;
    case SectionSelectionStatus::kEditedSelected:
      journey_logger_->IncrementSelectionEdits(section);
      break;
    case SectionSelectionStatus::kAddedSelected:
      journey_logger_->IncrementSelectionAdds(section);
      break;
    default:
      NOTREACHED();
  }
}

const std::string& PaymentRequestState::GetApplicationLocale() {
  return app_locale_;
}

autofill::PersonalDataManager* PaymentRequestState::GetPersonalDataManager() {
  return personal_data_manager_;
}

autofill::RegionDataLoader* PaymentRequestState::GetRegionDataLoader() {
  return payment_request_delegate_->GetRegionDataLoader();
}

bool PaymentRequestState::IsPaymentAppInvoked() const {
  return !!response_helper_;
}

autofill::AddressNormalizer* PaymentRequestState::GetAddressNormalizer() {
  return payment_request_delegate_->GetAddressNormalizer();
}

bool PaymentRequestState::IsInitialized() const {
  return get_all_apps_finished_;
}

void PaymentRequestState::SelectDefaultShippingAddressAndNotifyObservers() {
  // Only pre-select an address if the merchant provided at least one selected
  // shipping option, and the top profile is complete. Assumes that profiles
  // have already been sorted for completeness and frecency.
  if (!shipping_profiles().empty() && spec_->selected_shipping_option() &&
      profile_comparator()->IsShippingComplete(shipping_profiles_[0])) {
    selected_shipping_profile_ = shipping_profiles()[0];
  }
  // Record the missing required fields (if any) of the most complete shipping
  // profile.
  profile_comparator()->RecordMissingFieldsOfShippingProfile(
      shipping_profiles().empty() ? nullptr : shipping_profiles()[0]);
  UpdateIsReadyToPayAndNotifyObservers();
}

bool PaymentRequestState::ShouldShowShippingSection() const {
  if (!spec_->request_shipping())
    return false;

  return selected_app_ ? !selected_app_->HandlesShippingAddress() : true;
}

bool PaymentRequestState::ShouldShowContactSection() const {
  if (spec_->request_payer_name() &&
      (!selected_app_ || !selected_app_->HandlesPayerName())) {
    return true;
  }
  if (spec_->request_payer_email() &&
      (!selected_app_ || !selected_app_->HandlesPayerEmail())) {
    return true;
  }
  if (spec_->request_payer_phone() &&
      (!selected_app_ || !selected_app_->HandlesPayerPhone())) {
    return true;
  }

  return false;
}

base::WeakPtr<PaymentRequestState> PaymentRequestState::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentRequestState::PopulateProfileCache() {
  std::vector<autofill::AutofillProfile*> profiles =
      personal_data_manager_->GetProfilesToSuggest();

  std::vector<autofill::AutofillProfile*> raw_profiles_for_filtering;
  raw_profiles_for_filtering.reserve(profiles.size());

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        std::make_unique<autofill::AutofillProfile>(*profiles[i]));
    raw_profiles_for_filtering.push_back(profile_cache_.back().get());
  }

  contact_profiles_ = profile_comparator()->FilterProfilesForContact(
      raw_profiles_for_filtering);
  shipping_profiles_ = profile_comparator()->FilterProfilesForShipping(
      raw_profiles_for_filtering);

  // Set the number of suggestions shown for the sections requested by the
  // merchant.
  if (ShouldShowContactSection()) {
    bool has_complete_contact =
        contact_profiles_.empty()
            ? false
            : profile_comparator()->IsContactInfoComplete(contact_profiles_[0]);
    is_requested_autofill_data_available_ &= has_complete_contact;
    journey_logger_->SetNumberOfSuggestionsShown(
        JourneyLogger::Section::SECTION_CONTACT_INFO, contact_profiles_.size(),
        has_complete_contact);
  }
  if (ShouldShowShippingSection()) {
    bool has_complete_shipping =
        shipping_profiles_.empty()
            ? false
            : profile_comparator()->IsShippingComplete(shipping_profiles_[0]);
    is_requested_autofill_data_available_ &= has_complete_shipping;
    journey_logger_->SetNumberOfSuggestionsShown(
        JourneyLogger::Section::SECTION_SHIPPING_ADDRESS,
        shipping_profiles_.size(), has_complete_shipping);
  }
}

void PaymentRequestState::SetDefaultProfileSelections() {
  // Contact profiles were ordered by completeness in addition to frecency;
  // the first one is the best default selection.
  if (!contact_profiles().empty() &&
      profile_comparator()->IsContactInfoComplete(contact_profiles_[0]))
    selected_contact_profile_ = contact_profiles()[0];

  // Record the missing required fields (if any) of the most complete contact
  // profile.
  profile_comparator()->RecordMissingFieldsOfContactProfile(
      contact_profiles().empty() ? nullptr : contact_profiles()[0]);

  // Sort apps.
  PaymentApp::SortApps(&available_apps_);

  selected_app_ = nullptr;
  if (!available_apps_.empty() && available_apps_[0]->CanPreselect()) {
    selected_app_ = available_apps_[0].get();
    UpdateIsReadyToPayAndNotifyObservers();
  }

  // Record the missing required payment fields when no complete payment
  // info exists.
  if (available_apps_.empty()) {
    if (spec_->supports_basic_card()) {
      // All fields are missing when basic-card is requested but no card exits.
      base::UmaHistogramSparse("PaymentRequest.MissingPaymentFields",
                               CREDIT_CARD_EXPIRED | CREDIT_CARD_NO_CARDHOLDER |
                                   CREDIT_CARD_NO_NUMBER |
                                   CREDIT_CARD_NO_BILLING_ADDRESS);
    }
  } else if (available_apps_[0]->type() == PaymentApp::Type::AUTOFILL) {
    // Record the missing fields (if any) of the most complete app when
    // it's autofill based. SW based apps are always complete.
    static_cast<const AutofillPaymentApp*>(available_apps_[0].get())
        ->RecordMissingFieldsForApp();
  }

  SelectDefaultShippingAddressAndNotifyObservers();

  journey_logger_->SetNumberOfSuggestionsShown(
      JourneyLogger::Section::SECTION_PAYMENT_METHOD, available_apps().size(),
      selected_app_);
}

void PaymentRequestState::UpdateIsReadyToPayAndNotifyObservers() {
  is_ready_to_pay_ =
      ArePaymentDetailsSatisfied() && ArePaymentOptionsSatisfied();
  NotifyOnSelectedInformationChanged();
}

void PaymentRequestState::NotifyOnGetAllPaymentAppsFinished() {
  for (auto& observer : observers_)
    observer.OnGetAllPaymentAppsFinished();
}

void PaymentRequestState::NotifyOnSelectedInformationChanged() {
  for (auto& observer : observers_)
    observer.OnSelectedInformationChanged();
}

bool PaymentRequestState::ArePaymentDetailsSatisfied() {
  // There is no need to check for supported networks, because only supported
  // apps are listed/created in the flow.
  return selected_app_ != nullptr && selected_app_->IsCompleteForPayment();
}

bool PaymentRequestState::ArePaymentOptionsSatisfied() {
  if (is_waiting_for_merchant_validation_)
    return false;

  if (ShouldShowShippingSection() &&
      (!spec_->selected_shipping_option() ||
       !profile_comparator()->IsShippingComplete(selected_shipping_profile_))) {
    return false;
  }

  if (ShouldShowContactSection() &&
      !profile_comparator()->IsContactInfoComplete(selected_contact_profile_)) {
    return false;
  }

  return true;
}

void PaymentRequestState::OnAddressNormalized(
    bool success,
    const autofill::AutofillProfile& normalized_profile) {
  delegate_->OnShippingAddressSelected(
      data_util::GetPaymentAddressFromAutofillProfile(normalized_profile,
                                                      app_locale_));
}

}  // namespace payments
