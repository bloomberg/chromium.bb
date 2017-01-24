// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSResourceValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSResourceValue : public CSSResourceValue {
 public:
  FakeCSSResourceValue(ResourceStatus status) : m_status(status) {}
  ResourceStatus status() const override { return m_status; }

  CSSValue* toCSSValue() const override { return nullptr; }
  StyleValueType type() const override { return Unknown; }

 private:
  ResourceStatus m_status;
};

}  // namespace

TEST(CSSResourceValueTest, TestStatus) {
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::NotStarted))->state(),
            "unloaded");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::Pending))->state(),
            "loading");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::Cached))->state(),
            "loaded");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::LoadError))->state(),
            "error");
  EXPECT_EQ((new FakeCSSResourceValue(ResourceStatus::DecodeError))->state(),
            "error");
}

}  // namespace blink
