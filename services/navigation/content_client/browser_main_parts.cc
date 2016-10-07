// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/content_client/browser_main_parts.h"

#include "base/run_loop.h"
#include "content/public/common/service_manager_connection.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_net_log.h"
#include "services/navigation/navigation.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/test_views_delegate.h"

namespace navigation {

BrowserMainParts::BrowserMainParts(
    const content::MainFunctionParams& parameters) {}
BrowserMainParts::~BrowserMainParts() {}

void BrowserMainParts::ToolkitInitialized() {
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_.reset(new views::TestViewsDelegate);
}

void BrowserMainParts::PreMainMessageLoopRun() {
  content::ServiceManagerConnection* service_manager_connection =
      content::ServiceManagerConnection::GetForProcess();
  if (service_manager_connection) {
    window_manager_connection_ = views::WindowManagerConnection::Create(
        service_manager_connection->GetConnector(),
        service_manager_connection->GetIdentity());
  }
  net_log_.reset(new content::ShellNetLog("ash_shell"));
  browser_context_.reset(
      new content::ShellBrowserContext(false, net_log_.get()));
}

void BrowserMainParts::PostMainMessageLoopRun() {
  views_delegate_.reset();
  browser_context_.reset();
}

bool BrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::RunLoop().Run();
  return true;
}

}  // namespace navigation
