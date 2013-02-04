// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTING_ME2ME_HOST_H_
#define REMOTING_HOST_REMOTING_ME2ME_HOST_H_

#include "remoting/host/host_export.h"

namespace remoting {

// The host process's entry point exported from remoting_core.dll.
HOST_EXPORT int HostProcessMain(int argc, char** argv);

} // namespace remoting

#endif  // REMOTING_HOST_REMOTING_ME2ME_HOST_H_
