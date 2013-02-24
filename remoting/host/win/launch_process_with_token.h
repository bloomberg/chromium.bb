// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
#define REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_

#include <windows.h>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_handle.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
class Listener;
}  // namespace IPC

namespace remoting {

// Pipe name prefix used by Chrome IPC channels to convert a channel name into
// a pipe name.
extern const char kChromePipeNamePrefix[];

// This lock should be taken when creating handles that will be inherited by
// a child process. Without it the child process can inherit handles created for
// a different child process started at the same time.
extern base::LazyInstance<base::Lock>::Leaky g_inherit_handles_lock;

// Creates an already connected IPC channel. The server end of the channel
// is wrapped into a channel proxy that will invoke methods of |delegate|
// on the caller's thread while using |io_task_runner| to send and receive
// messages in the background. The client end is returned as an inheritable NT
// handle. |pipe_security_descriptor| is applied to the underlying pipe.
bool CreateConnectedIpcChannel(
    const std::string& channel_name,
    const std::string& pipe_security_descriptor,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Listener* delegate,
    base::win::ScopedHandle* client_out,
    scoped_ptr<IPC::ChannelProxy>* server_out);

// Creates the server end of the IPC channel and applies the security
// descriptor |pipe_security_descriptor| to it.
bool CreateIpcChannel(
    const std::string& channel_name,
    const std::string& pipe_security_descriptor,
    base::win::ScopedHandle* pipe_out);

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32 session_id, base::win::ScopedHandle* token_out);

// Launches |binary| in the security context of the user represented by
// |user_token|. The session ID specified by the token is respected as well.
// The other parameters are passed directly to CreateProcessAsUser().
// If |inherit_handles| is true |g_inherit_handles_lock| should be taken while
// any inheritable handles are open.
bool LaunchProcessWithToken(const base::FilePath& binary,
                            const CommandLine::StringType& command_line,
                            HANDLE user_token,
                            SECURITY_ATTRIBUTES* process_attributes,
                            SECURITY_ATTRIBUTES* thread_attributes,
                            bool inherit_handles,
                            DWORD creation_flags,
                            const char16* desktop_name,
                            base::win::ScopedHandle* process_out,
                            base::win::ScopedHandle* thread_out);

} // namespace remoting

#endif  // REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
