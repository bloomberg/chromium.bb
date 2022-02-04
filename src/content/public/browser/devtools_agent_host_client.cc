// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_agent_host_client.h"

namespace content {

bool DevToolsAgentHostClient::MayAttachToURL(const GURL& url, bool is_webui) {
  return true;
}

// Defaults to true, restricted clients must override this to false.
bool DevToolsAgentHostClient::MayAttachToBrowser() {
  return true;
}

bool DevToolsAgentHostClient::MaySendInputEventsToBrowser() {
  return MayAttachToBrowser();
}

// File access is allowed by default, only restricted clients that represent
// not entirely trusted protocol peers override this to false.
bool DevToolsAgentHostClient::MayReadLocalFiles() {
  return true;
}

bool DevToolsAgentHostClient::MayWriteLocalFiles() {
  return true;
}

bool DevToolsAgentHostClient::UsesBinaryProtocol() {
  return false;
}

// Only clients that already have powers of local code execution should override
// this to true.
bool DevToolsAgentHostClient::AllowUnsafeOperations() {
  return false;
}

absl::optional<url::Origin>
DevToolsAgentHostClient::GetNavigationInitiatorOrigin() {
  return absl::nullopt;
}

std::string DevToolsAgentHostClient::GetTypeForMetrics() {
  return "Other";
}

}  // namespace content
