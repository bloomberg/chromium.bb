// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation_state_test_util.h"

#include "base/basictypes.h"
#include "sync/internal_api/public/base/invalidation_state.h"

namespace syncer {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace {

class InvalidationStateEqMatcher
    : public MatcherInterface<const InvalidationState&> {
 public:
  explicit InvalidationStateEqMatcher(const InvalidationState& expected);

  virtual bool MatchAndExplain(const InvalidationState& actual,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const InvalidationState expected_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationStateEqMatcher);
};

InvalidationStateEqMatcher::InvalidationStateEqMatcher(
    const InvalidationState& expected) : expected_(expected) {
}

bool InvalidationStateEqMatcher::MatchAndExplain(
    const InvalidationState& actual, MatchResultListener* listener) const {
  return expected_.payload == actual.payload;
}

void InvalidationStateEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void InvalidationStateEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

void PrintTo(const InvalidationState& state, ::std::ostream* os) {
  *os << "{ payload: " << state.payload << " }";
}

Matcher<const InvalidationState&> Eq(const InvalidationState& expected) {
  return MakeMatcher(new InvalidationStateEqMatcher(expected));
}

}  // namespace syncer
