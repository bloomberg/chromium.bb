// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/model_type_invalidation_map_test_util.h"

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

class ModelTypeInvalidationMapEqMatcher
    : public MatcherInterface<const ModelTypeInvalidationMap&> {
 public:
  explicit ModelTypeInvalidationMapEqMatcher(
      const ModelTypeInvalidationMap& expected);

  virtual bool MatchAndExplain(const ModelTypeInvalidationMap& lhs,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const ModelTypeInvalidationMap expected_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeInvalidationMapEqMatcher);
};

ModelTypeInvalidationMapEqMatcher::ModelTypeInvalidationMapEqMatcher(
    const ModelTypeInvalidationMap& expected) : expected_(expected) {
}

bool ModelTypeInvalidationMapEqMatcher::MatchAndExplain(
    const ModelTypeInvalidationMap& actual,
    MatchResultListener* listener) const {
  ModelTypeInvalidationMap expected_only;
  ModelTypeInvalidationMap actual_only;
  typedef std::pair<ModelType, std::pair<Invalidation, Invalidation> >
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

  for (ModelTypeInvalidationMap::const_iterator it = expected_.begin();
       it != expected_.end(); ++it) {
    ModelTypeInvalidationMap::const_iterator find_it =
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

void ModelTypeInvalidationMapEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void ModelTypeInvalidationMapEqMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

Matcher<const ModelTypeInvalidationMap&> Eq(
    const ModelTypeInvalidationMap& expected) {
  return MakeMatcher(new ModelTypeInvalidationMapEqMatcher(expected));
}

}  // namespace syncer
