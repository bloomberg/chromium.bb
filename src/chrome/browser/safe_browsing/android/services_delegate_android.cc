// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/services_delegate_android.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/telemetry/android/android_telemetry_service.h"
#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/db/v4_local_database_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::Create(
    SafeBrowsingService* safe_browsing_service) {
  return base::WrapUnique(new ServicesDelegateAndroid(safe_browsing_service));
}

// static
std::unique_ptr<ServicesDelegate> ServicesDelegate::CreateForTest(
    SafeBrowsingService* safe_browsing_service,
    ServicesDelegate::ServicesCreator* services_creator) {
  NOTREACHED();
  return base::WrapUnique(new ServicesDelegateAndroid(safe_browsing_service));
}

ServicesDelegateAndroid::ServicesDelegateAndroid(
    SafeBrowsingService* safe_browsing_service)
    : safe_browsing_service_(safe_browsing_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ServicesDelegateAndroid::~ServicesDelegateAndroid() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ServicesDelegateAndroid::InitializeCsdService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}

const scoped_refptr<SafeBrowsingDatabaseManager>&
ServicesDelegateAndroid::database_manager() const {
  return database_manager_;
}

void ServicesDelegateAndroid::Initialize() {
  if (!database_manager_set_for_tests_) {
#if defined(SAFE_BROWSING_DB_REMOTE)
    database_manager_ =
        base::WrapRefCounted(new RemoteSafeBrowsingDatabaseManager());
#else
    database_manager_ = V4LocalDatabaseManager::Create(
        SafeBrowsingService::GetBaseFilename(),
        base::BindRepeating(
            &ServicesDelegateAndroid::GetEstimatedExtendedReportingLevel,
            base::Unretained(this)));
#endif
  }
}

ExtendedReportingLevel
ServicesDelegateAndroid::GetEstimatedExtendedReportingLevel() const {
  return safe_browsing_service_->estimated_extended_reporting_by_prefs();
}

void ServicesDelegateAndroid::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager) {
  database_manager_set_for_tests_ = true;
  database_manager_ = database_manager;
}

void ServicesDelegateAndroid::ShutdownServices() {}

void ServicesDelegateAndroid::RefreshState(bool enable) {}

void ServicesDelegateAndroid::ProcessResourceRequest(
    const ResourceRequestInfo* request) {}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
ServicesDelegateAndroid::CreatePreferenceValidationDelegate(Profile* profile) {
  return nullptr;
}

void ServicesDelegateAndroid::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {}

void ServicesDelegateAndroid::AddDownloadManager(
    content::DownloadManager* download_manager) {}

ClientSideDetectionService* ServicesDelegateAndroid::GetCsdService() {
  return nullptr;
}

DownloadProtectionService* ServicesDelegateAndroid::GetDownloadService() {
  return nullptr;
}

void ServicesDelegateAndroid::StartOnIOThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& v4_config) {
  database_manager_->StartOnIOThread(url_loader_factory, v4_config);
}

void ServicesDelegateAndroid::StopOnIOThread(bool shutdown) {
  database_manager_->StopOnIOThread(shutdown);
}

void ServicesDelegateAndroid::CreatePasswordProtectionService(
    Profile* profile) {}
void ServicesDelegateAndroid::RemovePasswordProtectionService(
    Profile* profile) {}
PasswordProtectionService*
ServicesDelegateAndroid::GetPasswordProtectionService(Profile* profile) const {
  NOTIMPLEMENTED();
  return nullptr;
}

void ServicesDelegateAndroid::CreateTelemetryService(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);

  if (profile->IsOffTheRecord()) {
    return;
  }

  telemetry_service_ = std::make_unique<AndroidTelemetryService>(
      safe_browsing_service_, profile);
}

void ServicesDelegateAndroid::RemoveTelemetryService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  telemetry_service_.release();
}

TelemetryService* ServicesDelegateAndroid::GetTelemetryService() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return telemetry_service_.get();
}

std::string ServicesDelegateAndroid::GetSafetyNetId() const {
  return database_manager_->GetSafetyNetId();
}

}  // namespace safe_browsing
