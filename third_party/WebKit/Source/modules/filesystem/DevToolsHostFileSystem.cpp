// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/filesystem/DevToolsHostFileSystem.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/DevToolsHost.h"
#include "core/page/Page.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "platform/json/JSONValues.h"

namespace blink {

DOMFileSystem* DevToolsHostFileSystem::isolatedFileSystem(
    DevToolsHost& host,
    const String& file_system_name,
    const String& root_url) {
  ExecutionContext* context = host.FrontendFrame()->GetDocument();
  return DOMFileSystem::Create(context, file_system_name,
                               kFileSystemTypeIsolated, KURL(root_url));
}

void DevToolsHostFileSystem::upgradeDraggedFileSystemPermissions(
    DevToolsHost& host,
    DOMFileSystem* dom_file_system) {
  std::unique_ptr<JSONObject> message = JSONObject::Create();
  message->SetInteger("id", 0);
  message->SetString("method", "upgradeDraggedFileSystemPermissions");
  std::unique_ptr<JSONArray> params = JSONArray::Create();
  params->PushString(dom_file_system->RootURL().GetString());
  message->SetArray("params", std::move(params));
  host.sendMessageToEmbedder(message->ToJSONString());
}

}  // namespace blink
