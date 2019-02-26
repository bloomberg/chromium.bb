// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/portal/portal.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class PortalUnitTest : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    RenderViewHostImplTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PortalUnitTest, InterfaceExists) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(blink::features::kPortals));

  ASSERT_TRUE(contents()->GetMainFrame()->binder_registry().CanBindInterface(
      blink::mojom::Portal::Name_));
}

}  // namespace content
