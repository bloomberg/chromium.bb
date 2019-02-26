// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_feature_list_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class AwContentBrowserClientTest : public testing::Test {};

TEST_F(AwContentBrowserClientTest, DisableCreatingTaskScheduler) {
  AwFeatureListCreator aw_feature_list_creator;
  AwContentBrowserClient client(&aw_feature_list_creator);
  EXPECT_TRUE(client.ShouldCreateTaskScheduler());

  AwContentBrowserClient::DisableCreatingTaskScheduler();
  EXPECT_FALSE(client.ShouldCreateTaskScheduler());
}

}  // namespace android_webview
