// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_gstatic_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if defined(OS_ANDROID)
#include "chrome/browser/autofill/android/save_update_address_profile_flow_manager.h"
#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#else  // !OS_ANDROID
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "components/zoom/zoom_observer.h"
#endif

namespace autofill {

class AutofillPopupControllerImpl;

// Chrome implementation of AutofillClient.
class ChromeAutofillClient
    : public AutofillClient,
      public content::WebContentsUserData<ChromeAutofillClient>,
      public content::WebContentsObserver
#if !defined(OS_ANDROID)
    ,
      public zoom::ZoomObserver
#endif  // !defined(OS_ANDROID)
{
 public:
  ChromeAutofillClient(const ChromeAutofillClient&) = delete;
  ChromeAutofillClient& operator=(const ChromeAutofillClient&) = delete;

  ~ChromeAutofillClient() override;

  // AutofillClient:
  version_info::Channel GetChannel() const override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::PaymentsClient* GetPaymentsClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  const GURL& GetLastCommittedURL() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  std::string GetVariationConfigCountryCode() const override;
  profile_metrics::BrowserProfileType GetProfileType() const override;
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(content::RenderFrameHost* rfh) override;

  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowCardUnmaskOtpInputDialog(
      const size_t& otp_length,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure) override;
  void DismissUnmaskAuthenticatorSelectionDialog(bool server_success) override;
#if !defined(OS_ANDROID)
  std::vector<std::string> GetAllowedMerchantsForVirtualCards() override;
  std::vector<std::string> GetAllowedBinRangesForVirtualCards() override;
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ShowLocalCardMigrationResults(
      const bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) override;
  void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) override;
  void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) override;
  void UpdateWebauthnOfferDialogWithError() override;
  bool CloseWebauthnDialog() override;
  void ConfirmSaveUpiIdLocally(
      const std::string& upi_id,
      base::OnceCallback<void(bool accept)> callback) override;
  void OfferVirtualCardOptions(
      const std::vector<CreditCard*>& candidates,
      base::OnceCallback<void(const std::string&)> callback) override;
#else  // defined(OS_ANDROID)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
#endif
  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  void ShowAutofillPopup(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override;
  base::span<const Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type) override;
  void HideAutofillPopup(PopupHidingReason reason) override;
  void ShowOfferNotificationIfApplicable(
      const AutofillOfferData* offer) override;
  void OnVirtualCardDataAvailable(
      const std::u16string& masked_card_identifier_string,
      const CreditCard* credit_card,
      const std::u16string& cvc,
      const gfx::Image& card_image) override;
  void ShowVirtualCardErrorDialog(bool is_permanent_error) override;
  void ShowAutofillProgressDialog(base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing) override;
  bool IsAutofillAssistantShowing() override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() const override;
  void ExecuteCommand(int id) override;
  LogManager* GetLogManager() const override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // content::WebContentsObserver implementation.
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_for_testing() {
    return popup_controller_;
  }
  void KeepPopupOpenForTesting() { keep_popup_open_for_testing_ = true; }

#if !defined(OS_ANDROID)
  // ZoomObserver implementation.
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif  // !defined(OS_ANDROID)

 private:
  friend class content::WebContentsUserData<ChromeAutofillClient>;

  explicit ChromeAutofillClient(content::WebContents* web_contents);

  Profile* GetProfile() const;
  bool IsMultipleAccountUser();
  std::u16string GetAccountHolderName();

  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;
  std::unique_ptr<LogManager> log_manager_;
  // If set to true, the popup will stay open regardless of external changes on
  // the test machine, that may normally cause the popup to be hidden
  bool keep_popup_open_for_testing_ = false;
#if defined(OS_ANDROID)
  CardExpirationDateFixFlowControllerImpl
      card_expiration_date_fix_flow_controller_;
  CardNameFixFlowControllerImpl card_name_fix_flow_controller_;
  SaveCardMessageControllerAndroid save_card_message_controller_android_;
  SaveUpdateAddressProfileFlowManager save_update_address_profile_flow_manager_;
#endif
  CardUnmaskPromptControllerImpl unmask_controller_;
  AutofillErrorDialogControllerImpl autofill_error_dialog_controller_;
  std::unique_ptr<AutofillProgressDialogControllerImpl>
      autofill_progress_dialog_controller_;

  // True if and only if the associated web_contents() is currently focused.
  bool has_focus_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
