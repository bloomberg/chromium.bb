// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/demo/mus_demo_service.h"

#include "base/memory/ptr_util.h"
#include "services/ui/demo/mus_demo.h"

namespace ui {
namespace demo {

MusDemoService::MusDemoService() : mus_demo_(base::MakeUnique<MusDemo>()) {}

MusDemoService::~MusDemoService() {}

void MusDemoService::OnStart(const service_manager::ServiceInfo& info) {
  mus_demo_->Start(connector());
}

bool MusDemoService::OnConnect(const service_manager::ServiceInfo& remote_info,
                               service_manager::InterfaceRegistry* registry) {
  return true;
}

}  // namespace demo
}  // namespace ui
