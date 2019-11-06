// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_metrics_service_client.h"

#include <jni.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_feature_list.h"
#include "android_webview/browser/aw_metrics_log_uploader.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/jni/AwMetricsServiceClient_jni.h"
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/hash/hash.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/metrics/version_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"

namespace android_webview {

base::LazyInstance<AwMetricsServiceClient>::Leaky g_lazy_instance_;

namespace {

const int kUploadIntervalMinutes = 30;

// A GUID in text form is composed of 32 hex digits and 4 hyphens.
const size_t kGuidSize = 32 + 4;
// The legacy file where WebView used to store the client ID, before it was
// moved to prefs.
const char* const kGuidFileName = "metrics_guid";

// Callbacks for metrics::MetricsStateManager::Create. Store/LoadClientInfo
// allow Windows Chrome to back up ClientInfo. They're no-ops for WebView.

void StoreClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadClientInfo() {
  std::unique_ptr<metrics::ClientInfo> client_info;
  return client_info;
}

// WebView metrics are sampled at 2%, based on the client ID. Since including
// app package names in WebView's metrics, as a matter of policy, the sample
// rate must not exceed 10%. Sampling is hard-coded (rather than controlled via
// variations, as in Chrome) because:
// - WebView is slow to download the variations seed and propagate it to each
//   app, so we'd miss metrics from the first few runs of each app.
// - WebView uses the low-entropy source for all studies, so there would be
//   crosstalk between the metrics sampling study and all other studies.
bool IsInSample(const std::string& client_id) {
  // client_id comes from base::GenerateGUID(), so its value is random/uniform,
  // except for a few bit positions with fixed values, and some hyphens. Rather
  // than separating the random payload from the fixed bits, just hash the whole
  // thing, to produce a new random/~uniform value.
  uint32_t hash = base::PersistentHash(client_id);

  // Since hashing is ~uniform, the chance that the value falls in the bottom
  // 2% (1/50th) of possible values is 2%.
  return hash < UINT32_MAX / 50u;
}

// Load the client ID from the legacy file, if any, store it in |id|, and then
// delete the file.
// TODO(crbug/939002): Remove this after ~all clients have migrated the ID.
void LoadLegacyClientId(std::unique_ptr<std::string>* id) {
  base::FilePath path;
  if (!internal::GetLegacyClientIdPath(&path))
    return;
  std::string contents;
  if (base::ReadFileToStringWithMaxSize(path, &contents, kGuidSize)) {
    if (base::IsValidGUID(contents))
      *id = std::make_unique<std::string>(std::move(contents));
  }
  base::DeleteFile(path, /*recursive=*/false);
}

std::unique_ptr<::metrics::MetricsService> CreateMetricsService(
    ::metrics::MetricsStateManager* state_manager,
    ::metrics::MetricsServiceClient* client,
    PrefService* prefs) {
  auto service =
      std::make_unique<::metrics::MetricsService>(state_manager, client, prefs);
  service->RegisterMetricsProvider(
      std::make_unique<::metrics::NetworkMetricsProvider>(
          content::CreateNetworkConnectionTrackerAsyncGetter()));
  service->RegisterMetricsProvider(
      std::make_unique<::metrics::GPUMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<::metrics::ScreenInfoMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<::metrics::CallStackProfileMetricsProvider>());
  service->InitializeMetricsRecordingState();
  return service;
}

}  // namespace

namespace internal {

// Get the path to the file where WebView used to store the client ID, before
// it was moved to prefs. Return true/false on success/failure.
// TODO(crbug/939002): Remove this after ~all clients have migrated the ID.
bool GetLegacyClientIdPath(base::FilePath* path) {
  base::FilePath dir;
  if (!base::PathService::Get(base::DIR_ANDROID_APP_DATA, &dir))
    return false;
  *path = dir.Append(FILE_PATH_LITERAL(kGuidFileName));
  return true;
}

}  // namespace internal

// static
AwMetricsServiceClient* AwMetricsServiceClient::GetInstance() {
  AwMetricsServiceClient* client = g_lazy_instance_.Pointer();
  DCHECK_CALLED_ON_VALID_SEQUENCE(client->sequence_checker_);
  return client;
}

AwMetricsServiceClient::AwMetricsServiceClient() {}
AwMetricsServiceClient::~AwMetricsServiceClient() {}

void AwMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_ == nullptr);  // Initialize should only happen once.
  DCHECK(!init_finished_);

  pref_service_ = pref_service;

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service_, this, base::string16(),
      base::BindRepeating(&StoreClientInfo),
      base::BindRepeating(&LoadClientInfo));

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadLegacyClientId, &legacy_client_id_),
      base::BindOnce(&AwMetricsServiceClient::InitializeWithClientId,
                     base::Unretained(this)));
}

void AwMetricsServiceClient::MaybeStartMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_finished_ && set_consent_finished_) {
    if (user_and_app_consent_) {
      metrics_service_ = CreateMetricsService(metrics_state_manager_.get(),
                                              this, pref_service_);
      metrics_state_manager_->ForceClientIdCreation();
      is_in_sample_ = IsInSample();
      if (IsReportingEnabled()) {
        // WebView has no shutdown sequence, so there's no need for a matching
        // Stop() call.
        metrics_service_->Start();
      }
    } else {
      pref_service_->ClearPref(metrics::prefs::kMetricsClientID);
    }
  }
}

void AwMetricsServiceClient::SetHaveMetricsConsent(bool consent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_consent_finished_ = true;
  user_and_app_consent_ = consent;
  MaybeStartMetrics();
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
AwMetricsServiceClient::CreateLowEntropyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metrics_state_manager_->CreateLowEntropyProvider();
}

bool AwMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_and_app_consent_;
}

bool AwMetricsServiceClient::IsReportingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return EnabledStateProvider::IsReportingEnabled() && is_in_sample_;
}

metrics::MetricsService* AwMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will be null if initialization hasn't finished, or if metrics
  // collection is disabled.
  return metrics_service_.get();
}

// In Chrome, UMA and Breakpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In WebView, UMA and Breakpad are
// independent, so this is a no-op.
void AwMetricsServiceClient::SetMetricsClientId(const std::string& client_id) {}

int32_t AwMetricsServiceClient::GetProduct() {
  return ::metrics::ChromeUserMetricsExtension::ANDROID_WEBVIEW;
}

std::string AwMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool AwMetricsServiceClient::GetBrand(std::string* brand_code) {
  // WebView doesn't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel AwMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(version_info::android::GetChannel());
}

std::string AwMetricsServiceClient::GetVersionString() {
  return version_info::GetVersionNumber();
}

void AwMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  done_callback.Run();
}

std::unique_ptr<metrics::MetricsLogUploader>
AwMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  // |server_url|, |insecure_server_url| and |mime_type| are unused because
  // WebView uses the platform logging mechanism instead of the normal UMA
  // server.
  return std::unique_ptr<::metrics::MetricsLogUploader>(
      new AwMetricsLogUploader(on_upload_complete));
}

base::TimeDelta AwMetricsServiceClient::GetStandardUploadInterval() {
  // The platform logging mechanism is responsible for upload frequency; this
  // just specifies how frequently to provide logs to the platform.
  return base::TimeDelta::FromMinutes(kUploadIntervalMinutes);
}

std::string AwMetricsServiceClient::GetAppPackageName() {
  if (!base::FeatureList::IsEnabled(features::kWebViewUmaLogAppPackageName))
    return std::string();

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_app_name =
      Java_AwMetricsServiceClient_getAppPackageName(env);
  if (j_app_name)
    return ConvertJavaStringToUTF8(env, j_app_name);
  return std::string();
}

void AwMetricsServiceClient::InitializeWithClientId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_finished_);

  if (legacy_client_id_) {
    pref_service_->SetString(metrics::prefs::kMetricsClientID,
                             *legacy_client_id_);
    legacy_client_id_.reset();
  }

  init_finished_ = true;
  MaybeStartMetrics();
}

bool AwMetricsServiceClient::IsInSample() {
  // Called in MaybeStartMetrics(), after metrics_service_ is created.
  return ::android_webview::IsInSample(metrics_service_->GetClientId());
}

// static
void JNI_AwMetricsServiceClient_SetHaveMetricsConsent(
    JNIEnv* env,
    jboolean consent) {
  AwMetricsServiceClient::GetInstance()->SetHaveMetricsConsent(consent);
}

}  // namespace android_webview
