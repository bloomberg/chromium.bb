// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/AdTracker.h"

#include "core/frame/LocalFrame.h"
#include "core/probe/CoreProbes.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class AdTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  LocalFrame* GetFrame() const {
    return page_holder_->GetDocument().GetFrame();
  }

  void WillExecuteScript(const String& script_url) {
    ad_tracker_->WillExecuteScript(script_url);
  }

  bool AnyExecutingScriptsTaggedAsAdResource() {
    return ad_tracker_->AnyExecutingScriptsTaggedAsAdResource();
  }

  void AppendToKnownAdScripts(const KURL& url) {
    ad_tracker_->AppendToKnownAdScripts(url);
  }

  Persistent<AdTracker> ad_tracker_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

void AdTrackerTest::SetUp() {
  page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  page_holder_->GetDocument().SetURL(KURL("https://example.com/foo"));
  ad_tracker_ = new AdTracker(GetFrame());
}

void AdTrackerTest::TearDown() {
  ad_tracker_->Shutdown();
}

TEST_F(AdTrackerTest, AnyExecutingScriptsTaggedAsAdResource) {
  KURL ad_script_url("https://example.com/bar.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

// Tests that if neither script in the stack is an ad,
// AnyExecutingScriptsTaggedAsAdResource should return false.
TEST_F(AdTrackerTest, AnyExecutingScriptsTaggedAsAdResource_False) {
  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
}

}  // namespace blink
