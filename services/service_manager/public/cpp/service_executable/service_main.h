// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_EXECUTABLE_SERVICE_MAIN_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_EXECUTABLE_SERVICE_MAIN_H_

#include "services/service_manager/public/mojom/service.mojom.h"

// Service executables linking against the
// "//services/service_manager/public/cpp/service_executable:main" target must
// implement this function as their entry point.
void ServiceMain(service_manager::mojom::ServiceRequest request);

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_SERVICE_SERVICE_MAIN_H_
