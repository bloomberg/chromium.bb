// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/enterprise/signals/client_certificate_fetcher.h"
#include "chrome/browser/enterprise/signals/context_info_fetcher.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/statusor.h"
#endif

#include "extensions/browser/extension_function.h"

namespace extensions {

#if !BUILDFLAG(IS_CHROMEOS)
namespace enterprise_reporting {

extern const char kDeviceIdNotFound[];

}  // namespace enterprise_reporting


class EnterpriseReportingPrivateGetDeviceIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceId",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEID)

  EnterpriseReportingPrivateGetDeviceIdFunction();

  EnterpriseReportingPrivateGetDeviceIdFunction(
      const EnterpriseReportingPrivateGetDeviceIdFunction&) = delete;
  EnterpriseReportingPrivateGetDeviceIdFunction& operator=(
      const EnterpriseReportingPrivateGetDeviceIdFunction&) = delete;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  ~EnterpriseReportingPrivateGetDeviceIdFunction() override;
};

#if !BUILDFLAG(IS_LINUX)

class EnterpriseReportingPrivateGetPersistentSecretFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getPersistentSecret",
                             ENTERPRISEREPORTINGPRIVATE_GETPERSISTENTSECRET)

  EnterpriseReportingPrivateGetPersistentSecretFunction();
  EnterpriseReportingPrivateGetPersistentSecretFunction(
      const EnterpriseReportingPrivateGetPersistentSecretFunction&) = delete;
  EnterpriseReportingPrivateGetPersistentSecretFunction& operator=(
      const EnterpriseReportingPrivateGetPersistentSecretFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetPersistentSecretFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved from the file.
  void OnDataRetrieved(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const std::string& data,
                       int32_t status);

  void SendResponse(const std::string& data, int32_t status);
};

#endif  // !BUILDFLAG(IS_LINUX)

class EnterpriseReportingPrivateGetDeviceDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceData",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEDATA)

  EnterpriseReportingPrivateGetDeviceDataFunction();
  EnterpriseReportingPrivateGetDeviceDataFunction(
      const EnterpriseReportingPrivateGetDeviceDataFunction&) = delete;
  EnterpriseReportingPrivateGetDeviceDataFunction& operator=(
      const EnterpriseReportingPrivateGetDeviceDataFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetDeviceDataFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved from the file.
  void OnDataRetrieved(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const std::string& data,
                       RetrieveDeviceDataStatus status);

  void SendResponse(const std::string& data, RetrieveDeviceDataStatus status);
};

class EnterpriseReportingPrivateSetDeviceDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.setDeviceData",
                             ENTERPRISEREPORTINGPRIVATE_SETDEVICEDATA)

  EnterpriseReportingPrivateSetDeviceDataFunction();
  EnterpriseReportingPrivateSetDeviceDataFunction(
      const EnterpriseReportingPrivateSetDeviceDataFunction&) = delete;
  EnterpriseReportingPrivateSetDeviceDataFunction& operator=(
      const EnterpriseReportingPrivateSetDeviceDataFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateSetDeviceDataFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was stored to the file.
  void OnDataStored(scoped_refptr<base::SequencedTaskRunner> task_runner,
                    bool status);

  void SendResponse(bool status);
};

class EnterpriseReportingPrivateGetDeviceInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceInfo",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEINFO)

  EnterpriseReportingPrivateGetDeviceInfoFunction();
  EnterpriseReportingPrivateGetDeviceInfoFunction(
      const EnterpriseReportingPrivateGetDeviceInfoFunction&) = delete;
  EnterpriseReportingPrivateGetDeviceInfoFunction& operator=(
      const EnterpriseReportingPrivateGetDeviceInfoFunction&) = delete;

  // Conversion function for this class to use a DeviceInfoFetcher result.
  static api::enterprise_reporting_private::DeviceInfo ToDeviceInfo(
      const enterprise_signals::DeviceInfo& device_signals);

 private:
  ~EnterpriseReportingPrivateGetDeviceInfoFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved.
  void OnDeviceInfoRetrieved(const enterprise_signals::DeviceInfo& device_info);
};

#endif  // !BUILDFLAG(IS_CHROMEOS)

class EnterpriseReportingPrivateGetContextInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getContextInfo",
                             ENTERPRISEREPORTINGPRIVATE_GETCONTEXTINFO)

  EnterpriseReportingPrivateGetContextInfoFunction();
  EnterpriseReportingPrivateGetContextInfoFunction(
      const EnterpriseReportingPrivateGetContextInfoFunction&) = delete;
  EnterpriseReportingPrivateGetContextInfoFunction& operator=(
      const EnterpriseReportingPrivateGetContextInfoFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetContextInfoFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the context data is retrieved.
  void OnContextInfoRetrieved(enterprise_signals::ContextInfo context_info);

  std::unique_ptr<enterprise_signals::ContextInfoFetcher> context_info_fetcher_;
};

class EnterpriseReportingPrivateGetCertificateFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getCertificate",
                             ENTERPRISEREPORTINGPRIVATE_GETCERTIFICATE)

  EnterpriseReportingPrivateGetCertificateFunction();
  EnterpriseReportingPrivateGetCertificateFunction(
      const EnterpriseReportingPrivateGetCertificateFunction&) = delete;
  EnterpriseReportingPrivateGetCertificateFunction& operator=(
      const EnterpriseReportingPrivateGetCertificateFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetCertificateFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  // Callback invoked when |client_cert_fetcher_| is done fetching and selecting
  // the client certificate.
  void OnClientCertFetched(std::unique_ptr<net::ClientCertIdentity> cert);

  std::unique_ptr<enterprise_signals::ClientCertificateFetcher>
      client_cert_fetcher_;
};

#if BUILDFLAG(IS_CHROMEOS)

class EnterpriseReportingPrivateEnqueueRecordFunction
    : public ExtensionFunction {
 public:
  inline static constexpr char kErrorInvalidEnqueueRecordRequest[] =
      "Invalid request";
  inline static constexpr char kUnexpectedErrorEnqueueRecordRequest[] =
      "Encountered unexpected error while enqueuing record";
  inline static constexpr char kErrorProfileNotAffiliated[] =
      "Profile is not affiliated";
  inline static constexpr char kErrorCannotAssociateRecordWithUser[] =
      "Cannot associate record with user";

  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.enqueueRecord",
                             ENTERPRISEREPORTINGPRIVATE_ENQUEUERECORD)

  EnterpriseReportingPrivateEnqueueRecordFunction();
  EnterpriseReportingPrivateEnqueueRecordFunction(
      const EnterpriseReportingPrivateEnqueueRecordFunction&) = delete;
  EnterpriseReportingPrivateEnqueueRecordFunction& operator=(
      const EnterpriseReportingPrivateEnqueueRecordFunction&) = delete;

  void SetProfileIsAffiliatedForTesting(bool is_affiliated);

 private:
  ~EnterpriseReportingPrivateEnqueueRecordFunction() override;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;

  bool TryParseParams(
      std::unique_ptr<api::enterprise_reporting_private::EnqueueRecord::Params>
          params,
      ::reporting::Record& record,
      ::reporting::Priority& priority);

  bool TryAttachDMTokenToRecord(
      ::reporting::Record& record,
      api::enterprise_reporting_private::EventType event_type);

  // Callback invoked after the record was successfully enqueued
  void OnRecordEnqueued(::reporting::Status result);

  bool IsProfileAffiliated(Profile* profile);

  bool profile_is_affiliated_for_testing_ = false;
};

#endif

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_
