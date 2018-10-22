// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_event_server.h"

#include <iostream>
#include <string>

#include "base/bind.h"

namespace tools {

AXEventServer::AXEventServer(base::ProcessId pid)
    : recorder_(
          content::AccessibilityEventRecorder::GetInstance(nullptr, pid)) {
  recorder_.ListenToEvents(
      base::BindRepeating(&AXEventServer::OnEvent, base::Unretained(this)));
  std::cout << "Events for process id: " << pid << std::endl;
}

AXEventServer::~AXEventServer() = default;

void AXEventServer::OnEvent(const std::string& event) const {
  std::cout << event << std::endl;
}

}  // namespace tools
