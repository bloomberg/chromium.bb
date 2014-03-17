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
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/win/scoped_handle.h"

namespace remoting {

// This lock should be taken when creating handles that will be inherited by
// a child process. Without it the child process can inherit handles created for
// a different child process started at the same time.
extern base::LazyInstance<base::Lock>::Leaky g_inherit_handles_lock;

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32 session_id, base::win::ScopedHandle* token_out);

// Launches |binary| in the security context of the user represented by
// |user_token|. The session ID specified by the token is respected as well.
// The other parameters are passed directly to CreateProcessAsUser().
// If |inherit_handles| is true |g_inherit_handles_lock| should be taken while
// any inheritable handles are open.
bool LaunchProcessWithToken(const base::FilePath& binary,
                            const base::CommandLine::StringType& command_line,
                            HANDLE user_token,
                            SECURITY_ATTRIBUTES* process_attributes,
                            SECURITY_ATTRIBUTES* thread_attributes,
                            bool inherit_handles,
                            DWORD creation_flags,
                            const base::char16* desktop_name,
                            base::win::ScopedHandle* process_out,
                            base::win::ScopedHandle* thread_out);

} // namespace remoting

#endif  // REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
