// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_util.h"

#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/host/win/security_descriptor.h"

using base::win::ScopedHandle;

namespace remoting {

// Pipe name prefix used by Chrome IPC channels to convert a channel name into
// a pipe name.
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome.";

bool CreateConnectedIpcChannel(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Listener* listener,
    base::File* client_out,
    std::unique_ptr<IPC::ChannelProxy>* server_out) {
  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to query the current user SID.";
    return false;
  }

  // Create a security descriptor that will be used to protect the named pipe in
  // between CreateNamedPipe() and CreateFile() calls before it will be passed
  // to the network process. It gives full access to the account that
  // the calling code is running under and  denies access by anyone else.
  std::string user_sid_utf8 = base::WideToUTF8(user_sid);
  std::string security_descriptor =
      base::StringPrintf("O:%sG:%sD:(A;;GA;;;%s)", user_sid_utf8.c_str(),
                         user_sid_utf8.c_str(), user_sid_utf8.c_str());

  // Generate a unique name for the channel.
  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();

  // Create the server end of the channel.
  ScopedHandle pipe;
  if (!CreateIpcChannel(channel_name, security_descriptor, &pipe)) {
    return false;
  }

  // Wrap the pipe into an IPC channel.
  std::unique_ptr<IPC::ChannelProxy> server(
      new IPC::ChannelProxy(listener, io_task_runner));
  IPC::AttachmentBroker::GetGlobal()->RegisterCommunicationChannel(
      server.get(), io_task_runner);
  server->Init(IPC::ChannelHandle(pipe.Get()), IPC::Channel::MODE_SERVER,
                true);

  // Convert the channel name to the pipe name.
  std::string pipe_name(kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = nullptr;
  security_attributes.bInheritHandle = TRUE;

  // Create the client end of the channel. This code should match the code in
  // IPC::Channel.
  base::File client(CreateFile(base::UTF8ToUTF16(pipe_name).c_str(),
                               GENERIC_READ | GENERIC_WRITE,
                               0,
                               &security_attributes,
                               OPEN_EXISTING,
                               SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                                   FILE_FLAG_OVERLAPPED,
                               nullptr));
  if (!client.IsValid()) {
    PLOG(ERROR) << "Failed to connect to '" << pipe_name << "'";
    return false;
  }

  *client_out = std::move(client);
  *server_out = std::move(server);
  return true;
}

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
