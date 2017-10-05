// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "core/frame/WebLocalFrameImpl.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/WebFrameScheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace blink {

class WebFrameSchedulerFrameTypeTest : public SimTest {};

TEST_F(WebFrameSchedulerFrameTypeTest, GetFrameType) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<!DOCTYPE HTML>"
      "<body>"
      "<iframe src=\"about:blank\"></iframe>"
      "</body>");

  EXPECT_EQ(WebFrameScheduler::FrameType::kMainFrame,
            MainFrame().GetFrame()->FrameScheduler()->GetFrameType());

  Frame* child = MainFrame().GetFrame()->Tree().FirstChild();
  ASSERT_TRUE(child->IsLocalFrame());
  EXPECT_EQ(WebFrameScheduler::FrameType::kSubframe,
            ToLocalFrame(child)->FrameScheduler()->GetFrameType());
}

}  // namespace blink
