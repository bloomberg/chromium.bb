// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_plugin_logger.h"

#include "remoting/host/plugin/host_script_object.h"

namespace remoting {

HostPluginLogger::HostPluginLogger(HostNPScriptObject* scriptable)
    : scriptable_object_(scriptable) {
}

HostPluginLogger::~HostPluginLogger() {
}

void HostPluginLogger::LogToUI(const std::string& message) {
  scriptable_object_->LogDebugInfo(message);
}

}  // namespace remoting
