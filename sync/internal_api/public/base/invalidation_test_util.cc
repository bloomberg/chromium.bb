// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation_test_util.h"

#include "base/basictypes.h"
#include "sync/internal_api/public/base/invalidation.h"

namespace syncer {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace {

class InvalidationEqMatcher
    : public MatcherInterface<const Invalidation&> {
 public:
  explicit InvalidationEqMatcher(const Invalidation& expected);

  virtual bool MatchAndExplain(const Invalidation& actual,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const Invalidation expected_;

  DISALLOW_COPY_AND_ASSIGN(InvalidationEqMatcher);
};

InvalidationEqMatcher::InvalidationEqMatcher(
    const Invalidation& expected) : expected_(expected) {
}

bool InvalidationEqMatcher::MatchAndExplain(
    const Invalidation& actual, MatchResultListener* listener) const {
  return expected_.payload == actual.payload;
}

void InvalidationEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void InvalidationEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

void PrintTo(const Invalidation& invalidation, ::std::ostream* os) {
  *os << "{ payload: " << invalidation.payload << " }";
}

Matcher<const Invalidation&> Eq(const Invalidation& expected) {
  return MakeMatcher(new InvalidationEqMatcher(expected));
}

}  // namespace syncer
