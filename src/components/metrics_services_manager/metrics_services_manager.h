// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
#define COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_

#include <memory>

#include "base/metrics/field_trial.h"
#include "base/threading/thread_checker.h"

namespace base {
class FilePath;
}

namespace metrics {
class MetricsService;
class MetricsServiceClient;
}

namespace ukm {
class UkmService;
}

namespace variations {
class VariationsService;
}

namespace metrics_services_manager {

class MetricsServicesManagerClient;

// MetricsServicesManager is a helper class for embedders that use the various
// metrics-related services in a Chrome-like fashion: MetricsService (via its
// client) and VariationsService.
class MetricsServicesManager {
 public:
  // Creates the MetricsServicesManager with the given client.
  explicit MetricsServicesManager(
      std::unique_ptr<MetricsServicesManagerClient> client);

  MetricsServicesManager(const MetricsServicesManager&) = delete;
  MetricsServicesManager& operator=(const MetricsServicesManager&) = delete;

  virtual ~MetricsServicesManager();

  // Instantiates the FieldTrialList using Chrome's default entropy provider.
  // Uses |enable_gpu_benchmarking_switch| to set up the FieldTrialList for
  // benchmarking runs.
  //
  // Side effect: Initializes the CleanExitBeacon.
  void InstantiateFieldTrialList(
      const char* enable_gpu_benchmarking_switch = nullptr) const;

  // Returns the MetricsService, creating it if it hasn't been created yet (and
  // additionally creating the MetricsServiceClient in that case).
  metrics::MetricsService* GetMetricsService();

  // Returns the UkmService, creating it if it hasn't been created yet.
  ukm::UkmService* GetUkmService();

  // Returns the VariationsService, creating it if it hasn't been created yet.
  variations::VariationsService* GetVariationsService();

  // Called when loading state changed.
  void LoadingStateChanged(bool is_loading);

  // Should be called when a plugin loading error occurs.
  void OnPluginLoadingError(const base::FilePath& plugin_path);

  // Update the managed services when permissions for uploading metrics change.
  void UpdateUploadPermissions(bool may_upload);

  // Gets the current state of metric reporting.
  bool IsMetricsReportingEnabled() const;

  // Gets the current state of metrics consent.
  bool IsMetricsConsentGiven() const;

  // Returns the default entropy provider.
  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateEntropyProviderForTesting();

 private:
  // Returns the MetricsServiceClient, creating it if it hasn't been
  // created yet (and additionally creating the MetricsService in that case).
  metrics::MetricsServiceClient* GetMetricsServiceClient();

  // Update which services are running to match current permissions.
  void UpdateRunningServices();

  // Update the state of UkmService to match current permissions.
  void UpdateUkmService();

  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdatePermissions(bool current_may_record,
                         bool current_consent_given,
                         bool current_may_upload);

  // The client passed in from the embedder.
  const std::unique_ptr<MetricsServicesManagerClient> client_;

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // The current metrics reporting setting.
  bool may_upload_;

  // The current metrics recording setting.
  bool may_record_;

  // The current metrics setting reflecting if consent was given.
  bool consent_given_;

  // The MetricsServiceClient. Owns the MetricsService.
  std::unique_ptr<metrics::MetricsServiceClient> metrics_service_client_;

  // The VariationsService, for server-side experiments infrastructure.
  std::unique_ptr<variations::VariationsService> variations_service_;
};

}  // namespace metrics_services_manager

#endif  // COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
