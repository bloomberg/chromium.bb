// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "net/cert/x509_util.h"

namespace extensions {

namespace {
#if !defined(OS_CHROMEOS)
const char kEndpointVerificationRetrievalFailed[] =
    "Failed to retrieve the endpoint verification data.";
const char kEndpointVerificationStoreFailed[] =
    "Failed to store the endpoint verification data.";
#endif  // !defined(OS_CHROMEOS)

api::enterprise_reporting_private::SettingValue ToInfoSettingValue(
    enterprise_signals::SettingValue value) {
  switch (value) {
    case enterprise_signals::SettingValue::NONE:
      return api::enterprise_reporting_private::SETTING_VALUE_NONE;
    case enterprise_signals::SettingValue::UNKNOWN:
      return api::enterprise_reporting_private::SETTING_VALUE_UNKNOWN;
    case enterprise_signals::SettingValue::DISABLED:
      return api::enterprise_reporting_private::SETTING_VALUE_DISABLED;
    case enterprise_signals::SettingValue::ENABLED:
      return api::enterprise_reporting_private::SETTING_VALUE_ENABLED;
  }
}

api::enterprise_reporting_private::ContextInfo ToContextInfo(
    enterprise_signals::ContextInfo&& signals) {
  api::enterprise_reporting_private::ContextInfo info;

  info.browser_affiliation_ids = std::move(signals.browser_affiliation_ids);
  info.profile_affiliation_ids = std::move(signals.profile_affiliation_ids);
  info.on_file_attached_providers =
      std::move(signals.on_file_attached_providers);
  info.on_file_downloaded_providers =
      std::move(signals.on_file_downloaded_providers);
  info.on_bulk_data_entry_providers =
      std::move(signals.on_bulk_data_entry_providers);
  info.on_security_event_providers =
      std::move(signals.on_security_event_providers);
  info.site_isolation_enabled = signals.site_isolation_enabled;
  info.chrome_cleanup_enabled =
      signals.chrome_cleanup_enabled.has_value()
          ? std::make_unique<bool>(signals.chrome_cleanup_enabled.value())
          : nullptr;
  info.chrome_remote_desktop_app_blocked =
      signals.chrome_remote_desktop_app_blocked;
  info.third_party_blocking_enabled =
      signals.third_party_blocking_enabled.has_value()
          ? std::make_unique<bool>(signals.third_party_blocking_enabled.value())
          : nullptr;
  info.os_firewall = ToInfoSettingValue(signals.os_firewall);
  info.system_dns_servers = std::move(signals.system_dns_servers);
  switch (signals.realtime_url_check_mode) {
    case safe_browsing::REAL_TIME_CHECK_DISABLED:
      info.realtime_url_check_mode = extensions::api::
          enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED;
      break;
    case safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED:
      info.realtime_url_check_mode =
          extensions::api::enterprise_reporting_private::
              REALTIME_URL_CHECK_MODE_ENABLED_MAIN_FRAME;
      break;
  }
  info.browser_version = std::move(signals.browser_version);
  info.built_in_dns_client_enabled = signals.built_in_dns_client_enabled;

  switch (signals.safe_browsing_protection_level) {
    case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      info.safe_browsing_protection_level = extensions::api::
          enterprise_reporting_private::SAFE_BROWSING_LEVEL_DISABLED;
      break;
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      info.safe_browsing_protection_level = extensions::api::
          enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD;
      break;
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      info.safe_browsing_protection_level = extensions::api::
          enterprise_reporting_private::SAFE_BROWSING_LEVEL_ENHANCED;
      break;
  }
  if (!signals.password_protection_warning_trigger.has_value()) {
    info.password_protection_warning_trigger = extensions::api::
        enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET;
  } else {
    switch (signals.password_protection_warning_trigger.value()) {
      case safe_browsing::PASSWORD_PROTECTION_OFF:
        info.password_protection_warning_trigger =
            extensions::api::enterprise_reporting_private::
                PASSWORD_PROTECTION_TRIGGER_PASSWORD_PROTECTION_OFF;
        break;
      case safe_browsing::PASSWORD_REUSE:
        info.password_protection_warning_trigger =
            extensions::api::enterprise_reporting_private::
                PASSWORD_PROTECTION_TRIGGER_PASSWORD_REUSE;
        break;
      case safe_browsing::PHISHING_REUSE:
        info.password_protection_warning_trigger =
            extensions::api::enterprise_reporting_private::
                PASSWORD_PROTECTION_TRIGGER_PHISHING_REUSE;
        break;
      case safe_browsing::PASSWORD_PROTECTION_TRIGGER_MAX:
        NOTREACHED();
        break;
    }
  }

  return info;
}

}  // namespace

#if !defined(OS_CHROMEOS)
namespace enterprise_reporting {
const char kDeviceIdNotFound[] = "Failed to retrieve the device id.";
}  // namespace enterprise_reporting

// GetDeviceId

EnterpriseReportingPrivateGetDeviceIdFunction::
    EnterpriseReportingPrivateGetDeviceIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceIdFunction::Run() {
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
  if (client_id.empty())
    return RespondNow(Error(enterprise_reporting::kDeviceIdNotFound));
  return RespondNow(OneArgument(base::Value(client_id)));
}

EnterpriseReportingPrivateGetDeviceIdFunction::
    ~EnterpriseReportingPrivateGetDeviceIdFunction() = default;

// getPersistentSecret

#if !defined(OS_LINUX)

EnterpriseReportingPrivateGetPersistentSecretFunction::
    EnterpriseReportingPrivateGetPersistentSecretFunction() = default;
EnterpriseReportingPrivateGetPersistentSecretFunction::
    ~EnterpriseReportingPrivateGetPersistentSecretFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetPersistentSecretFunction::Run() {
  std::unique_ptr<
      api::enterprise_reporting_private::GetPersistentSecret::Params>
      params(api::enterprise_reporting_private::GetPersistentSecret::Params::
                 Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool force_create = params->reset_secret ? *params->reset_secret : false;
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceSecret, force_create,
          base::BindOnce(
              &EnterpriseReportingPrivateGetPersistentSecretFunction::
                  OnDataRetrieved,
              this, base::ThreadTaskRunnerHandle::Get())));
  return RespondLater();
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::OnDataRetrieved(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& data,
    long int status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateGetPersistentSecretFunction::SendResponse,
          this, data, status));
}

void EnterpriseReportingPrivateGetPersistentSecretFunction::SendResponse(
    const std::string& data,
    long int status) {
  if (status == 0) {  // Success.
    VLOG(1) << "The Endpoint Verification secret was retrieved.";
    Respond(OneArgument(base::Value(base::Value::BlobStorage(
        reinterpret_cast<const uint8_t*>(data.data()),
        reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
  } else {
    VLOG(1) << "Endpoint Verification secret retrieval error: " << status;
    Respond(Error(base::StringPrintf("%ld", static_cast<long int>(status))));
  }
}

#endif  // !defined(OS_LINUX)

// getDeviceData

EnterpriseReportingPrivateGetDeviceDataFunction::
    EnterpriseReportingPrivateGetDeviceDataFunction() = default;
EnterpriseReportingPrivateGetDeviceDataFunction::
    ~EnterpriseReportingPrivateGetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceDataFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::GetDeviceData::Params>
      params(api::enterprise_reporting_private::GetDeviceData::Params::Create(
          args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &RetrieveDeviceData, params->id,
          base::BindOnce(
              &EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved,
              this, base::ThreadTaskRunnerHandle::Get())));
  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceDataFunction::OnDataRetrieved(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::string& data,
    RetrieveDeviceDataStatus status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateGetDeviceDataFunction::SendResponse, this,
          data, status));
}

void EnterpriseReportingPrivateGetDeviceDataFunction::SendResponse(
    const std::string& data,
    RetrieveDeviceDataStatus status) {
  switch (status) {
    case RetrieveDeviceDataStatus::kSuccess:
      VLOG(1) << "The Endpoint Verification data was retrieved.";
      Respond(OneArgument(base::Value(base::Value::BlobStorage(
          reinterpret_cast<const uint8_t*>(data.data()),
          reinterpret_cast<const uint8_t*>(data.data() + data.size())))));
      return;
    case RetrieveDeviceDataStatus::kDataRecordNotFound:
      VLOG(1) << "The Endpoint Verification data is not present.";
      Respond(OneArgument(base::Value(base::Value::BlobStorage())));
      return;
    default:
      VLOG(1) << "Endpoint Verification data retrieval error: "
              << static_cast<long int>(status);
      Respond(Error(kEndpointVerificationRetrievalFailed));
  }
}

// setDeviceData

EnterpriseReportingPrivateSetDeviceDataFunction::
    EnterpriseReportingPrivateSetDeviceDataFunction() = default;
EnterpriseReportingPrivateSetDeviceDataFunction::
    ~EnterpriseReportingPrivateSetDeviceDataFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateSetDeviceDataFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::SetDeviceData::Params>
      params(api::enterprise_reporting_private::SetDeviceData::Params::Create(
          args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &StoreDeviceData, params->id, std::move(params->data),
          base::BindOnce(
              &EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored,
              this, base::ThreadTaskRunnerHandle::Get())));
  return RespondLater();
}

void EnterpriseReportingPrivateSetDeviceDataFunction::OnDataStored(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool status) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EnterpriseReportingPrivateSetDeviceDataFunction::SendResponse, this,
          status));
}

void EnterpriseReportingPrivateSetDeviceDataFunction::SendResponse(
    bool status) {
  if (status) {
    VLOG(1) << "The Endpoint Verification data was stored.";
    Respond(NoArguments());
  } else {
    VLOG(1) << "Endpoint Verification data storage error.";
    Respond(Error(kEndpointVerificationStoreFailed));
  }
}

// getDeviceInfo

EnterpriseReportingPrivateGetDeviceInfoFunction::
    EnterpriseReportingPrivateGetDeviceInfoFunction() = default;
EnterpriseReportingPrivateGetDeviceInfoFunction::
    ~EnterpriseReportingPrivateGetDeviceInfoFunction() = default;

// static
api::enterprise_reporting_private::DeviceInfo
EnterpriseReportingPrivateGetDeviceInfoFunction::ToDeviceInfo(
    enterprise_signals::DeviceInfo&& device_signals) {
  api::enterprise_reporting_private::DeviceInfo device_info;

  device_info.os_name = std::move(device_signals.os_name);
  device_info.os_version = std::move(device_signals.os_version);
  device_info.security_patch_level =
      std::move(device_signals.security_patch_level);
  device_info.device_host_name = std::move(device_signals.device_host_name);
  device_info.device_model = std::move(device_signals.device_model);
  device_info.serial_number = std::move(device_signals.serial_number);
  device_info.screen_lock_secured =
      ToInfoSettingValue(device_signals.screen_lock_secured);
  device_info.disk_encrypted =
      ToInfoSettingValue(device_signals.disk_encrypted);
  device_info.mac_addresses = std::move(device_signals.mac_addresses);
  if (device_signals.windows_machine_domain.has_value()) {
    device_info.windows_machine_domain = std::make_unique<std::string>(
        device_signals.windows_machine_domain.value());
  } else {
    device_info.windows_machine_domain = nullptr;
  }
  if (device_signals.windows_user_domain.has_value()) {
    device_info.windows_user_domain = std::make_unique<std::string>(
        device_signals.windows_user_domain.value());
  } else {
    device_info.windows_user_domain = nullptr;
  }

  return device_info;
}

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetDeviceInfoFunction::Run() {
#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateCOMSTATaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&enterprise_signals::DeviceInfoFetcher::Fetch,
                     enterprise_signals::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#else
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateTaskRunner({base::MayBlock()}).get(), FROM_HERE,
      base::BindOnce(&enterprise_signals::DeviceInfoFetcher::Fetch,
                     enterprise_signals::DeviceInfoFetcher::CreateInstance()),
      base::BindOnce(&EnterpriseReportingPrivateGetDeviceInfoFunction::
                         OnDeviceInfoRetrieved,
                     this));
#endif  // defined(OS_WIN)

  return RespondLater();
}

void EnterpriseReportingPrivateGetDeviceInfoFunction::OnDeviceInfoRetrieved(
    enterprise_signals::DeviceInfo device_signals) {
  Respond(OneArgument(base::Value::FromUniquePtrValue(
      ToDeviceInfo(std::move(device_signals)).ToValue())));
}

#endif  // !defined(OS_CHROMEOS)

// getContextInfo

EnterpriseReportingPrivateGetContextInfoFunction::
    EnterpriseReportingPrivateGetContextInfoFunction() = default;
EnterpriseReportingPrivateGetContextInfoFunction::
    ~EnterpriseReportingPrivateGetContextInfoFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetContextInfoFunction::Run() {
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetInstance()
          ->GetForBrowserContext(browser_context());
  DCHECK(connectors_service);

  context_info_fetcher_ =
      enterprise_signals::ContextInfoFetcher::CreateInstance(
          browser_context(), connectors_service);
  context_info_fetcher_->Fetch(base::BindOnce(
      &EnterpriseReportingPrivateGetContextInfoFunction::OnContextInfoRetrieved,
      this));

  return RespondLater();
}

void EnterpriseReportingPrivateGetContextInfoFunction::OnContextInfoRetrieved(
    enterprise_signals::ContextInfo context_info) {
  Respond(OneArgument(base::Value::FromUniquePtrValue(
      ToContextInfo(std::move(context_info)).ToValue())));
}

// getCertificate

EnterpriseReportingPrivateGetCertificateFunction::
    EnterpriseReportingPrivateGetCertificateFunction() = default;
EnterpriseReportingPrivateGetCertificateFunction::
    ~EnterpriseReportingPrivateGetCertificateFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseReportingPrivateGetCertificateFunction::Run() {
  std::unique_ptr<api::enterprise_reporting_private::GetCertificate::Params>
      params(api::enterprise_reporting_private::GetCertificate::Params::Create(
          args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // If AutoSelectCertificateForUrl is not set at the machine level, this
  // operation is not supported and should return immediately with the
  // appropriate status field value.
  if (!chrome::enterprise_util::IsMachinePolicyPref(
          prefs::kManagedAutoSelectCertificateForUrls)) {
    api::enterprise_reporting_private::Certificate ret;
    ret.status = extensions::api::enterprise_reporting_private::
        CERTIFICATE_STATUS_POLICY_UNSET;
    return RespondNow(
        OneArgument(base::Value::FromUniquePtrValue(ret.ToValue())));
  }

  client_cert_fetcher_ =
      enterprise_signals::ClientCertificateFetcher::Create(browser_context());
  client_cert_fetcher_->FetchAutoSelectedCertificateForUrl(
      GURL(params->url),
      base::BindOnce(&EnterpriseReportingPrivateGetCertificateFunction::
                         OnClientCertFetched,
                     this));

  return RespondLater();
}

void EnterpriseReportingPrivateGetCertificateFunction::OnClientCertFetched(
    std::unique_ptr<net::ClientCertIdentity> cert) {
  api::enterprise_reporting_private::Certificate ret;

  // Getting here means the status is always OK, but the |encoded_certificate|
  // field is only set if there actually was a certificate selected.
  ret.status =
      extensions::api::enterprise_reporting_private::CERTIFICATE_STATUS_OK;
  if (cert) {
    base::StringPiece der_cert = net::x509_util::CryptoBufferAsStringPiece(
        cert->certificate()->cert_buffer());
    ret.encoded_certificate = std::make_unique<std::vector<uint8_t>>(
        der_cert.begin(), der_cert.end());
  }

  Respond(OneArgument(base::Value::FromUniquePtrValue(ret.ToValue())));
}

}  // namespace extensions
