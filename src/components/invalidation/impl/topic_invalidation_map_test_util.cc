// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/topic_invalidation_map_test_util.h"

#include <algorithm>

#include "base/macros.h"

namespace syncer {

using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::PrintToString;

namespace {

class TopicInvalidationMapEqMatcher
    : public MatcherInterface<const TopicInvalidationMap&> {
 public:
  explicit TopicInvalidationMapEqMatcher(const TopicInvalidationMap& expected);

  bool MatchAndExplain(const TopicInvalidationMap& lhs,
                       MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  const TopicInvalidationMap expected_;

  DISALLOW_COPY_AND_ASSIGN(TopicInvalidationMapEqMatcher);
};

TopicInvalidationMapEqMatcher::TopicInvalidationMapEqMatcher(
    const TopicInvalidationMap& expected)
    : expected_(expected) {}

struct InvalidationEqPredicate {
  explicit InvalidationEqPredicate(const Invalidation& inv1) : inv1_(inv1) {}

  bool operator()(const Invalidation& inv2) { return inv1_.Equals(inv2); }

  const Invalidation& inv1_;
};

bool TopicInvalidationMapEqMatcher::MatchAndExplain(
    const TopicInvalidationMap& actual,
    MatchResultListener* listener) const {
  std::vector<syncer::Invalidation> expected_invalidations;
  std::vector<syncer::Invalidation> actual_invalidations;

  expected_.GetAllInvalidations(&expected_invalidations);
  actual.GetAllInvalidations(&actual_invalidations);

  std::vector<syncer::Invalidation> expected_only;
  std::vector<syncer::Invalidation> actual_only;

  for (const auto& expected_invalidation : expected_invalidations) {
    if (std::find_if(actual_invalidations.begin(), actual_invalidations.end(),
                     InvalidationEqPredicate(expected_invalidation)) ==
        actual_invalidations.end()) {
      expected_only.push_back(expected_invalidation);
    }
  }

  for (const auto& actual_invalidation : actual_invalidations) {
    if (std::find_if(expected_invalidations.begin(),
                     expected_invalidations.end(),
                     InvalidationEqPredicate(actual_invalidation)) ==
        expected_invalidations.end()) {
      actual_only.push_back(actual_invalidation);
    }
  }

  if (expected_only.empty() && actual_only.empty()) {
    return true;
  }

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

  return false;
}

void TopicInvalidationMapEqMatcher::DescribeTo(::std::ostream* os) const {
  // TODO(crbug.com/1055286): seems there is no custom printer for
  // TopicInvalidationMap.
  *os << " is equal to " << PrintToString(expected_);
}

void TopicInvalidationMapEqMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

Matcher<const TopicInvalidationMap&> Eq(const TopicInvalidationMap& expected) {
  return MakeMatcher(new TopicInvalidationMapEqMatcher(expected));
}

}  // namespace syncer
