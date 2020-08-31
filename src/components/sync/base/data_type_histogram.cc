// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type_histogram.h"

#include "base/metrics/histogram_functions.h"

namespace syncer {

namespace {
const char kModelTypeMemoryHistogramPrefix[] = "Sync.ModelTypeMemoryKB.";
const char kModelTypeCountHistogramPrefix[] = "Sync.ModelTypeCount4.";
const char kModelTypeUpdateDropHistogramPrefix[] = "Sync.ModelTypeUpdateDrop.";

std::string GetHistogramSuffixForUpdateDropReason(UpdateDropReason reason) {
  switch (reason) {
    case UpdateDropReason::kInconsistentClientTag:
      return "InconsistentClientTag";
    case UpdateDropReason::kCannotGenerateStorageKey:
      return "CannotGenerateStorageKey";
    case UpdateDropReason::kTombstoneInFullUpdate:
      return "TombstoneInFullUpdate";
    case UpdateDropReason::kTombstoneForNonexistentInIncrementalUpdate:
      return "TombstoneForNonexistentInIncrementalUpdate";
    case UpdateDropReason::kDecryptionPending:
      return "DecryptionPending";
    case UpdateDropReason::kFailedToDecrypt:
      return "FailedToDecrypt";
  }
}

}  // namespace

void SyncRecordModelTypeUpdateDropReason(UpdateDropReason reason,
                                         ModelType type) {
  std::string full_histogram_name =
      kModelTypeUpdateDropHistogramPrefix +
      GetHistogramSuffixForUpdateDropReason(reason);
  base::UmaHistogramEnumeration(full_histogram_name,
                                ModelTypeHistogramValue(type));
}

void SyncRecordModelTypeMemoryHistogram(ModelType model_type, size_t bytes) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
  std::string full_histogram_name =
      kModelTypeMemoryHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, bytes / 1024);
}

void SyncRecordModelTypeCountHistogram(ModelType model_type, size_t count) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
  std::string full_histogram_name =
      kModelTypeCountHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, count);
}

}  // namespace syncer
