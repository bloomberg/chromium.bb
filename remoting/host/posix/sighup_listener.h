// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a signal handler that is used to safely handle SIGHUP
// and trigger the specified callback. It is used on Linux and Mac in order to
// reload the me2me host configuration.

#ifndef REMOTING_HOST_POSIX_SIGHUP_LISTENER_H_
#define REMOTING_HOST_POSIX_SIGHUP_LISTENER_H_

#include "base/callback_forward.h"

namespace remoting {

// Register for SIGHUP notifications on the current thread, which must have
// an associated MessageLoopForIO.
bool RegisterHupSignalHandler(const base::Closure& callback);

}  // namespace remoting

#endif  // REMOTING_HOST_POSIX_SIGHUP_LISTENER_H_
