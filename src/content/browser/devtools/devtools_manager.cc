// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return base::Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(
          GetContentClient()->browser()->CreateDevToolsManagerDelegate()) {}

DevToolsManager::~DevToolsManager() = default;

}  // namespace content
