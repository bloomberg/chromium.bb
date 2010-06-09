// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/simple_host.h"

#include "base/stl_util-inl.h"
#include "build/build_config.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/host/session_manager.h"
#include "remoting/jingle_glue/jingle_channel.h"

namespace remoting {

SimpleHost::SimpleHost(const std::string& username,
                       const std::string& password,
                       Capturer* capturer,
                       Encoder* encoder,
                       EventExecutor* executor)
      : capture_thread_("CaptureThread"),
        encode_thread_("EncodeThread"),
        username_(username),
        password_(password),
        capturer_(capturer),
        encoder_(encoder),
        executor_(executor) {
}

void SimpleHost::Run() {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // Submit a task to perform host registration. We'll also start
  // listening to connection if registration is done.
  RegisterHost();

  // Run the main message loop. This is the main loop of this host
  // object.
  main_loop_.Run();
}

// This method is called when we need to the host process.
void SimpleHost::DestroySession() {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // First we tell the session to pause and then we wait until all
  // the tasks are done.
  session_->Pause();

  // TODO(hclam): Revise the order.
  encode_thread_.Stop();
  capture_thread_.Stop();
}

// This method talks to the cloud to register the host process. If
// successful we will start listening to network requests.
void SimpleHost::RegisterHost() {
  DCHECK_EQ(&main_loop_, MessageLoop::current());
  DCHECK(!jingle_client_);

  // Connect to the talk network with a JingleClient.
  jingle_client_ = new JingleClient();
  jingle_client_->Init(username_, password_, this);
}

// This method is called if a client is connected to this object.
void SimpleHost::OnClientConnected(ClientConnection* client) {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // Create a new RecordSession if there was none.
  if (!session_) {
    // The first we need to make sure capture and encode thread are
    // running.
    capture_thread_.Start();
    encode_thread_.Start();

    // Then we create a SessionManager passing the message loops that
    // it should run on.
    // Note that we pass the ownership of the capturer and encoder to
    // the session manager.
    DCHECK(capturer_.get());
    DCHECK(encoder_.get());
    session_ = new SessionManager(capture_thread_.message_loop(),
                                  encode_thread_.message_loop(),
                                  &main_loop_,
                                  capturer_.release(),
                                  encoder_.release());

    // Immediately add the client and start the session.
    session_->AddClient(client);
    session_->Start();
    LOG(INFO) << "Session manager started";
  } else {
    // If a session manager already exists we simply add the new client.
    session_->AddClient(client);
  }
}

void SimpleHost::OnClientDisconnected(ClientConnection* client) {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // Remove the client from the session manager.
  DCHECK(session_);
  session_->RemoveClient(client);

  // Also remove reference to ClientConnection from this object.
  client_ = NULL;

  // TODO(hclam): If the last client has disconnected we need destroy
  // the session manager and shutdown the capture and encode threads.
  // Right now we assume there's only one client.
  DestroySession();
}

////////////////////////////////////////////////////////////////////////////
// ClientConnection::EventHandler implementations
void SimpleHost::HandleMessages(ClientConnection* client,
                                ClientMessageList* messages) {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // Delegate the messages to EventExecutor and delete the unhandled
  // messages.
  DCHECK(executor_.get());
  executor_->HandleInputEvents(messages);
  STLDeleteElements<ClientMessageList>(messages);
}

void SimpleHost::OnConnectionOpened(ClientConnection* client) {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // Completes the client connection.
  LOG(INFO) << "Connection to client established.";
  OnClientConnected(client_.get());
}

void SimpleHost::OnConnectionClosed(ClientConnection* client) {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // Completes the client connection.
  LOG(INFO) << "Connection to client closed.";
  OnClientDisconnected(client_.get());
}

void SimpleHost::OnConnectionFailed(ClientConnection* client) {
  DCHECK_EQ(&main_loop_, MessageLoop::current());

  // The client has disconnected.
  LOG(ERROR) << "Connection failed unexpectedly.";
  OnClientDisconnected(client_.get());
}

////////////////////////////////////////////////////////////////////////////
// JingleClient::Callback implementations
void SimpleHost::OnStateChange(JingleClient* jingle_client,
                               JingleClient::State state) {
  DCHECK_EQ(jingle_client_.get(), jingle_client);

  if (state == JingleClient::CONNECTED) {
    // TODO(hclam): Change to use LOG(INFO).
    // LOG(INFO) << "Host connected as "
    //           << jingle_client->GetFullJid() << "." << std::endl;
    printf("Host connected as %s\n", jingle_client->GetFullJid().c_str());

    // Start heartbeating after we connected
    heartbeat_sender_ = new HeartbeatSender();
    // TODO(sergeyu): where do we get host id?
    heartbeat_sender_->Start(jingle_client_.get(), "HostID");
  } else if (state == JingleClient::CLOSED) {
    LOG(INFO) << "Host disconnected from talk network." << std::endl;

    heartbeat_sender_ = NULL;
  }
}

bool SimpleHost::OnAcceptConnection(
    JingleClient* jingle_client, const std::string& jid,
    JingleChannel::Callback** channel_callback) {
  DCHECK_EQ(jingle_client_.get(), jingle_client);

  if (client_.get())
    return false;

  LOG(INFO) << "Client connected: " << jid << std::endl;

  // If we accept the connected then create a client object and set the
  // callback.
  client_ = new ClientConnection(&main_loop_, new ProtocolDecoder(), this);
  *channel_callback = client_.get();
  return true;
}

void SimpleHost::OnNewConnection(JingleClient* jingle_client,
                                 scoped_refptr<JingleChannel> channel) {
  DCHECK_EQ(jingle_client_.get(), jingle_client);

  // Since the session manager has not started, it is still safe to access
  // the client directly. Note that we give the ownership of the channel
  // to the client.
  client_->set_jingle_channel(channel);
}

}  // namespace remoting
