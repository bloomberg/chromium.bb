// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_MANAGER_INTERFACE_H_
#define SERVICES_WS_WINDOW_MANAGER_INTERFACE_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace ws {

// Used for any associated interfaces that the local environment exposes to
// clients.
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowManagerInterface {
 public:
  WindowManagerInterface() {}
  virtual ~WindowManagerInterface() {}
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_MANAGER_INTERFACE_H_
