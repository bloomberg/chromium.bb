// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_UTIL_H_
#define REMOTING_HOST_IPC_UTIL_H_

#include <string>

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace remoting {

#if defined(OS_WIN)

// Creates the server end of the IPC channel and applies the security
// descriptor |pipe_security_descriptor| to it.
bool CreateIpcChannel(
    const std::string& channel_name,
    const std::string& pipe_security_descriptor,
    base::win::ScopedHandle* pipe_out);

#endif  // defined(OS_WIN)

} // namespace remoting

#endif  // REMOTING_HOST_IPC_UTIL_H_
