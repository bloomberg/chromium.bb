// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_ELEVATED_CONTROLLER_MODULE_H_
#define REMOTING_HOST_WIN_ELEVATED_CONTROLLER_MODULE_H_

#include "remoting/host/host_export.h"

namespace remoting {

// The elevated controller's entry point exported from remoting_core.dll.
HOST_EXPORT int ElevatedControllerMain();

} // namespace remoting

#endif  // REMOTING_HOST_WIN_ELEVATED_CONTROLLER_MODULE_H_
