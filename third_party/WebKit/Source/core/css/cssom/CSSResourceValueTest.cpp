// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSResourceValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSResourceValue : public CSSResourceValue {
 public:
  FakeCSSResourceValue(ResourceStatus status) : status_(status) {}
  ResourceStatus Status() const override { return status_; }

  const CSSValue* ToCSSValue() const override { return nullptr; }
  StyleValueType GetType() const override { return kUnknownType; }

 private:
  ResourceStatus status_;
};

}  // namespace

TEST(CSSResourceValueTest, TestStatus) {
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::kNotStarted))->state(),
            "unloaded");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::kPending))->state(),
            "loading");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::kCached))->state(),
            "loaded");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::kLoadError))->state(),
            "error");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::kDecodeError))->state(),
            "error");
}

}  // namespace blink
