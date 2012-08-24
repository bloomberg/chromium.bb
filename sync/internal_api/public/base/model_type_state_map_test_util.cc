// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_state_map_test_util.h"

#include <algorithm>

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type_test_util.h"

namespace syncer {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace {

class ModelTypeStateMapEqMatcher
    : public MatcherInterface<const ModelTypeStateMap&> {
 public:
  explicit ModelTypeStateMapEqMatcher(const ModelTypeStateMap& expected);

  virtual bool MatchAndExplain(const ModelTypeStateMap& lhs,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const ModelTypeStateMap expected_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeStateMapEqMatcher);
};

ModelTypeStateMapEqMatcher::ModelTypeStateMapEqMatcher(
    const ModelTypeStateMap& expected) : expected_(expected) {
}

bool ModelTypeStateMapEqMatcher::MatchAndExplain(
    const ModelTypeStateMap& actual, MatchResultListener* listener) const {
  ModelTypeStateMap expected_only;
  ModelTypeStateMap actual_only;
  typedef std::pair<ModelType,
                    std::pair<InvalidationState, InvalidationState> >
      ValueDifference;
  std::vector<ValueDifference> value_differences;

  std::set_difference(expected_.begin(), expected_.end(),
                      actual.begin(), actual.end(),
                      std::inserter(expected_only, expected_only.begin()),
                      expected_.value_comp());
  std::set_difference(actual.begin(), actual.end(),
                      expected_.begin(), expected_.end(),
                      std::inserter(actual_only, actual_only.begin()),
                      actual.value_comp());

  for (ModelTypeStateMap::const_iterator it = expected_.begin();
       it != expected_.end(); ++it) {
    ModelTypeStateMap::const_iterator find_it =
        actual.find(it->first);
    if (find_it != actual.end() &&
        !Matches(Eq(it->second))(find_it->second)) {
      value_differences.push_back(std::make_pair(
          it->first, std::make_pair(it->second, find_it->second)));
    }
  }

  if (expected_only.empty() && actual_only.empty() && value_differences.empty())
    return true;

  bool printed_header = false;
  if (!actual_only.empty()) {
    *listener << " which has these unexpected elements: "
              << PrintToString(actual_only);
    printed_header = true;
  }

  if (!expected_only.empty()) {
    *listener << (printed_header ? ",\nand" : "which")
              << " doesn't have these expected elements: "
              << PrintToString(expected_only);
    printed_header = true;
  }

  if (!value_differences.empty()) {
    *listener << (printed_header ? ",\nand" : "which")
              << " differ in the following values: "
              << PrintToString(value_differences);
  }

  return false;
}

void ModelTypeStateMapEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void ModelTypeStateMapEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

Matcher<const ModelTypeStateMap&> Eq(const ModelTypeStateMap& expected) {
  return MakeMatcher(new ModelTypeStateMapEqMatcher(expected));
}

}  // namespace syncer
