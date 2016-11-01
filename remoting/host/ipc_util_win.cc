// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_util.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "ipc/ipc_channel.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting {

// Pipe name prefix used by Chrome IPC channels to convert a channel name into
// a pipe name.
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome.";

bool CreateIpcChannel(
    const std::string& channel_name,
    const std::string& pipe_security_descriptor,
    base::win::ScopedHandle* pipe_out) {
  // Create security descriptor for the channel.
  ScopedSd sd = ConvertSddlToSd(pipe_security_descriptor);
  if (!sd) {
    PLOG(ERROR) << "Failed to create a security descriptor for the Chromoting "
                   "IPC channel";
    return false;
  }

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = sd.get();
  security_attributes.bInheritHandle = FALSE;

  // Convert the channel name to the pipe name.
  std::string pipe_name(kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  // Create the server end of the pipe. This code should match the code in
  // IPC::Channel with exception of passing a non-default security descriptor.
  base::win::ScopedHandle pipe;
  pipe.Set(CreateNamedPipe(
      base::UTF8ToUTF16(pipe_name).c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
      1,
      IPC::Channel::kReadBufferSize,
      IPC::Channel::kReadBufferSize,
      5000,
      &security_attributes));
  if (!pipe.IsValid()) {
    PLOG(ERROR)
        << "Failed to create the server end of the Chromoting IPC channel";
    return false;
  }

  *pipe_out = std::move(pipe);
  return true;
}

} // namespace remoting
