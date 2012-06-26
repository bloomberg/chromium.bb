// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LAUNCH_PROCESS_IN_SESSION_WIN_H_
#define REMOTING_HOST_LAUNCH_PROCESS_IN_SESSION_WIN_H_

#include <windows.h>
#include <string>

#include "base/file_path.h"
#include "base/process_util.h"

namespace remoting {

// Launches |binary| in a different session. The target session is specified by
// |user_token|.
bool LaunchProcessInSession(const FilePath& binary,
                            const std::wstring& command_line,
                            HANDLE user_token,
                            base::Process* process_out);

} // namespace remoting

#endif  // REMOTING_HOST_LAUNCH_PROCESS_IN_SESSION_WIN_H_
