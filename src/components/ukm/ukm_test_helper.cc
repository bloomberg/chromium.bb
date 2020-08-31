// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_test_helper.h"

#include <algorithm>
#include <string>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "components/metrics/unsent_log_store.h"
#include "third_party/zlib/google/compression_utils.h"

namespace ukm {

UkmTestHelper::UkmTestHelper(UkmService* ukm_service)
    : ukm_service_(ukm_service) {}

bool UkmTestHelper::IsExtensionRecordingEnabled() const {
  return ukm_service_ ? ukm_service_->extensions_enabled_ : false;
}

bool UkmTestHelper::IsRecordingEnabled() const {
  return ukm_service_ ? ukm_service_->recording_enabled_ : false;
}

bool UkmTestHelper::IsReportUserNoisedUserBirthYearAndGenderEnabled() {
  return base::FeatureList::IsEnabled(
      ukm::UkmService::kReportUserNoisedUserBirthYearAndGender);
}

uint64_t UkmTestHelper::GetClientId() {
  return ukm_service_->client_id_;
}

std::unique_ptr<Report> UkmTestHelper::GetUkmReport() {
  if (!HasUnsentLogs())
    return nullptr;

  metrics::UnsentLogStore* log_store =
      ukm_service_->reporting_service_.ukm_log_store();
  if (log_store->has_staged_log()) {
    // For testing purposes, we examine the content of a staged log without
    // ever sending the log, so discard any previously staged log.
    log_store->DiscardStagedLog();
  }

  log_store->StageNextLog();
  if (!log_store->has_staged_log())
    return nullptr;

  std::string uncompressed_log_data;
  if (!compression::GzipUncompress(log_store->staged_log(),
                                   &uncompressed_log_data))
    return nullptr;

  std::unique_ptr<ukm::Report> report = std::make_unique<ukm::Report>();
  if (!report->ParseFromString(uncompressed_log_data))
    return nullptr;

  return report;
}

UkmSource* UkmTestHelper::GetSource(SourceId source_id) {
  if (!ukm_service_)
    return nullptr;

  auto it = ukm_service_->sources().find(source_id);
  return it == ukm_service_->sources().end() ? nullptr : it->second.get();
}

bool UkmTestHelper::HasSource(SourceId source_id) {
  return ukm_service_ && base::Contains(ukm_service_->sources(), source_id);
}

bool UkmTestHelper::IsSourceObsolete(SourceId source_id) {
  return ukm_service_ &&
         base::Contains(ukm_service_->recordings_.obsolete_source_ids,
                        source_id);
}

void UkmTestHelper::RecordSourceForTesting(SourceId source_id) {
  if (ukm_service_)
    ukm_service_->UpdateSourceURL(source_id, GURL("http://example.com"));
}

void UkmTestHelper::BuildAndStoreLog() {
  // Wait for initialization to complete before flushing.
  base::RunLoop run_loop;
  ukm_service_->SetInitializationCompleteCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  ukm_service_->Flush();
}

bool UkmTestHelper::HasUnsentLogs() {
  return ukm_service_ &&
         ukm_service_->reporting_service_.ukm_log_store()->has_unsent_logs();
}

}  // namespace ukm
