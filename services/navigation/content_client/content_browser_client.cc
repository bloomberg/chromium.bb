// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/navigation/content_client/content_browser_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/service_info.h"
#include "content/public/common/service_manager_connection.h"
#include "content/shell/browser/shell_browser_context.h"
#include "services/navigation/content_client/browser_main_parts.h"
#include "services/navigation/navigation.h"

namespace navigation {

ContentBrowserClient::ContentBrowserClient() {}
ContentBrowserClient::~ContentBrowserClient() {}

content::BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new BrowserMainParts(parameters);
  return browser_main_parts_;
}

std::string ContentBrowserClient::GetServiceUserIdForBrowserContext(
    content::BrowserContext* browser_context) {
  // Unlike Chrome, where there are different browser contexts for each process,
  // each with their own userid, here there is only one and we should reuse the
  // same userid as our own process to avoid having to create multiple shell
  // connections.
  return content::ServiceManagerConnection::GetForProcess()->GetIdentity()
      .user_id();
}

void ContentBrowserClient::RegisterInProcessServices(
    StaticServiceMap* services) {
  content::ServiceManagerConnection::GetForProcess()->AddConnectionFilter(
      base::MakeUnique<Navigation>());
}

}  // namespace navigation
