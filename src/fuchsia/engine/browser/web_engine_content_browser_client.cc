// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_content_browser_client.h"

#include <utility>

#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"
#include "fuchsia/engine/browser/web_engine_devtools_manager_delegate.h"

WebEngineContentBrowserClient::WebEngineContentBrowserClient(
    zx::channel context_channel)
    : context_channel_(std::move(context_channel)) {}

WebEngineContentBrowserClient::~WebEngineContentBrowserClient() = default;

content::BrowserMainParts*
WebEngineContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  DCHECK(context_channel_);
  main_parts_ = new WebEngineBrowserMainParts(std::move(context_channel_));
  return main_parts_;
}

content::DevToolsManagerDelegate*
WebEngineContentBrowserClient::GetDevToolsManagerDelegate() {
  DCHECK(main_parts_);
  DCHECK(main_parts_->browser_context());
  return new WebEngineDevToolsManagerDelegate(main_parts_->browser_context());
}

std::string WebEngineContentBrowserClient::GetProduct() const {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string WebEngineContentBrowserClient::GetUserAgent() const {
  return content::BuildUserAgentFromProduct(
      version_info::GetProductNameAndVersionForUserAgent());
}
