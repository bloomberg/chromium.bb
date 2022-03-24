// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/audio/arc_audio_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcAudioBridgeTest : public testing::Test {
 protected:
  ArcAudioBridgeTest() = default;
  ArcAudioBridgeTest(const ArcAudioBridgeTest&) = delete;
  ArcAudioBridgeTest& operator=(const ArcAudioBridgeTest&) = delete;
  ~ArcAudioBridgeTest() override = default;

  void SetUp() override {
    ash::CrasAudioHandler::InitializeForTesting();
    bridge_ = ArcAudioBridge::GetForBrowserContextForTesting(&context_);
  }
  void TearDown() override { ash::CrasAudioHandler::Shutdown(); }

  ArcAudioBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  ArcAudioBridge* bridge_ = nullptr;
};

TEST_F(ArcAudioBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
