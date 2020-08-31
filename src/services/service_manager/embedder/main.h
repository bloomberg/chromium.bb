// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_H_
#define SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace service_manager {

class MainDelegate;

struct COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER) MainParams {
  explicit MainParams(MainDelegate* delegate);
  ~MainParams();

  MainDelegate* const delegate;

#if !defined(OS_WIN) && !defined(OS_ANDROID)
  int argc = 0;
  const char** argv = nullptr;
#endif
};

// Main function which should be called as early as possible by any executable
// embedding the service manager.
int COMPONENT_EXPORT(SERVICE_MANAGER_EMBEDDER) Main(const MainParams& params);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_H_
