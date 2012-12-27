// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/session_event_executor.h"

using base::win::ScopedHandle;

namespace remoting {

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgentWin : public DesktopSessionAgent {
 public:
  DesktopSessionAgentWin(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner);

 protected:
  virtual ~DesktopSessionAgentWin();

  virtual bool CreateChannelForNetworkProcess(
      IPC::PlatformFileForTransit* client_out,
      scoped_ptr<IPC::ChannelProxy>* server_out) OVERRIDE;
  virtual scoped_ptr<EventExecutor> CreateEventExecutor() OVERRIDE;

  // Requests the daemon to inject Secure Attention Sequence into the current
  // session.
  void InjectSas();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgentWin);
};

DesktopSessionAgentWin::DesktopSessionAgentWin(
    scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner)
    : DesktopSessionAgent(audio_capture_task_runner,
                          caller_task_runner,
                          input_task_runner,
                          io_task_runner,
                          video_capture_task_runner) {
}

DesktopSessionAgentWin::~DesktopSessionAgentWin() {
}

bool DesktopSessionAgentWin::CreateChannelForNetworkProcess(
    IPC::PlatformFileForTransit* client_out,
    scoped_ptr<IPC::ChannelProxy>* server_out) {
  // Generate a unique name for the channel.
  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();

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
  std::string security_descriptor = base::StringPrintf(
      "O:%1$sG:%1$sD:(A;;GA;;;%1$s)", WideToUTF8(user_sid).c_str());

  // Create a connected IPC channel.
  ScopedHandle client;
  scoped_ptr<IPC::ChannelProxy> server;
  if (!CreateConnectedIpcChannel(channel_name, security_descriptor,
                                 io_task_runner(), this, &client, &server)) {
    return false;
  }

  *client_out = client.Take();
  *server_out = server.Pass();
  return true;
}

scoped_ptr<EventExecutor> DesktopSessionAgentWin::CreateEventExecutor() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  scoped_ptr<EventExecutor> event_executor =
      EventExecutor::Create(input_task_runner(), caller_task_runner());
  event_executor.reset(new SessionEventExecutorWin(
      input_task_runner(), event_executor.Pass(), caller_task_runner(),
      base::Bind(&DesktopSessionAgentWin::InjectSas, this)));
  return event_executor.Pass();
}

void DesktopSessionAgentWin::InjectSas() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (delegate().get())
    delegate()->InjectSas();
}

// static
scoped_refptr<DesktopSessionAgent> DesktopSessionAgent::Create(
    scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner) {
  return scoped_refptr<DesktopSessionAgent>(new DesktopSessionAgentWin(
      audio_capture_task_runner, caller_task_runner, input_task_runner,
      io_task_runner, video_capture_task_runner));
}

}  // namespace remoting
