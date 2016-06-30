// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/gnubby_auth_handler.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/win_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/ipc_util.h"
#include "remoting/host/security_key/remote_security_key_ipc_constants.h"
#include "remoting/host/security_key/remote_security_key_ipc_server.h"

namespace {

// The timeout used to disconnect a client from the IPC Server channel if it
// forgets to do so.  This ensures the server channel is not blocked forever.
const int kInitialRequestTimeoutSeconds = 5;

// This value represents the amount of time to wait for a gnubby request from
// the client before terminating the connection.
const int kGnubbyRequestTimeoutSeconds = 60;

}  // namespace

namespace remoting {

// Creates an IPC server channel which services IPC clients that want to start
// a security key forwarding session.  Once an IPC Client connects to the
// server, the GnubbyAuthHandlerWin class will create a new
// RemoteSecurityKeyIpcServer instance that will service that request.  The new
// instance will exist for the lifetime of the security key request and will be
// assigned a unique IPC channel name and connection id.  The channel name is
// sent to the client which should disconnect the IPC server channel and
// connect to the security key forwarding session IPC channel to send/receive
// security key messages.  The IPC server channel will then be reset so it can
// can service the next client/request.  This system allows multiple security
// key forwarding sessions to occur concurrently.
// TODO(joedow): Update GnubbyAuthHandler impls to run on a separate IO thread
//               instead of the thread it was created on: crbug.com/591739
class GnubbyAuthHandlerWin : public GnubbyAuthHandler, public IPC::Listener {
 public:
  explicit GnubbyAuthHandlerWin(ClientSessionDetails* client_session_details);
  ~GnubbyAuthHandlerWin() override;

 private:
  typedef std::map<int, std::unique_ptr<RemoteSecurityKeyIpcServer>>
      ActiveChannels;

  // GnubbyAuthHandler interface.
  void CreateGnubbyConnection() override;
  bool IsValidConnectionId(int gnubby_connection_id) const override;
  void SendClientResponse(int gnubby_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int gnubby_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Creates the IPC server channel and waits for a connection using
  // |ipc_server_channel_name_|.
  void StartIpcServerChannel();

  // Restarts the IPC server channel to prepare for another connection.
  void RecreateIpcServerChannel();

  // Closes the IPC channel created for a security key forwarding session.
  void CloseSecurityKeyRequestIpcChannel(int connection_id);

  // Returns the IPC Channel instance created for |connection_id|.
  ActiveChannels::const_iterator GetChannelForConnectionId(
      int connection_id) const;

  // Creates a unique name based on the well-known IPC channel name.
  std::string GenerateUniqueChannelName();

  // Represents the last id assigned to a new security key request IPC channel.
  int last_connection_id_ = 0;

  // Sends a gnubby extension messages to the remote client when called.
  SendMessageCallback send_message_callback_;

  // Interface which provides details about the client session.
  ClientSessionDetails* client_session_details_ = nullptr;

  // Tracks the IPC channel created for each security key forwarding session.
  ActiveChannels active_channels_;

  // The amount of time to wait for a client to process the connection details
  // message and disconnect from the IPC server channel before disconnecting it.
  base::TimeDelta disconnect_timeout_;

  // Used to recreate the IPC server channel if a client forgets to disconnect.
  base::OneShotTimer timer_;

  // IPC Clients connect to this channel first to receive their own IPC
  // channel to start a security key forwarding session on.
  std::unique_ptr<IPC::Channel> ipc_server_channel_;

  // Ensures GnubbyAuthHandlerWin methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<GnubbyAuthHandlerWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GnubbyAuthHandlerWin);
};

std::unique_ptr<GnubbyAuthHandler> GnubbyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback) {
  std::unique_ptr<GnubbyAuthHandler> auth_handler(
      new GnubbyAuthHandlerWin(client_session_details));
  auth_handler->SetSendMessageCallback(send_message_callback);
  return auth_handler;
}

GnubbyAuthHandlerWin::GnubbyAuthHandlerWin(
    ClientSessionDetails* client_session_details)
    : client_session_details_(client_session_details),
      disconnect_timeout_(
          base::TimeDelta::FromSeconds(kInitialRequestTimeoutSeconds)),
      weak_factory_(this) {
  DCHECK(client_session_details_);
}

GnubbyAuthHandlerWin::~GnubbyAuthHandlerWin() {}

void GnubbyAuthHandlerWin::CreateGnubbyConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StartIpcServerChannel();
}

bool GnubbyAuthHandlerWin::IsValidConnectionId(int connection_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return (GetChannelForConnectionId(connection_id) != active_channels_.end());
}

void GnubbyAuthHandlerWin::SendClientResponse(
    int connection_id,
    const std::string& response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ActiveChannels::const_iterator iter =
      GetChannelForConnectionId(connection_id);
  if (iter == active_channels_.end()) {
    HOST_LOG << "Invalid gnubby connection ID received: " << connection_id;
    return;
  }

  if (!iter->second->SendResponse(response_data)) {
    CloseSecurityKeyRequestIpcChannel(connection_id);
  }
}

void GnubbyAuthHandlerWin::SendErrorAndCloseConnection(int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SendClientResponse(connection_id, kRemoteSecurityKeyConnectionError);
  CloseSecurityKeyRequestIpcChannel(connection_id);
}

void GnubbyAuthHandlerWin::SetSendMessageCallback(
    const SendMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  send_message_callback_ = callback;
}

size_t GnubbyAuthHandlerWin::GetActiveConnectionCountForTest() const {
  return active_channels_.size();
}

void GnubbyAuthHandlerWin::SetRequestTimeoutForTest(base::TimeDelta timeout) {
  disconnect_timeout_ = timeout;
}

void GnubbyAuthHandlerWin::StartIpcServerChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Create a named pipe owned by the current user (the LocalService account
  // (SID: S-1-5-19) when running in the network process) which is available to
  // all authenticated users.
  // presubmit: allow wstring
  std::wstring user_sid;
  CHECK(base::win::GetUserSidString(&user_sid));
  std::string user_sid_utf8 = base::WideToUTF8(user_sid);
  std::string security_descriptor = base::StringPrintf(
      "O:%sG:%sD:(A;;GA;;;AU)", user_sid_utf8.c_str(), user_sid_utf8.c_str());

  base::win::ScopedHandle pipe;
  CHECK(CreateIpcChannel(remoting::GetRemoteSecurityKeyIpcChannelName(),
                         security_descriptor, &pipe));
  ipc_server_channel_ =
      IPC::Channel::CreateNamedServer(IPC::ChannelHandle(pipe.Get()), this);
  CHECK(ipc_server_channel_->Connect());
}

void GnubbyAuthHandlerWin::RecreateIpcServerChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  timer_.Stop();
  ipc_server_channel_.reset();

  StartIpcServerChannel();
}

void GnubbyAuthHandlerWin::CloseSecurityKeyRequestIpcChannel(
    int connection_id) {
  active_channels_.erase(connection_id);
}

GnubbyAuthHandlerWin::ActiveChannels::const_iterator
GnubbyAuthHandlerWin::GetChannelForConnectionId(int connection_id) const {
  return active_channels_.find(connection_id);
}

std::string GnubbyAuthHandlerWin::GenerateUniqueChannelName() {
  return GetRemoteSecurityKeyIpcChannelName() + "." +
         IPC::Channel::GenerateUniqueRandomChannelID();
}

bool GnubbyAuthHandlerWin::OnMessageReceived(const IPC::Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // This class does handle any IPC messages sent by the client.
  return false;
}

void GnubbyAuthHandlerWin::OnChannelConnected(int32_t peer_pid) {
  DCHECK(thread_checker_.CalledOnValidThread());

  timer_.Start(FROM_HERE, disconnect_timeout_,
               base::Bind(&GnubbyAuthHandlerWin::OnChannelError,
                          base::Unretained(this)));

  // Verify the IPC connection attempt originated from the session we are
  // currently remoting.  We don't want to service requests from arbitrary
  // Windows sessions.
  bool close_connection = false;
  DWORD peer_session_id;
  if (!ProcessIdToSessionId(peer_pid, &peer_session_id)) {
    PLOG(ERROR) << "ProcessIdToSessionId() failed";
    close_connection = true;
  } else if (peer_session_id != client_session_details_->desktop_session_id()) {
    LOG(INFO) << "Ignoring connection attempt from outside remoted session.";
    close_connection = true;
  }
  if (close_connection) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&GnubbyAuthHandlerWin::OnChannelError,
                              weak_factory_.GetWeakPtr()));
    return;
  }

  int new_connection_id = ++last_connection_id_;
  std::unique_ptr<RemoteSecurityKeyIpcServer> ipc_server(
      RemoteSecurityKeyIpcServer::Create(
          new_connection_id, peer_session_id, disconnect_timeout_,
          send_message_callback_,
          base::Bind(&GnubbyAuthHandlerWin::CloseSecurityKeyRequestIpcChannel,
                     base::Unretained(this), new_connection_id)));

  std::string unique_channel_name = GenerateUniqueChannelName();
  if (ipc_server->CreateChannel(
          unique_channel_name,
          base::TimeDelta::FromSeconds(kGnubbyRequestTimeoutSeconds))) {
    active_channels_[new_connection_id] = std::move(ipc_server);
    ipc_server_channel_->Send(
        new ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionDetails(
            unique_channel_name));
  }
}

void GnubbyAuthHandlerWin::OnChannelError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Could be an error, most likely the client disconnected though.  Either way
  // we should restart the server to prepare for the next connection.
  RecreateIpcServerChannel();
}

}  // namespace remoting
