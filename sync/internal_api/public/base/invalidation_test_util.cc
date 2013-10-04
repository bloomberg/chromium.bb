// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/invalidation_test_util.h"

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"
#include "sync/internal_api/public/base/invalidation.h"

namespace syncer {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace {

class AckHandleEqMatcher
    : public MatcherInterface<const AckHandle&> {
 public:
  explicit AckHandleEqMatcher(const AckHandle& expected);

  virtual bool MatchAndExplain(const AckHandle& actual,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const AckHandle expected_;

  DISALLOW_COPY_AND_ASSIGN(AckHandleEqMatcher);
};

AckHandleEqMatcher::AckHandleEqMatcher(const AckHandle& expected)
    : expected_(expected) {
}

bool AckHandleEqMatcher::MatchAndExplain(const AckHandle& actual,
                                         MatchResultListener* listener) const {
  return expected_.Equals(actual);
}

void AckHandleEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void AckHandleEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

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
  if (!(expected_.object_id() == actual.object_id())) {
    return false;
  }
  if (expected_.is_unknown_version() && actual.is_unknown_version()) {
    return true;
  } else if (expected_.is_unknown_version() != actual.is_unknown_version()) {
    return false;
  } else {
    // Neither is unknown version.
    return expected_.payload() == actual.payload()
        && expected_.version() == actual.version();
  }
}

void InvalidationEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void InvalidationEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

void PrintTo(const AckHandle& ack_handle, ::std::ostream* os ) {
  scoped_ptr<base::Value> value(ack_handle.ToValue());
  std::string printable_ack_handle;
  base::JSONWriter::Write(value.get(), &printable_ack_handle);
  *os << "{ ack_handle: " << printable_ack_handle << " }";
}

Matcher<const AckHandle&> Eq(const AckHandle& expected) {
  return MakeMatcher(new AckHandleEqMatcher(expected));
}

void PrintTo(const Invalidation& inv, ::std::ostream* os) {
  *os << "{ payload: " << inv.ToString() << " }";
}

Matcher<const Invalidation&> Eq(const Invalidation& expected) {
  return MakeMatcher(new InvalidationEqMatcher(expected));
}

}  // namespace syncer
