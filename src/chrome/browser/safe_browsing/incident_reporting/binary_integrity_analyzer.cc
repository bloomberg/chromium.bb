// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/proto/csd.pb.h"

namespace safe_browsing {

void RecordSignatureVerificationTime(size_t file_index,
                                     const base::TimeDelta& verification_time) {
  static const char kHistogramName[] = "SBIRS.VerifyBinaryIntegrity.";

  base::HistogramBase* signature_verification_time_histogram =
      base::Histogram::FactoryTimeGet(
          std::string(kHistogramName) + base::NumberToString(file_index),
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromSeconds(20), 50,
          base::Histogram::kUmaTargetedHistogramFlag);

  signature_verification_time_histogram->AddTime(verification_time);
}

void ClearBinaryIntegrityForFile(IncidentReceiver* incident_receiver,
                                 const std::string& basename) {
  std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
      incident(new ClientIncidentReport_IncidentData_BinaryIntegrityIncident());
  incident->set_file_basename(basename);
  incident_receiver->ClearIncidentForProcess(
      std::make_unique<BinaryIntegrityIncident>(std::move(incident)));
}

void RegisterBinaryIntegrityAnalysis() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::Bind(&VerifyBinaryIntegrity));
#endif
}

}  // namespace safe_browsing
