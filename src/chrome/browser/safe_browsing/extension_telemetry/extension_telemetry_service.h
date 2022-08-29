// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/common/extension_id.h"

class Profile;
class PrefService;

namespace extensions {
class Extension;
class ExtensionPrefs;
class ExtensionRegistry;
}  // namespace extensions

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

enum class ExtensionSignalType;
class ExtensionSignal;
class ExtensionSignalProcessor;
class ExtensionTelemetryReportRequest;
class ExtensionTelemetryReportRequest_ExtensionInfo;
class ExtensionTelemetryUploader;
class ExtensionTelemetryPersister;
class SafeBrowsingTokenFetcher;

// This class process extension signals and reports telemetry for a given
// profile (regular profile only). It is used exclusively on the UI thread.
// Lifetime:
// The service is instantiated when the associated profile is instantiated. It
// is destructed when the corresponding profile is destructed.
// Enable/Disable state:
// The service is enabled/disabled based on kEnhancedSafeBrowsing. The service
// subscribes to the SB preference change notification to update its state.
// When enabled, the service receives and stores signal information. It also
// periodically creates telemetry reports and uploads them to the SB servers.
// When disabled, any previously stored signal information is cleared, incoming
// signals are ignored and no reports are sent to the SB servers.
class ExtensionTelemetryService : public KeyedService {
 public:
  ExtensionTelemetryService(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      extensions::ExtensionRegistry* extension_registry,
      extensions::ExtensionPrefs* extension_prefs);

  ExtensionTelemetryService(const ExtensionTelemetryService&) = delete;
  ExtensionTelemetryService& operator=(const ExtensionTelemetryService&) =
      delete;

  ~ExtensionTelemetryService() override;

  // Enables/disables the service.
  void SetEnabled(bool enable);
  bool enabled() const { return enabled_; }

  // Accepts extension telemetry signals for processing.
  void AddSignal(std::unique_ptr<ExtensionSignal> signal);

  base::TimeDelta current_reporting_interval() {
    return current_reporting_interval_;
  }

  // KeyedService:
  void Shutdown() override;

 private:
  // Called when prefs that affect extension telemetry service are changed.
  void OnPrefChanged();

  // Creates and uploads telemetry reports.
  void CreateAndUploadReport();

  void OnUploadComplete(bool success);

  // Returns a bool that represents if there is any signal processor
  // information to report.
  bool SignalDataPresent();

  // Creates telemetry report protobuf for all extension store extensions
  // and currently installed extensions along with signal data retrieved from
  // signal processors.
  std::unique_ptr<ExtensionTelemetryReportRequest> CreateReport();

  void DumpReportForTest(const ExtensionTelemetryReportRequest& report);

  // Collects extension information for reporting.
  std::unique_ptr<ExtensionTelemetryReportRequest_ExtensionInfo>
  GetExtensionInfoForReport(const extensions::Extension& extension);

  void UploadPersistedFile(std::string report);

  // Creates access token fetcher based on profile log-in status.
  // Returns nullptr when the user is not signed in.
  std::unique_ptr<SafeBrowsingTokenFetcher> GetTokenFetcher();

  // Called periodically based on the Telemetry Service timer. If time
  // elapsed since last upload is less than the reporting interval, persists
  // a new report, else uploads current report and any persisted reports.
  void PersistOrUploadData();

  // Uploads an extension telemetry report.
  void UploadReport(std::unique_ptr<std::string> report);

  // Checks the time of the last upload and if enough time has passed,
  // uploads telemetry data. Runs on a delayed post task on startup.
  void StartUploadCheck();

  // The persister object is bound to the threadpool. This prevents the
  // the read/write operations the `persister_` runs from blocking
  // the UI thread. It also allows the `persister_` object to be
  // destroyed cleanly while running tasks during Chrome shutdown.
  base::SequenceBound<ExtensionTelemetryPersister> persister_;

  // The profile with which this instance of the service is associated.
  const raw_ptr<Profile> profile_;

  // The URLLoaderFactory used to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // Observes changes to kSafeBrowsingEnhanced.
  PrefChangeRegistrar pref_change_registrar_;

  // Unowned objects used for getting extension information.
  const raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  const raw_ptr<extensions::ExtensionPrefs> extension_prefs_;

  // Keeps track of the state of the service.
  bool enabled_ = false;

  // Used for periodic collection of telemetry reports.
  base::RepeatingTimer timer_;
  base::TimeDelta current_reporting_interval_;

  // The current report being uploaded.
  std::unique_ptr<ExtensionTelemetryReportRequest> active_report_;
  // The current uploader instance uploading the active report.
  std::unique_ptr<ExtensionTelemetryUploader> active_uploader_;

  // Maps extension id to extension data.
  using ExtensionStore = base::flat_map<
      extensions::ExtensionId,
      std::unique_ptr<ExtensionTelemetryReportRequest_ExtensionInfo>>;
  ExtensionStore extension_store_;

  using SignalProcessors =
      base::flat_map<ExtensionSignalType,
                     std::unique_ptr<ExtensionSignalProcessor>>;
  SignalProcessors signal_processors_;

  friend class ExtensionTelemetryServiceTest;
  friend class ExtensionTelemetryServiceBrowserTest;

  base::WeakPtrFactory<ExtensionTelemetryService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_SERVICE_H_
