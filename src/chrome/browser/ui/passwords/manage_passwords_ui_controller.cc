// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller_impl.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

using password_manager::PasswordFormManagerForUI;

int ManagePasswordsUIController::save_fallback_timeout_in_seconds_ = 90;

namespace {

password_manager::PasswordStore* GetPasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

std::vector<std::unique_ptr<autofill::PasswordForm>> CopyFormVector(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> result(forms.size());
  for (size_t i = 0; i < forms.size(); ++i)
    result[i] = std::make_unique<autofill::PasswordForm>(*forms[i]);
  return result;
}

const password_manager::InteractionsStats* FindStatsByUsername(
    base::span<const password_manager::InteractionsStats> stats,
    const base::string16& username) {
  auto it = std::find_if(stats.begin(), stats.end(),
                         [&username](const auto& element) {
                           return username == element.username_value;
                         });
  return it == stats.end() ? nullptr : &*it;
}

}  // namespace

ManagePasswordsUIController::ManagePasswordsUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      are_passwords_revealed_when_next_bubble_is_opened_(false) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
  if (password_store)
    password_store->AddObserver(this);
}

ManagePasswordsUIController::~ManagePasswordsUIController() {}

void ManagePasswordsUIController::OnPasswordSubmitted(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  // If the save bubble is already shown (possibly manual fallback for saving)
  // then ignore the changes because the user may interact with it right now.
  if (bubble_status_ == BubbleStatus::SHOWN &&
      GetState() == password_manager::ui::PENDING_PASSWORD_STATE)
    return;
  bool show_bubble = !form_manager->IsBlacklisted();
  DestroyAccountChooser();
  save_fallback_timer_.Stop();
  passwords_data_.OnPendingPassword(std::move(form_manager));
  if (show_bubble) {
    const password_manager::InteractionsStats* stats =
        GetCurrentInteractionStats();
    const int show_threshold =
        password_bubble_experiment::GetSmartBubbleDismissalThreshold();
    if (stats && show_threshold > 0 && stats->dismissal_count >= show_threshold)
      show_bubble = false;
  }
  if (show_bubble)
    bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnUpdatePasswordSubmitted(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  DestroyAccountChooser();
  save_fallback_timer_.Stop();
  passwords_data_.OnUpdatePassword(std::move(form_manager));
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnShowManualFallbackForSaving(
    std::unique_ptr<PasswordFormManagerForUI> form_manager,
    bool has_generated_password,
    bool is_update) {
  DestroyAccountChooser();
  if (has_generated_password)
    passwords_data_.OnAutomaticPasswordSave(std::move(form_manager));
  else if (is_update)
    passwords_data_.OnUpdatePassword(std::move(form_manager));
  else
    passwords_data_.OnPendingPassword(std::move(form_manager));
  UpdateBubbleAndIconVisibility();
  save_fallback_timer_.Start(
      FROM_HERE, GetTimeoutForSaveFallback(), this,
      &ManagePasswordsUIController::OnHideManualFallbackForSaving);
}

void ManagePasswordsUIController::OnHideManualFallbackForSaving() {
  if (passwords_data_.state() != password_manager::ui::PENDING_PASSWORD_STATE &&
      passwords_data_.state() !=
          password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
      passwords_data_.state() != password_manager::ui::CONFIRMATION_STATE) {
    return;
  }
  // Don't hide the fallback if the bubble is open.
  if (IsShowingBubble())
    return;

  save_fallback_timer_.Stop();

  ClearPopUpFlagForBubble();
  if (passwords_data_.GetCurrentForms().empty())
    passwords_data_.OnInactive();
  else
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);

  UpdateBubbleAndIconVisibility();
}

bool ManagePasswordsUIController::OnChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
    const GURL& origin,
    const ManagePasswordsState::CredentialsCallback& callback) {
  DCHECK(!local_credentials.empty());
  if (!HasBrowserWindow())
    return false;
  // If |local_credentials| contains PSL matches they shouldn't be propagated to
  // the state because PSL matches aren't saved for current page. This logic is
  // implemented here because Android uses ManagePasswordsState as a data source
  // for account chooser.
  CredentialManagerDialogController::FormsVector locals;
  if (!local_credentials[0]->is_public_suffix_match)
    locals = CopyFormVector(local_credentials);
  passwords_data_.OnRequestCredentials(std::move(locals), origin);
  passwords_data_.set_credentials_callback(callback);
  auto* raw_controller = new CredentialManagerDialogControllerImpl(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()), this);
  dialog_controller_.reset(raw_controller);
  raw_controller->ShowAccountChooser(CreateAccountChooser(raw_controller),
                                     std::move(local_credentials));
  UpdateBubbleAndIconVisibility();
  return true;
}

void ManagePasswordsUIController::OnAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {
  DCHECK(!local_forms.empty());
  DestroyAccountChooser();
  passwords_data_.OnAutoSignin(std::move(local_forms), origin);
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPromptEnableAutoSignin() {
  // Both the account chooser and the previous prompt shouldn't be closed.
  if (dialog_controller_)
    return;
  auto* raw_controller = new CredentialManagerDialogControllerImpl(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()), this);
  dialog_controller_.reset(raw_controller);
  raw_controller->ShowAutosigninPrompt(CreateAutoSigninPrompt(raw_controller));
}

void ManagePasswordsUIController::OnAutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  DestroyAccountChooser();
  save_fallback_timer_.Stop();
  passwords_data_.OnAutomaticPasswordSave(std::move(form_manager));
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordAutofilled(
    const std::vector<const autofill::PasswordForm*>& password_forms,
    const GURL& origin,
    const std::vector<const autofill::PasswordForm*>* federated_matches) {
  // To change to managed state only when the managed state is more important
  // for the user that the current state.
  if (passwords_data_.state() == password_manager::ui::INACTIVE_STATE ||
      passwords_data_.state() == password_manager::ui::MANAGE_STATE) {
    ClearPopUpFlagForBubble();
    passwords_data_.OnPasswordAutofilled(password_forms, origin,
                                         federated_matches);
    // Don't close the existing bubble. Update the icon later.
    if (bubble_status_ == BubbleStatus::SHOWN)
      bubble_status_ = BubbleStatus::SHOWN_PENDING_ICON_UPDATE;
    if (bubble_status_ != BubbleStatus::SHOWN_PENDING_ICON_UPDATE)
      UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::OnCredentialLeak(
    const password_manager::CredentialLeakType leak_type,
    const GURL& origin) {
  // Existing dialog shouldn't be closed.
  if (dialog_controller_)
    return;

  // Hide the manage passwords bubble if currently shown.
  if (IsShowingBubble())
    HidePasswordBubble();
  else
    ClearPopUpFlagForBubble();

  auto* raw_controller =
      new CredentialLeakDialogControllerImpl(this, leak_type, origin);
  dialog_controller_.reset(raw_controller);
  raw_controller->ShowCredentialLeakPrompt(
      CreateCredentialLeakPrompt(raw_controller));
}

void ManagePasswordsUIController::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  password_manager::ui::State current_state = GetState();
  passwords_data_.ProcessLoginsChanged(changes);
  if (current_state != GetState()) {
    ClearPopUpFlagForBubble();
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::UpdateIconAndBubbleState(
    ManagePasswordsIconView* icon) {
  if (ShouldBubblePopUp()) {
    DCHECK(!dialog_controller_);
    // This will detach any existing bubble so OnBubbleHidden() isn't called.
    weak_ptr_factory_.InvalidateWeakPtrs();
    // We must display the icon before showing the bubble, as the bubble would
    // be otherwise unanchored.
    icon->SetState(GetState());
    ShowBubbleWithoutUserInteraction();
    // If the bubble appeared then the status is updated in OnBubbleShown().
    ClearPopUpFlagForBubble();
  } else {
    password_manager::ui::State state = GetState();
    // The dialog should hide the icon.
    if (dialog_controller_ &&
        state == password_manager::ui::CREDENTIAL_REQUEST_STATE)
      state = password_manager::ui::INACTIVE_STATE;
    icon->SetState(state);
  }
}

base::WeakPtr<PasswordsModelDelegate>
ManagePasswordsUIController::GetModelDelegateProxy() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* ManagePasswordsUIController::GetWebContents() const {
  return web_contents();
}

const GURL& ManagePasswordsUIController::GetOrigin() const {
  return passwords_data_.origin();
}

password_manager::PasswordFormMetricsRecorder*
ManagePasswordsUIController::GetPasswordFormMetricsRecorder() {
  // The form manager may be null for example for auto sign-in toasts of the
  // credential manager API.
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return form_manager ? form_manager->GetMetricsRecorder() : nullptr;
}

password_manager::PasswordFeatureManager*
ManagePasswordsUIController::GetPasswordFeatureManager() {
  password_manager::PasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
  return client->GetPasswordFeatureManager();
}

password_manager::ui::State ManagePasswordsUIController::GetState() const {
  return passwords_data_.state();
}

const autofill::PasswordForm& ManagePasswordsUIController::GetPendingPassword()
    const {
  if (GetState() == password_manager::ui::AUTO_SIGNIN_STATE)
    return *GetCurrentForms()[0];

  DCHECK(GetState() == password_manager::ui::PENDING_PASSWORD_STATE ||
         GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         GetState() == password_manager::ui::CONFIRMATION_STATE)
      << GetState();
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return form_manager->GetPendingCredentials();
}

password_manager::metrics_util::CredentialSourceType
ManagePasswordsUIController::GetCredentialSource() const {
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return form_manager
             ? form_manager->GetCredentialSource()
             : password_manager::metrics_util::CredentialSourceType::kUnknown;
}

const std::vector<std::unique_ptr<autofill::PasswordForm>>&
ManagePasswordsUIController::GetCurrentForms() const {
  return passwords_data_.GetCurrentForms();
}

const password_manager::InteractionsStats*
ManagePasswordsUIController::GetCurrentInteractionStats() const {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return FindStatsByUsername(
      form_manager->GetInteractionsStats(),
      form_manager->GetPendingCredentials().username_value);
}

bool ManagePasswordsUIController::BubbleIsManualFallbackForSaving() const {
  return save_fallback_timer_.IsRunning();
}

void ManagePasswordsUIController::OnBubbleShown() {
  are_passwords_revealed_when_next_bubble_is_opened_ = false;
  bubble_status_ = BubbleStatus::SHOWN;
}

void ManagePasswordsUIController::OnBubbleHidden() {
  bool update_icon =
      (bubble_status_ == BubbleStatus::SHOWN_PENDING_ICON_UPDATE);
  bubble_status_ = BubbleStatus::NOT_SHOWN;
  if (GetState() == password_manager::ui::CONFIRMATION_STATE ||
      GetState() == password_manager::ui::AUTO_SIGNIN_STATE) {
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
    update_icon = true;
  }
  if (update_icon)
    UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnNoInteraction() {
  if (GetState() != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
      GetState() != password_manager::ui::PENDING_PASSWORD_STATE) {
    // Do nothing if the state was changed. It can happen for example when the
    // bubble is active and a page navigation happens.
    return;
  }
  bool is_update =
      GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNoInteraction(is_update);
}

void ManagePasswordsUIController::OnNopeUpdateClicked() {
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNopeUpdateClicked();
}

void ManagePasswordsUIController::NeverSavePassword() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  passwords_data_.form_manager()->OnNeverClicked();
  ClearPopUpFlagForBubble();
  if (passwords_data_.GetCurrentForms().empty())
    passwords_data_.OnInactive();
  else
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordsRevealed() {
  DCHECK(passwords_data_.form_manager());
  passwords_data_.form_manager()->OnPasswordsRevealed();
}

void ManagePasswordsUIController::SavePassword(const base::string16& username,
                                               const base::string16& password) {
  UpdatePasswordFormUsernameAndPassword(username, password,
                                        passwords_data_.form_manager());

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.PasswordSavedWithManualFallback",
                        BubbleIsManualFallbackForSaving());
  if (GetPasswordFormMetricsRecorder() && BubbleIsManualFallbackForSaving()) {
    GetPasswordFormMetricsRecorder()->RecordDetailedUserAction(
        password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
            kTriggeredManualFallbackForSaving);
  }

  save_fallback_timer_.Stop();
  passwords_data_.form_manager()->Save();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  // The icon is to be updated after the bubble (either "Save password" or "Sign
  // in to Chrome") is closed.
  bubble_status_ = BubbleStatus::SHOWN_PENDING_ICON_UPDATE;
  if (Browser* browser = chrome::FindBrowserWithWebContents(web_contents()))
    browser->window()->GetAutofillBubbleHandler()->OnPasswordSaved();
}

void ManagePasswordsUIController::ChooseCredential(
    const autofill::PasswordForm& form,
    password_manager::CredentialType credential_type) {
  DCHECK(dialog_controller_);
  DCHECK_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
            credential_type);
  // Copy the argument before destroying the controller. |form| is a member of
  // |dialog_controller_|.
  autofill::PasswordForm copy_form = form;
  dialog_controller_.reset();
  passwords_data_.ChooseCredential(&copy_form);
  ClearPopUpFlagForBubble();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::NavigateToPasswordManagerSettingsPage(
    password_manager::ManagePasswordsReferrer referrer) {
  NavigateToManagePasswordsPage(
      chrome::FindBrowserWithWebContents(web_contents()), referrer);
}

void ManagePasswordsUIController::NavigateToPasswordManagerAccountDashboard(
    password_manager::ManagePasswordsReferrer referrer) {
  NavigateToGooglePasswordManager(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      referrer);
}

void ManagePasswordsUIController::NavigateToPasswordCheckup() {
  NavigateToPasswordCheckupPage(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

void ManagePasswordsUIController::EnableSync(const AccountInfo& account,
                                             bool is_default_promo_account) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  signin_ui_util::EnableSyncFromPromo(
      browser, account,
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
      is_default_promo_account);
}

void ManagePasswordsUIController::OnDialogHidden() {
  dialog_controller_.reset();
  if (GetState() == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    ClearPopUpFlagForBubble();
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::OnLeakDialogHidden() {
  dialog_controller_.reset();
  if (GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    bubble_status_ = BubbleStatus::SHOULD_POP_UP;
    UpdateBubbleAndIconVisibility();
  }
}

bool ManagePasswordsUIController::AuthenticateUser() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ManagePasswordsUIController::RequestAuthenticationAndReopenBubble,
          weak_ptr_factory_.GetWeakPtr()));
  return false;
#else
  return true;
#endif
}

bool ManagePasswordsUIController::ArePasswordsRevealedWhenBubbleIsOpened()
    const {
  return are_passwords_revealed_when_next_bubble_is_opened_;
}

void ManagePasswordsUIController::HidePasswordBubble() {
  if (TabDialogs* tab_dialogs = TabDialogs::FromWebContents(web_contents()))
    tab_dialogs->HideManagePasswordsBubble();
}

void ManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  // If we're not on a "webby" URL (e.g. "chrome://sign-in"), we shouldn't
  // display either the bubble or the icon.
  if (!ChromePasswordManagerClient::CanShowBubbleOnURL(
          web_contents()->GetLastCommittedURL())) {
    ClearPopUpFlagForBubble();
    passwords_data_.OnInactive();
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  browser->window()->UpdatePageActionIcon(PageActionIconType::kManagePasswords);
}

AccountChooserPrompt* ManagePasswordsUIController::CreateAccountChooser(
    CredentialManagerDialogController* controller) {
  return CreateAccountChooserPromptView(controller, web_contents());
}

AutoSigninFirstRunPrompt* ManagePasswordsUIController::CreateAutoSigninPrompt(
    CredentialManagerDialogController* controller) {
  return CreateAutoSigninPromptView(controller, web_contents());
}

CredentialLeakPrompt* ManagePasswordsUIController::CreateCredentialLeakPrompt(
    CredentialLeakDialogController* controller) {
  return CreateCredentialLeakPromptView(controller, web_contents());
}

bool ManagePasswordsUIController::HasBrowserWindow() const {
  return chrome::FindBrowserWithWebContents(web_contents()) != nullptr;
}

void ManagePasswordsUIController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      // Don't react to same-document (fragment) navigations.
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Keep the state if the bubble is currently open or the fallback for saving
  // should be still available.
  if (IsShowingBubble() || save_fallback_timer_.IsRunning()) {
    return;
  }

  // Otherwise, reset the password manager.
  DestroyAccountChooser();
  ClearPopUpFlagForBubble();
  passwords_data_.OnInactive();
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HidePasswordBubble();
}

// static
base::TimeDelta ManagePasswordsUIController::GetTimeoutForSaveFallback() {
  return base::TimeDelta::FromSeconds(
      ManagePasswordsUIController::save_fallback_timeout_in_seconds_);
}

void ManagePasswordsUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK(ShouldBubblePopUp());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // Can be zero in the tests.
  if (!browser)
    return;

  chrome::ExecuteCommand(browser, IDC_MANAGE_PASSWORDS_FOR_PAGE);
}

void ManagePasswordsUIController::ClearPopUpFlagForBubble() {
  if (ShouldBubblePopUp())
    bubble_status_ = BubbleStatus::NOT_SHOWN;
}

void ManagePasswordsUIController::DestroyAccountChooser() {
  if (dialog_controller_ && dialog_controller_->IsShowingAccountChooser()) {
    dialog_controller_.reset();
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  }
}

void ManagePasswordsUIController::WebContentsDestroyed() {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents());
  if (password_store)
    password_store->RemoveObserver(this);
  HidePasswordBubble();
}

void ManagePasswordsUIController::RequestAuthenticationAndReopenBubble() {
  base::WeakPtr<ManagePasswordsUIController> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  bool auth_is_successful = ShowAuthenticationDialog();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ManagePasswordsUIController::ReopenBubbleAfterAuth,
                     weak_ptr, auth_is_successful));
}

void ManagePasswordsUIController::ReopenBubbleAfterAuth(
    bool auth_is_successful) {
  if (GetState() != password_manager::ui::PENDING_PASSWORD_STATE &&
      GetState() != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE)
    return;
  if (auth_is_successful)
    are_passwords_revealed_when_next_bubble_is_opened_ = true;
  bubble_status_ = BubbleStatus::SHOULD_POP_UP_AFTER_REAUTH;
  UpdateBubbleAndIconVisibility();
}

bool ManagePasswordsUIController::ShowAuthenticationDialog() {
#if defined(OS_WIN)
  return password_manager_util_win::AuthenticateUser(
      web_contents()->GetNativeView(),
      password_manager::ReauthPurpose::VIEW_PASSWORD);
#elif defined(OS_MACOSX)
  return password_manager_util_mac::AuthenticateUser(
      password_manager::ReauthPurpose::VIEW_PASSWORD);
#else
  NOTREACHED();
  return true;
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManagePasswordsUIController)
