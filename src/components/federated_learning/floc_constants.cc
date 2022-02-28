// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_constants.h"

#include <limits>

namespace federated_learning {

// This is only for experimentation and won't be served to websites.
const uint8_t kMaxNumberOfBitsInFloc = 50;
static_assert(kMaxNumberOfBitsInFloc > 0 &&
                  kMaxNumberOfBitsInFloc <=
                      std::numeric_limits<uint64_t>::digits,
              "Number of bits in the floc id must be greater than 0 and no "
              "greater than 64.");

const char kFlocIdValuePrefKey[] = "federated_learning.floc_id.value";

const char kFlocIdStatusPrefKey[] = "federated_learning.floc_id.status";

const char kFlocIdHistoryBeginTimePrefKey[] =
    "federated_learning.floc_id.history_begin_time";

const char kFlocIdHistoryEndTimePrefKey[] =
    "federated_learning.floc_id.history_end_time";

const char kFlocIdFinchConfigVersionPrefKey[] =
    "federated_learning.floc_id.finch_config_version";

const char kFlocIdSortingLshVersionPrefKey[] =
    "federated_learning.floc_id.sorting_lsh_version";

const char kFlocIdComputeTimePrefKey[] =
    "federated_learning.floc_id.compute_time";

const char kManifestFlocComponentFormatKey[] = "floc_component_format";

const int kCurrentFlocComponentFormatVersion = 3;

const uint8_t kSortingLshMaxBits = 7;

const uint32_t kSortingLshBlockedMask = 0b1000000;

const uint32_t kSortingLshSizeMask = 0b0111111;

const base::FilePath::CharType kTopLevelDirectoryName[] =
    FILE_PATH_LITERAL("Floc");

const base::FilePath::CharType kSortingLshClustersFileName[] =
    FILE_PATH_LITERAL("SortingLshClusters");

}  // namespace federated_learning
