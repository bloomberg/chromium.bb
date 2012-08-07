// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
#define REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_

#include <windows.h>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/process_util.h"

namespace remoting {

// Launches |binary| in the security context of the user represented by
// |user_token|. The session ID specified by the token is respected as well.
bool LaunchProcessWithToken(const FilePath& binary,
                            const CommandLine::StringType& command_line,
                            HANDLE user_token,
                            base::Process* process_out);

} // namespace remoting

#endif  // REMOTING_HOST_WIN_LAUNCH_PROCESS_WITH_TOKEN_H_
