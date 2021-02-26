// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_UTILS_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_UTILS_H_

#include "base/strings/string16.h"

namespace chromeos {
namespace local_search_service {

struct Result;

// Score is non-zero if |query| is a prefix of |text|. No case normalization is
// done.
float ExactPrefixMatchScore(const base::string16& query,
                            const base::string16& text);

// Returns block matching ratio between |query| and |text|. No case
// normalization is done.
float BlockMatchScore(const base::string16& query, const base::string16& text);

// |query| approximately matches |text| if its prefix score is above the
// |prefix_threshold| or block matching score is above |block_threshold|.
bool IsRelevantApproximately(const base::string16& query,
                             const base::string16& text,
                             float prefix_threshold,
                             float block_threshold);

// Returns whether |r1| score is higher than |r2|'s.
bool CompareResults(const Result& r1, const Result& r2);

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_SEARCH_UTILS_H_
