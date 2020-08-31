// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/security_state/ios/security_state_utils.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#include "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
std::unique_ptr<WebViewAutofillClientIOS> WebViewAutofillClientIOS::Create(
    web::WebState* web_state,
    ios_web_view::WebViewBrowserState* browser_state) {
  return std::make_unique<autofill::WebViewAutofillClientIOS>(
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale(),
      browser_state->GetPrefs(),
      ios_web_view::WebViewPersonalDataManagerFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewAutocompleteHistoryManagerFactory::
          GetForBrowserState(browser_state),
      web_state,
      ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewStrikeDatabaseFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewProfileSyncServiceFactory::GetForBrowserState(
          browser_state),
      // TODO(crbug.com/928595): Replace the closure with a callback to the
      // renderer that indicates if log messages should be sent from the
      // renderer.
      LogManager::Create(
          autofill::WebViewAutofillLogRouterFactory::GetForBrowserState(
              browser_state),
          base::Closure()));
}

WebViewAutofillClientIOS::WebViewAutofillClientIOS(
    const std::string& locale,
    PrefService* pref_service,
    PersonalDataManager* personal_data_manager,
    AutocompleteHistoryManager* autocomplete_history_manager,
    web::WebState* web_state,
    signin::IdentityManager* identity_manager,
    StrikeDatabase* strike_database,
    syncer::SyncService* sync_service,
    std::unique_ptr<autofill::LogManager> log_manager)
    : pref_service_(pref_service),
      personal_data_manager_(personal_data_manager),
      autocomplete_history_manager_(autocomplete_history_manager),
      web_state_(web_state),
      identity_manager_(identity_manager),
      payments_client_(std::make_unique<payments::PaymentsClient>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              web_state_->GetBrowserState()->GetURLLoaderFactory()),
          identity_manager_,
          personal_data_manager_,
          web_state_->GetBrowserState()->IsOffTheRecord())),
      form_data_importer_(
          std::make_unique<FormDataImporter>(this,
                                             payments_client_.get(),
                                             personal_data_manager_,
                                             locale)),
      strike_database_(strike_database),
      sync_service_(sync_service),
      log_manager_(std::move(log_manager)) {}

WebViewAutofillClientIOS::~WebViewAutofillClientIOS() {
  HideAutofillPopup(PopupHidingReason::kTabGone);
}

PersonalDataManager* WebViewAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

AutocompleteHistoryManager*
WebViewAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
}

PrefService* WebViewAutofillClientIOS::GetPrefs() {
  return pref_service_;
}

syncer::SyncService* WebViewAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

signin::IdentityManager* WebViewAutofillClientIOS::GetIdentityManager() {
  return identity_manager_;
}

FormDataImporter* WebViewAutofillClientIOS::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* WebViewAutofillClientIOS::GetPaymentsClient() {
  return payments_client_.get();
}

StrikeDatabase* WebViewAutofillClientIOS::GetStrikeDatabase() {
  return strike_database_;
}

ukm::UkmRecorder* WebViewAutofillClientIOS::GetUkmRecorder() {
  // UKM recording is not supported for WebViews.
  return nullptr;
}

ukm::SourceId WebViewAutofillClientIOS::GetUkmSourceId() {
  // UKM recording is not supported for WebViews.
  return 0;
}

AddressNormalizer* WebViewAutofillClientIOS::GetAddressNormalizer() {
  return nullptr;
}

security_state::SecurityLevel
WebViewAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  return security_state::GetSecurityLevelForWebState(web_state_);
}

void WebViewAutofillClientIOS::ShowAutofillSettings(
    bool show_credit_card_settings) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  [bridge_ showUnmaskPromptForCard:card reason:reason delegate:delegate];
}

void WebViewAutofillClientIOS::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  [bridge_ didReceiveUnmaskVerificationResult:result];
}

void WebViewAutofillClientIOS::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const base::string16&)> callback) {
  base::Optional<AccountInfo> primary_account_info =
      identity_manager_->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager_->GetPrimaryAccountInfo());
  base::string16 account_name =
      primary_account_info ? base::UTF8ToUTF16(primary_account_info->full_name)
                           : base::string16();
  [bridge_ confirmCreditCardAccountName:account_name
                               callback:std::move(callback)];
}

void WebViewAutofillClientIOS::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const base::string16&, const base::string16&)>
        callback) {
  [bridge_ confirmCreditCardExpirationWithCard:card
                                      callback:std::move(callback)];
}

void WebViewAutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  // No op. ios/web_view does not support local saves of autofill data.
}

void WebViewAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  [bridge_ confirmSaveCreditCardToCloud:card
                      legalMessageLines:legal_message_lines
                  saveCreditCardOptions:options
                               callback:std::move(callback)];
}

void WebViewAutofillClientIOS::CreditCardUploadCompleted(bool card_saved) {
  [bridge_ handleCreditCardUploadCompleted:card_saved];
}

void WebViewAutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {}

bool WebViewAutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void WebViewAutofillClientIOS::ScanCreditCard(CreditCardScanCallback callback) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
    bool /*unused_autoselect_first_suggestion*/,
    PopupType popup_type,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:suggestions popupDelegate:delegate];
}

void WebViewAutofillClientIOS::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  NOTREACHED();
}

base::span<const Suggestion> WebViewAutofillClientIOS::GetPopupSuggestions()
    const {
  NOTIMPLEMENTED();
  return base::span<const Suggestion>();
}

void WebViewAutofillClientIOS::PinPopupView() {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::UpdatePopup(
    const std::vector<Suggestion>& suggestions,
    PopupType popup_type) {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::HideAutofillPopup(PopupHidingReason reason) {
  [bridge_ hideAutofillPopup];
}

bool WebViewAutofillClientIOS::IsAutocompleteEnabled() {
  return false;
}

void WebViewAutofillClientIOS::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  [bridge_ propagateAutofillPredictionsForForms:forms];
}

void WebViewAutofillClientIOS::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

bool WebViewAutofillClientIOS::IsContextSecure() {
  return IsContextSecureForWebState(web_state_);
}

bool WebViewAutofillClientIOS::ShouldShowSigninPromo() {
  return false;
}

bool WebViewAutofillClientIOS::AreServerCardsSupported() {
  return true;
}

void WebViewAutofillClientIOS::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

LogManager* WebViewAutofillClientIOS::GetLogManager() const {
  return log_manager_.get();
}

}  // namespace autofill
