// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller/OomInterventionImpl.h"

#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class OomInterventionImplTest : public ::testing::Test {
 protected:
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

TEST_F(OomInterventionImplTest, DetectedAndDeclined) {
  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad("about::blank");
  Page* page = web_view->MainFrameImpl()->GetFrame()->GetPage();
  EXPECT_FALSE(page->Paused());

  auto intervention = std::make_unique<OomInterventionImpl>();
  EXPECT_TRUE(page->Paused());

  intervention.reset();
  EXPECT_FALSE(page->Paused());
}

}  // namespace blink
