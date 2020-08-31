// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_support_host.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/hash/sha1.h"
#include "base/i18n/timezone.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/screen.h"

using sync_pb::UserConsentTypes;

namespace {
constexpr char kAction[] = "action";
constexpr char kArcManaged[] = "arcManaged";
constexpr char kData[] = "data";
constexpr char kDeviceId[] = "deviceId";
constexpr char kActionInitialize[] = "initialize";
constexpr char kActionSetMetricsMode[] = "setMetricsMode";
constexpr char kActionBackupAndRestoreMode[] = "setBackupAndRestoreMode";
constexpr char kActionLocationServiceMode[] = "setLocationServiceMode";
constexpr char kActionSetWindowBounds[] = "setWindowBounds";
constexpr char kActionCloseWindow[] = "closeWindow";

// Action to show a page. The message should have "page" field, which is one of
// IDs for section div elements. For the "active-directory-auth" page, the
// "federationUrl" and "deviceManagementUrlPrefix" options are required.
constexpr char kActionShowPage[] = "showPage";
constexpr char kPage[] = "page";
constexpr char kOptions[] = "options";
constexpr char kFederationUrl[] = "federationUrl";
constexpr char kDeviceManagementUrlPrefix[] = "deviceManagementUrlPrefix";

// Action to show the error page. The message should have "errorMessage",
// which is a localized error text, and "shouldShowSendFeedback" boolean value.
constexpr char kActionShowErrorPage[] = "showErrorPage";
constexpr char kErrorMessage[] = "errorMessage";
constexpr char kShouldShowSendFeedback[] = "shouldShowSendFeedback";

// The preference update should have those two fields.
constexpr char kEnabled[] = "enabled";
constexpr char kManaged[] = "managed";

// The JSON data sent from the extension should have at least "event" field.
// Each event data is defined below.
// The key of the event type.
constexpr char kEvent[] = "event";

// "onWindowClosed" is fired when the extension window is closed.
// No data will be provided.
constexpr char kEventOnWindowClosed[] = "onWindowClosed";

// "onAuthSucceeded" is fired when Active Directory authentication succeeds.
constexpr char kEventOnAuthSucceeded[] = "onAuthSucceeded";

// "onAuthFailed" is fired when Active Directory authentication failed.
constexpr char kEventOnAuthFailed[] = "onAuthFailed";
constexpr char kAuthErrorMessage[] = "errorMessage";

// "onAgreed" is fired when a user clicks "Agree" button.
// The message should have the following fields:
// - tosContent
// - tosShown
// - isMetricsEnabled
// - isBackupRestoreEnabled
// - isBackupRestoreManaged
// - isLocationServiceEnabled
// - isLocationServiceManaged
constexpr char kEventOnAgreed[] = "onAgreed";
constexpr char kTosContent[] = "tosContent";
constexpr char kTosShown[] = "tosShown";
constexpr char kIsMetricsEnabled[] = "isMetricsEnabled";
constexpr char kIsBackupRestoreEnabled[] = "isBackupRestoreEnabled";
constexpr char kIsBackupRestoreManaged[] = "isBackupRestoreManaged";
constexpr char kIsLocationServiceEnabled[] = "isLocationServiceEnabled";
constexpr char kIsLocationServiceManaged[] = "isLocationServiceManaged";

// "onCanceled" is fired when user clicks "Cancel" button.
// The message should have the same fields as "onAgreed" above.
constexpr char kEventOnCanceled[] = "onCanceled";

// "onRetryClicked" is fired when a user clicks "RETRY" button on the error
// page.
constexpr char kEventOnRetryClicked[] = "onRetryClicked";

// "onSendFeedbackClicked" is fired when a user clicks "Send Feedback" button.
constexpr char kEventOnSendFeedbackClicked[] = "onSendFeedbackClicked";

// "onOpenPrivacySettingsPageClicked" is fired when a user clicks privacy
// settings link.
constexpr char kEventOnOpenPrivacySettingsPageClicked[] =
    "onOpenPrivacySettingsPageClicked";

void RequestOpenApp(Profile* profile) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          arc::kPlayStoreAppId);
  DCHECK(extension);
  DCHECK(extensions::util::IsAppLaunchable(arc::kPlayStoreAppId, profile));
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      .LaunchAppWithParams(CreateAppLaunchParamsUserContainer(
          profile, extension, WindowOpenDisposition::NEW_WINDOW,
          apps::mojom::AppLaunchSource::kSourceChromeInternal));
}

std::ostream& operator<<(std::ostream& os, ArcSupportHost::UIPage ui_page) {
  switch (ui_page) {
    case ArcSupportHost::UIPage::NO_PAGE:
      return os << "NO_PAGE";
    case ArcSupportHost::UIPage::TERMS:
      return os << "TERMS";
    case ArcSupportHost::UIPage::ARC_LOADING:
      return os << "ARC_LOADING";
    case ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH:
      return os << "ACTIVE_DIRECTORY_AUTH";
    case ArcSupportHost::UIPage::ERROR:
      return os << "ERROR";
  }

  // Some compiler reports an error even if all values of an enum-class are
  // covered indivisually in a switch statement.
  NOTREACHED();
  return os;
}

std::ostream& operator<<(std::ostream& os, ArcSupportHost::Error error) {
  switch (error) {
    case ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR:
      return os << "SIGN_IN_NETWORK_ERROR";
    case ArcSupportHost::Error::SIGN_IN_SERVICE_UNAVAILABLE_ERROR:
      return os << "SIGN_IN_SERVICE_UNAVAILABLE_ERROR";
    case ArcSupportHost::Error::SIGN_IN_BAD_AUTHENTICATION_ERROR:
      return os << "SIGN_IN_BAD_AUTHENTICATION_ERROR";
    case ArcSupportHost::Error::SIGN_IN_GMS_NOT_AVAILABLE_ERROR:
      return os << "SIGN_IN_GMS_NOT_AVAILABLE_ERROR";
    case ArcSupportHost::Error::SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR:
      return os << "SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR";
    case ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR:
      return os << "SIGN_IN_UNKNOWN_ERROR";
    case ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR:
      return os << "SERVER_COMMUNICATION_ERROR";
    case ArcSupportHost::Error::ANDROID_MANAGEMENT_REQUIRED_ERROR:
      return os << "ANDROID_MANAGEMENT_REQUIRED_ERROR";
    case ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR:
      return os << "NETWORK_UNAVAILABLE_ERROR";
  }

  // Some compiler reports an error even if all values of an enum-class are
  // covered indivisually in a switch statement.
  NOTREACHED();
  return os;
}

}  // namespace

ArcSupportHost::ArcSupportHost(Profile* profile)
    : profile_(profile),
      request_open_app_callback_(base::Bind(&RequestOpenApp)) {
  DCHECK(profile_);
}

ArcSupportHost::~ArcSupportHost() {
  // Delegates should have been reset to nullptr at this point.
  DCHECK(!auth_delegate_);
  DCHECK(!tos_delegate_);
  DCHECK(!error_delegate_);

  if (message_host_)
    DisconnectMessageHost();
}

void ArcSupportHost::SetAuthDelegate(AuthDelegate* delegate) {
  // Since AuthDelegate and TermsOfServiceDelegate should not have overlapping
  // life cycle, both delegates can't be non-null at the same time.
  DCHECK(!(delegate && tos_delegate_));
  auth_delegate_ = delegate;
}

void ArcSupportHost::SetTermsOfServiceDelegate(
    TermsOfServiceDelegate* delegate) {
  // Since AuthDelegate and TermsOfServiceDelegate should not have overlapping
  // life cycle, both delegates can't be non-null at the same time.
  DCHECK(!(delegate && auth_delegate_));
  tos_delegate_ = delegate;
}

void ArcSupportHost::SetErrorDelegate(ErrorDelegate* delegate) {
  error_delegate_ = delegate;
}

void ArcSupportHost::SetArcManaged(bool is_arc_managed) {
  DCHECK(!message_host_);
  is_arc_managed_ = is_arc_managed;
}

void ArcSupportHost::Close() {
  ui_page_ = UIPage::NO_PAGE;
  if (!message_host_) {
    VLOG(2) << "ArcSupportHost::Close() is called "
            << "but message_host_ is not available.";
    return;
  }

  base::DictionaryValue message;
  message.SetString(kAction, kActionCloseWindow);
  message_host_->SendMessage(message);

  // Disconnect immediately, so that onWindowClosed event will not be
  // delivered to here.
  DisconnectMessageHost();
}

void ArcSupportHost::ShowTermsOfService() {
  ShowPage(UIPage::TERMS);
}

void ArcSupportHost::ShowArcLoading() {
  ShowPage(UIPage::ARC_LOADING);
}

void ArcSupportHost::ShowActiveDirectoryAuth(
    const GURL& federation_url,
    const std::string& device_management_url_prefix) {
  active_directory_auth_federation_url_ = federation_url;
  active_directory_auth_device_management_url_prefix_ =
      device_management_url_prefix;
  ShowPage(UIPage::ACTIVE_DIRECTORY_AUTH);
}

void ArcSupportHost::ShowPage(UIPage ui_page) {
  ui_page_ = ui_page;
  if (!message_host_) {
    if (app_start_pending_) {
      VLOG(2) << "ArcSupportHost::ShowPage(" << ui_page << ") is called "
              << "before connection to ARC support Chrome app has finished "
              << "establishing.";
      return;
    }
    RequestAppStart();
    return;
  }

  base::DictionaryValue message;
  message.SetString(kAction, kActionShowPage);
  switch (ui_page) {
    case UIPage::TERMS:
      message.SetString(kPage, "terms");
      break;
    case UIPage::ARC_LOADING:
      message.SetString(kPage, "arc-loading");
      break;
    case UIPage::ACTIVE_DIRECTORY_AUTH:
      DCHECK(active_directory_auth_federation_url_.is_valid());
      DCHECK(!active_directory_auth_device_management_url_prefix_.empty());
      message.SetString(kPage, "active-directory-auth");
      message.SetPath(
          {kOptions, kFederationUrl},
          base::Value(active_directory_auth_federation_url_.spec()));
      message.SetPath(
          {kOptions, kDeviceManagementUrlPrefix},
          base::Value(active_directory_auth_device_management_url_prefix_));
      break;
    default:
      NOTREACHED();
      return;
  }
  message_host_->SendMessage(message);
}

void ArcSupportHost::ShowError(Error error, bool should_show_send_feedback) {
  ui_page_ = UIPage::ERROR;
  error_ = error;
  should_show_send_feedback_ = should_show_send_feedback;
  if (!message_host_) {
    if (app_start_pending_) {
      VLOG(2) << "ArcSupportHost::ShowError(" << error << ", "
              << should_show_send_feedback << ") is called before connection "
              << "to ARC support Chrome app is established.";
      return;
    }
    RequestAppStart();
    return;
  }

  base::DictionaryValue message;
  message.SetString(kAction, kActionShowErrorPage);
  int message_id;
  switch (error) {
    case Error::SIGN_IN_NETWORK_ERROR:
      message_id = IDS_ARC_SIGN_IN_NETWORK_ERROR;
      break;
    case Error::SIGN_IN_SERVICE_UNAVAILABLE_ERROR:
      message_id = IDS_ARC_SIGN_IN_SERVICE_UNAVAILABLE_ERROR;
      break;
    case Error::SIGN_IN_BAD_AUTHENTICATION_ERROR:
      message_id = IDS_ARC_SIGN_IN_BAD_AUTHENTICATION_ERROR;
      break;
    case Error::SIGN_IN_GMS_NOT_AVAILABLE_ERROR:
      message_id = IDS_ARC_SIGN_IN_GMS_NOT_AVAILABLE_ERROR;
      break;
    case Error::SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR:
      message_id = IDS_ARC_SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR;
      break;
    case Error::SIGN_IN_UNKNOWN_ERROR:
      message_id = IDS_ARC_SIGN_IN_UNKNOWN_ERROR;
      break;
    case Error::SERVER_COMMUNICATION_ERROR:
      message_id = IDS_ARC_SERVER_COMMUNICATION_ERROR;
      break;
    case Error::ANDROID_MANAGEMENT_REQUIRED_ERROR:
      message_id = IDS_ARC_ANDROID_MANAGEMENT_REQUIRED_ERROR;
      break;
    case Error::NETWORK_UNAVAILABLE_ERROR:
      message_id = IDS_ARC_NETWORK_UNAVAILABLE_ERROR;
      break;
  }
  message.SetString(kErrorMessage, l10n_util::GetStringUTF16(message_id));
  message.SetBoolean(kShouldShowSendFeedback, should_show_send_feedback);
  message_host_->SendMessage(message);
}

void ArcSupportHost::SetMetricsPreferenceCheckbox(bool is_enabled,
                                                  bool is_managed) {
  metrics_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionSetMetricsMode, metrics_checkbox_);
}

void ArcSupportHost::SetBackupAndRestorePreferenceCheckbox(bool is_enabled,
                                                           bool is_managed) {
  backup_and_restore_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionBackupAndRestoreMode,
                               backup_and_restore_checkbox_);
}

void ArcSupportHost::SetLocationServicesPreferenceCheckbox(bool is_enabled,
                                                           bool is_managed) {
  location_services_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionLocationServiceMode,
                               location_services_checkbox_);
}

void ArcSupportHost::SendPreferenceCheckboxUpdate(
    const std::string& action_name,
    const PreferenceCheckboxData& data) {
  if (!message_host_)
    return;

  base::DictionaryValue message;
  message.SetString(kAction, action_name);
  message.SetBoolean(kEnabled, data.is_enabled);
  message.SetBoolean(kManaged, data.is_managed);
  message_host_->SendMessage(message);
}

void ArcSupportHost::SetMessageHost(arc::ArcSupportMessageHost* message_host) {
  if (message_host_ == message_host)
    return;

  app_start_pending_ = false;
  if (message_host_)
    DisconnectMessageHost();
  message_host_ = message_host;
  message_host_->SetObserver(this);
  display::Screen* screen = display::Screen::GetScreen();
  if (screen)
    screen->AddObserver(this);

  if (!Initialize()) {
    Close();
    return;
  }

  // Hereafter, install the requested ui state into the ARC support Chrome app.

  // Set preference checkbox state.
  SendPreferenceCheckboxUpdate(kActionSetMetricsMode, metrics_checkbox_);
  SendPreferenceCheckboxUpdate(kActionBackupAndRestoreMode,
                               backup_and_restore_checkbox_);
  SendPreferenceCheckboxUpdate(kActionLocationServiceMode,
                               location_services_checkbox_);

  if (ui_page_ == UIPage::NO_PAGE) {
    // Close() is called before opening the window.
    Close();
  } else if (ui_page_ == UIPage::ERROR) {
    ShowError(error_, should_show_send_feedback_);
  } else {
    ShowPage(ui_page_);
  }
}

void ArcSupportHost::UnsetMessageHost(
    arc::ArcSupportMessageHost* message_host) {
  if (message_host_ != message_host)
    return;
  DisconnectMessageHost();
}

void ArcSupportHost::DisconnectMessageHost() {
  DCHECK(message_host_);
  display::Screen* screen = display::Screen::GetScreen();
  if (screen)
    screen->RemoveObserver(this);
  message_host_->SetObserver(nullptr);
  message_host_ = nullptr;
}

void ArcSupportHost::RequestAppStart() {
  DCHECK(!message_host_);
  DCHECK(!app_start_pending_);

  app_start_pending_ = true;
  request_open_app_callback_.Run(profile_);
}

void ArcSupportHost::SetRequestOpenAppCallbackForTesting(
    const RequestOpenAppCallback& callback) {
  DCHECK(!message_host_);
  DCHECK(!app_start_pending_);
  DCHECK(!callback.is_null());
  request_open_app_callback_ = callback;
}

bool ArcSupportHost::Initialize() {
  DCHECK(message_host_);

  const bool is_child =
      user_manager::UserManager::Get()->IsLoggedInAsChildUser();

  auto loadtime_data = std::make_unique<base::DictionaryValue>();
  loadtime_data->SetString("appWindow", l10n_util::GetStringUTF16(
                                            IDS_ARC_PLAYSTORE_ICON_TITLE_BETA));
  loadtime_data->SetString(
      "greetingHeader", l10n_util::GetStringUTF16(IDS_ARC_OOBE_TERMS_HEADING));
  loadtime_data->SetString(
      "initializingHeader",
      l10n_util::GetStringUTF16(IDS_ARC_PLAYSTORE_SETTING_UP_TITLE));
  loadtime_data->SetString(
      "greetingDescription",
      l10n_util::GetStringUTF16(IDS_ARC_OOBE_TERMS_DESCRIPTION));
  loadtime_data->SetString(
      "buttonAgree",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE));
  loadtime_data->SetString(
      "buttonCancel",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_CANCEL));
  loadtime_data->SetString(
      "buttonNext",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_NEXT));
  loadtime_data->SetString(
      "buttonSendFeedback",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_SEND_FEEDBACK));
  loadtime_data->SetString(
      "buttonRetry",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_RETRY));
  loadtime_data->SetString(
      "progressTermsLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_TERMS));
  loadtime_data->SetString(
      "progressAndroidLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_ANDROID));
  loadtime_data->SetString(
      "authorizationFailed",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_AUTHORIZATION_FAILED));
  loadtime_data->SetString(
      "termsOfService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_TERMS_OF_SERVICE));
  loadtime_data->SetString(
      "textMetricsEnabled",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED));
  loadtime_data->SetString(
      "textMetricsDisabled",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED));
  loadtime_data->SetString(
      "textMetricsManagedEnabled",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED));
  loadtime_data->SetString(
      "textMetricsManagedDisabled",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED));
  loadtime_data->SetString(
      "textBackupRestore",
      l10n_util::GetStringUTF16(is_child
                                    ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD
                                    : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE));
  loadtime_data->SetString("textPaiService",
                           l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_PAI));
  loadtime_data->SetString(
      "textGoogleServiceConfirmation",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_GOOGLE_SERVICE_CONFIRMATION));
  loadtime_data->SetString(
      "textLocationService",
      l10n_util::GetStringUTF16(is_child ? IDS_ARC_OPT_IN_LOCATION_SETTING_CHILD
                                         : IDS_ARC_OPT_IN_LOCATION_SETTING));
  loadtime_data->SetString(
      "serverError",
      l10n_util::GetStringUTF16(IDS_ARC_SERVER_COMMUNICATION_ERROR));
  loadtime_data->SetString(
      "controlledByPolicy",
      l10n_util::GetStringUTF16(IDS_CONTROLLED_SETTING_POLICY));
  loadtime_data->SetString(
      "learnMoreStatisticsTitle",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_TITLE));
  loadtime_data->SetString(
      "learnMoreStatistics",
      l10n_util::GetStringUTF16(is_child
                                    ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD
                                    : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS));
  loadtime_data->SetString(
      "learnMoreBackupAndRestoreTitle",
      l10n_util::GetStringUTF16(
          IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_TITLE));
  loadtime_data->SetString(
      "learnMoreBackupAndRestore",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_CHILD
                   : IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE));
  loadtime_data->SetString(
      "learnMoreLocationServicesTitle",
      l10n_util::GetStringUTF16(
          IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_TITLE));
  loadtime_data->SetString(
      "learnMoreLocationServices",
      l10n_util::GetStringUTF16(
          is_child ? IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_CHILD
                   : IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES));
  loadtime_data->SetString(
      "learnMorePaiServiceTitle",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_PAI_SERVICE_TITLE));
  loadtime_data->SetString(
      "learnMorePaiService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_PAI_SERVICE));
  loadtime_data->SetString(
      "overlayClose",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_CLOSE));
  loadtime_data->SetString(
      "privacyPolicyLink",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK));
  loadtime_data->SetString(
      "activeDirectoryAuthTitle",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_ACTIVE_DIRECTORY_AUTH_TITLE));
  loadtime_data->SetString(
      "activeDirectoryAuthDesc",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_ACTIVE_DIRECTORY_AUTH_DESC));
  loadtime_data->SetString(
      "overlayLoading", l10n_util::GetStringUTF16(IDS_ARC_POPUP_HELP_LOADING));

  loadtime_data->SetBoolean(kArcManaged, is_arc_managed_);
  loadtime_data->SetBoolean("isOwnerProfile",
                            chromeos::ProfileHelper::IsOwnerProfile(profile_));

  const std::string& country_code = base::CountryCodeForCurrentTimezone();
  loadtime_data->SetString("countryCode", country_code);

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, loadtime_data.get());
  loadtime_data->SetString("locale", app_locale);

  base::DictionaryValue message;
  message.SetString(kAction, kActionInitialize);
  message.Set(kData, std::move(loadtime_data));

  const std::string device_id = user_manager::known_user::GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(profile_));
  message.SetString(kDeviceId, device_id);

  message_host_->SendMessage(message);
  return true;
}

void ArcSupportHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  if (!message_host_)
    return;

  base::DictionaryValue message;
  message.SetString(kAction, kActionSetWindowBounds);
  message_host_->SendMessage(message);
}

void ArcSupportHost::OnMessage(const base::DictionaryValue& message) {
  std::string event;
  if (!message.GetString(kEvent, &event)) {
    NOTREACHED();
    return;
  }

  if (event == kEventOnWindowClosed) {
    // If ToS negotiation is ongoing, call the specific function.
    if (tos_delegate_) {
      tos_delegate_->OnTermsRejected();
    } else {
      DCHECK(error_delegate_);
      error_delegate_->OnWindowClosed();
    }
  } else if (event == kEventOnAuthSucceeded) {
    DCHECK(auth_delegate_);
    auth_delegate_->OnAuthSucceeded();
  } else if (event == kEventOnAuthFailed) {
    DCHECK(auth_delegate_);
    std::string error_message;
    if (!message.GetString(kAuthErrorMessage, &error_message)) {
      NOTREACHED();
      return;
    }
    // TODO(https://crbug.com/756144): Remove once reason for crash has been
    // determined.
    LOG_IF(ERROR, !auth_delegate_)
        << "auth_delegate_ is NULL, error: " << error_message;
    auth_delegate_->OnAuthFailed(error_message);
  } else if (event == kEventOnAgreed || event == kEventOnCanceled) {
    DCHECK(tos_delegate_);
    bool tos_shown;
    std::string tos_content;
    bool is_metrics_enabled;
    bool is_backup_restore_enabled;
    bool is_backup_restore_managed;
    bool is_location_service_enabled;
    bool is_location_service_managed;
    if (!message.GetString(kTosContent, &tos_content) ||
        !message.GetBoolean(kTosShown, &tos_shown) ||
        !message.GetBoolean(kIsMetricsEnabled, &is_metrics_enabled) ||
        !message.GetBoolean(kIsBackupRestoreEnabled,
                            &is_backup_restore_enabled) ||
        !message.GetBoolean(kIsBackupRestoreManaged,
                            &is_backup_restore_managed) ||
        !message.GetBoolean(kIsLocationServiceEnabled,
                            &is_location_service_enabled) ||
        !message.GetBoolean(kIsLocationServiceManaged,
                            &is_location_service_managed)) {
      NOTREACHED();
      return;
    }

    bool accepted = event == kEventOnAgreed;
    if (!accepted) {
      // Cancel is equivalent to not granting consent to the individual
      // features, so ensure we don't record consent.
      is_backup_restore_enabled = false;
      is_location_service_enabled = false;
    }

    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
    // This class doesn't care about browser sync consent.
    DCHECK(identity_manager->HasPrimaryAccount(
        signin::ConsentLevel::kNotRequired));
    CoreAccountId account_id = identity_manager->GetPrimaryAccountId(
        signin::ConsentLevel::kNotRequired);
    bool is_child = user_manager::UserManager::Get()->IsLoggedInAsChildUser();

    // Record acceptance of ToS if it was shown to the user, otherwise simply
    // record acceptance of an empty ToS.
    UserConsentTypes::ArcPlayTermsOfServiceConsent play_consent;
    play_consent.set_status(accepted ? UserConsentTypes::GIVEN
                                     : UserConsentTypes::NOT_GIVEN);
    play_consent.set_confirmation_grd_id(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
    play_consent.set_consent_flow(
        UserConsentTypes::ArcPlayTermsOfServiceConsent::SETUP);
    if (tos_shown) {
      play_consent.set_play_terms_of_service_text_length(tos_content.length());
      play_consent.set_play_terms_of_service_hash(
          base::SHA1HashString(tos_content));
    }
    ConsentAuditorFactory::GetForProfile(profile_)->RecordArcPlayConsent(
        account_id, play_consent);

    // If the user - not policy - controls Backup and Restore setting, record
    // whether consent was given.
    if (!is_backup_restore_managed) {
      UserConsentTypes::ArcBackupAndRestoreConsent backup_and_restore_consent;
      backup_and_restore_consent.set_confirmation_grd_id(
          IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
      backup_and_restore_consent.add_description_grd_ids(
          is_child ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD
                   : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
      backup_and_restore_consent.set_status(is_backup_restore_enabled
                                                ? UserConsentTypes::GIVEN
                                                : UserConsentTypes::NOT_GIVEN);

      ConsentAuditorFactory::GetForProfile(profile_)
          ->RecordArcBackupAndRestoreConsent(account_id,
                                             backup_and_restore_consent);
    }

    // If the user - not policy - controls Location Services setting, record
    // whether consent was given.
    if (!is_location_service_managed) {
      UserConsentTypes::ArcGoogleLocationServiceConsent
          location_service_consent;
      location_service_consent.set_confirmation_grd_id(
          IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
      location_service_consent.add_description_grd_ids(
          is_child ? IDS_ARC_OPT_IN_LOCATION_SETTING_CHILD
                   : IDS_ARC_OPT_IN_LOCATION_SETTING);
      location_service_consent.set_status(is_location_service_enabled
                                              ? UserConsentTypes::GIVEN
                                              : UserConsentTypes::NOT_GIVEN);

      ConsentAuditorFactory::GetForProfile(profile_)
          ->RecordArcGoogleLocationServiceConsent(account_id,
                                                  location_service_consent);
    }

    if (accepted) {
      tos_delegate_->OnTermsAgreed(is_metrics_enabled,
                                   is_backup_restore_enabled,
                                   is_location_service_enabled);
    }
  } else if (event == kEventOnRetryClicked) {
    // If ToS negotiation or manual authentication is ongoing, call the
    // corresponding delegate.  Otherwise, call the general retry function.
    if (tos_delegate_) {
      tos_delegate_->OnTermsRetryClicked();
    } else if (auth_delegate_) {
      auth_delegate_->OnAuthRetryClicked();
    } else {
      DCHECK(error_delegate_);
      error_delegate_->OnRetryClicked();
    }
  } else if (event == kEventOnSendFeedbackClicked) {
    DCHECK(error_delegate_);
    error_delegate_->OnSendFeedbackClicked();
  } else if (event == kEventOnOpenPrivacySettingsPageClicked) {
    chrome::ShowSettingsSubPageForProfile(profile_, chrome::kPrivacySubPage);
  } else {
    LOG(ERROR) << "Unknown message: " << event;
    NOTREACHED();
  }
}
