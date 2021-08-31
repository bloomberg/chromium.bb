// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

// Creates test data to be registered to the index. |input| is a map from
// id to contents (id and content).
std::vector<Data> CreateTestData(
    const std::map<std::string,
                   std::vector<std::pair<std::string, std::string>>>& input);

// Similar to above, but takes a weight for each item.
std::vector<Data> CreateTestData(
    const std::map<std::string,
                   std::vector<std::tuple<std::string, std::string, float>>>&
        input);

// Checks |result|'s id, score and number of matching positions are expected.
void CheckResult(const Result& result,
                 const std::string& expected_id,
                 float expected_score,
                 size_t expected_number_positions);

float TfIdfScore(size_t num_docs,
                 size_t num_docs_with_term,
                 float weighted_num_term_occurrence_in_doc,
                 size_t doc_length);

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
