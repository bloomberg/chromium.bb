// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_UTIL_H_
#define REMOTING_HOST_IPC_UTIL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif  // defined(OS_WIN)

namespace base {
class File;
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
class Listener;
}  // namespace IPC

namespace remoting {

// Creates an already connected IPC channel. The server end of the channel
// is wrapped into a channel proxy that will invoke methods of |listener|
// on the caller's thread while using |io_task_runner| to send and receive
// messages in the background. The client end is returned as a pipe handle
// (inheritable on Windows).
// The channel is registered with the global AttachmentBroker.
bool CreateConnectedIpcChannel(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Listener* listener,
    base::File* client_out,
    std::unique_ptr<IPC::ChannelProxy>* server_out);

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
