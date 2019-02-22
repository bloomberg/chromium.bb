// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_TARGET_REGISTRY_H_
#define CONTENT_BROWSER_DEVTOOLS_TARGET_REGISTRY_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"

namespace content {

class DevToolsSession;

class TargetRegistry {
 public:
  static bool IsRuntimeResumeCommand(base::Value* value);

  explicit TargetRegistry(DevToolsSession* root_session);
  ~TargetRegistry();

  void AttachSubtargetSession(const std::string& session_id,
                              DevToolsAgentHostImpl* agent_host,
                              DevToolsAgentHostClient* client,
                              base::OnceClosure resume_if_throttled);
  void DetachSubtargetSession(const std::string& session_id);
  bool CanDispatchMessageOnAgentHost(base::DictionaryValue* parsed_message);
  void DispatchMessageOnAgentHost(
      const std::string& message,
      std::unique_ptr<base::DictionaryValue> parsed_message);
  void SendMessageToClient(const std::string& session_id,
                           const std::string& message);

 private:
  DevToolsSession* root_session_;
  struct SessionInfo;
  base::flat_map<std::string, std::unique_ptr<SessionInfo>> sessions_;
  DISALLOW_COPY_AND_ASSIGN(TargetRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_TARGET_REGISTRY_H_
