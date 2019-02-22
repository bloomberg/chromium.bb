// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_environment_content.h"

#include <memory>

#include "ash/test/ash_test_views_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace ash {
namespace {

std::unique_ptr<content::WebContents> CreateWebContents(
    content::BrowserContext* browser_context) {
  return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                           nullptr);
}

}  // namespace

// static
std::unique_ptr<AshTestEnvironment> AshTestEnvironment::Create() {
  return std::make_unique<AshTestEnvironmentContent>();
}

// static
std::string AshTestEnvironment::Get100PercentResourceFileName() {
  return "ash_test_resources_with_content_100_percent.pak";
}

AshTestEnvironmentContent::AshTestEnvironmentContent()
    : network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()),
      thread_bundle_(std::make_unique<content::TestBrowserThreadBundle>()) {
  content::SetNetworkConnectionTrackerForTesting(
      network::TestNetworkConnectionTracker::GetInstance());
}

AshTestEnvironmentContent::~AshTestEnvironmentContent() {
  content::SetNetworkConnectionTrackerForTesting(nullptr);
}

void AshTestEnvironmentContent::SetUp() {
  scoped_web_contents_creator_ =
      std::make_unique<views::WebView::ScopedWebContentsCreatorForTesting>(
          base::BindRepeating(&CreateWebContents));
}

void AshTestEnvironmentContent::TearDown() {
  scoped_web_contents_creator_.reset();
}

std::unique_ptr<AshTestViewsDelegate>
AshTestEnvironmentContent::CreateViewsDelegate() {
  return std::make_unique<AshTestViewsDelegate>();
}

}  // namespace ash
