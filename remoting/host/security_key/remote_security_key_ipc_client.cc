// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/remote_security_key_ipc_client.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/security_key/remote_security_key_ipc_constants.h"

namespace remoting {

RemoteSecurityKeyIpcClient::RemoteSecurityKeyIpcClient()
    : initial_ipc_channel_name_(remoting::GetRemoteSecurityKeyIpcChannelName()),
      weak_factory_(this) {}

RemoteSecurityKeyIpcClient::~RemoteSecurityKeyIpcClient() {}

bool RemoteSecurityKeyIpcClient::WaitForSecurityKeyIpcServerChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The retry loop is needed as the IPC Servers we connect to are reset (torn
  // down and recreated) and we should be resilient in that case.  We need to
  // strike a balance between resilience and speed as we do not want to add
  // un-necessary delay to the local scenario when no session is active.
  // 500ms was chosen as a reasonable balance between reliability of remote
  // session detection and overhead added to the local security key operation
  // when no remote session is present.
  const base::TimeDelta kTotalWaitTime = base::TimeDelta::FromMilliseconds(500);
  const base::TimeDelta kPerIterationWaitTime =
      base::TimeDelta::FromMilliseconds(10);
  const int kLoopIterations = kTotalWaitTime / kPerIterationWaitTime;
  for (int i = 0; i < kLoopIterations; i++) {
    if (IPC::Channel::IsNamedServerInitialized(initial_ipc_channel_name_)) {
      return true;
    }

    base::PlatformThread::Sleep(kPerIterationWaitTime);
  }

  return false;
}

void RemoteSecurityKeyIpcClient::EstablishIpcConnection(
    const base::Closure& connection_ready_callback,
    const base::Closure& connection_error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!connection_ready_callback.is_null());
  DCHECK(!connection_error_callback.is_null());
  DCHECK(!ipc_channel_);

  connection_ready_callback_ = connection_ready_callback;
  connection_error_callback_ = connection_error_callback;

  ConnectToIpcChannel(initial_ipc_channel_name_);
}

bool RemoteSecurityKeyIpcClient::SendSecurityKeyRequest(
    const std::string& request_payload,
    const ResponseCallback& response_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!request_payload.empty());
  DCHECK(!response_callback.is_null());

  if (!ipc_channel_) {
    LOG(ERROR) << "Request made before IPC connection was established.";
    return false;
  }

  if (!response_callback_.is_null()) {
    LOG(ERROR)
        << "Request made while waiting for a response to a previous request.";
    return false;
  }

  response_callback_ = response_callback;
  return ipc_channel_->Send(
      new ChromotingRemoteSecurityKeyToNetworkMsg_Request(request_payload));
}

void RemoteSecurityKeyIpcClient::CloseIpcConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ipc_channel_.reset();
}

void RemoteSecurityKeyIpcClient::SetInitialIpcChannelNameForTest(
    const std::string& initial_ipc_channel_name) {
  initial_ipc_channel_name_ = initial_ipc_channel_name;
}

void RemoteSecurityKeyIpcClient::SetExpectedIpcServerSessionIdForTest(
    uint32_t expected_session_id) {
  expected_ipc_server_session_id_ = expected_session_id;
}

bool RemoteSecurityKeyIpcClient::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RemoteSecurityKeyIpcClient, message)
    IPC_MESSAGE_HANDLER(
        ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionDetails,
        OnConnectionDetails)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToRemoteSecurityKeyMsg_Response,
                        OnSecurityKeyResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void RemoteSecurityKeyIpcClient::OnChannelConnected(int32_t peer_pid) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_WIN)
  DWORD peer_session_id;
  if (!ProcessIdToSessionId(peer_pid, &peer_session_id)) {
    uint32_t last_error = GetLastError();
    LOG(ERROR) << "ProcessIdToSessionId failed with error code: " << last_error;
    base::ResetAndReturn(&connection_error_callback_).Run();
    return;
  }

  if (peer_session_id != expected_ipc_server_session_id_) {
    LOG(ERROR)
        << "Cannot establish connection with IPC server running in session: "
        << peer_session_id;
    base::ResetAndReturn(&connection_error_callback_).Run();
    return;
  }
#endif  // defined(OS_WIN)

  // If we have received the connection details already (i.e.
  // |ipc_channel_name_| is populated) then we signal that the connection is
  // ready for use.  Otherwise this is the initial connection and we will wait
  // to receive the ConnectionDetails message before proceeding.
  if (!ipc_channel_name_.empty()) {
    base::ResetAndReturn(&connection_ready_callback_).Run();
  }
}

void RemoteSecurityKeyIpcClient::OnChannelError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!connection_error_callback_.is_null()) {
    base::ResetAndReturn(&connection_error_callback_).Run();
  }
}

void RemoteSecurityKeyIpcClient::OnConnectionDetails(
    const std::string& channel_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ipc_channel_name_ = channel_name;

  // Now that we have received the name for the IPC channel we will use for our
  // security key request, we want to disconnect from the intial IPC channel
  // and then connect to the new one.
  // NOTE: We do not want to perform these tasks now as we are in the middle of
  // existing IPC message handler, thus we post the tasks so they will be
  // handled after this method completes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RemoteSecurityKeyIpcClient::ConnectToIpcChannel,
                            weak_factory_.GetWeakPtr(),
                            base::ConstRef(ipc_channel_name_)));
}

void RemoteSecurityKeyIpcClient::OnSecurityKeyResponse(
    const std::string& response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!connection_error_callback_.is_null());

  if (!response_data.empty()) {
    base::ResetAndReturn(&response_callback_).Run(response_data);
  } else {
    LOG(ERROR) << "Invalid response received";
    base::ResetAndReturn(&connection_error_callback_).Run();
  }
}

void RemoteSecurityKeyIpcClient::ConnectToIpcChannel(
    const std::string& channel_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Verify that any existing IPC connection has been closed.
  CloseIpcConnection();

  // The retry loop is needed as the IPC Servers we connect to are reset (torn
  // down and recreated) and we should be resilient in that case.
  const base::TimeDelta kTotalWaitTime =
      base::TimeDelta::FromMilliseconds(1000);
  const base::TimeDelta kPerIterationWaitTime =
      base::TimeDelta::FromMilliseconds(25);
  const int kLoopIterations = kTotalWaitTime / kPerIterationWaitTime;
  IPC::ChannelHandle channel_handle(channel_name);
  for (int i = 0; i < kLoopIterations; i++) {
    ipc_channel_ = IPC::Channel::CreateNamedClient(channel_handle, this);
    if (ipc_channel_->Connect()) {
      return;
    }

    ipc_channel_.reset();
    base::PlatformThread::Sleep(kPerIterationWaitTime);
  }

  if (!connection_error_callback_.is_null()) {
    base::ResetAndReturn(&connection_error_callback_).Run();
  }
}

}  // namespace remoting
