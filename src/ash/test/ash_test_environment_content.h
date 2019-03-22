// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_ENVIRONMENT_CONTENT_H_
#define ASH_TEST_ASH_TEST_ENVIRONMENT_CONTENT_H_

#include "ash/test/ash_test_environment.h"
#include "base/macros.h"
#include "ui/views/controls/webview/webview.h"

namespace content {
class TestBrowserThreadBundle;
}

namespace network {
class TestNetworkConnectionTracker;
}

namespace ash {

// AshTestEnvironment implementation for tests that use content.
class AshTestEnvironmentContent : public AshTestEnvironment {
 public:
  AshTestEnvironmentContent();
  ~AshTestEnvironmentContent() override;

  // AshTestEnvironment:
  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<AshTestViewsDelegate> CreateViewsDelegate() override;

 private:
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  std::unique_ptr<views::WebView::ScopedWebContentsCreatorForTesting>
      scoped_web_contents_creator_;

  DISALLOW_COPY_AND_ASSIGN(AshTestEnvironmentContent);
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_ENVIRONMENT_CONTENT_H_
