// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

Service::~Service() {}

void Service::OnStart(ServiceContext* context) {}

bool Service::OnStop() { return true; }

}  // namespace service_manager
