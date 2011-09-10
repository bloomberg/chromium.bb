// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if !defined(OS_WIN)
extern "C" {
#include <unistd.h>
}
#endif  // !defined(OS_WIN)

#include <iostream>
#include <list>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/time.h"
#include "crypto/nss_util.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/jingle_session_manager.h"

namespace remoting {
namespace protocol {

namespace {
const int kBufferSize = 4096;
const char kDummyAuthToken[] = "";
}  // namespace

class ProtocolTestClient;

class ProtocolTestConnection
    : public base::RefCountedThreadSafe<ProtocolTestConnection> {
 public:
  ProtocolTestConnection(ProtocolTestClient* client)
      : client_(client),
        message_loop_(MessageLoop::current()),
        session_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &ProtocolTestConnection::OnWritten)),
        pending_write_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &ProtocolTestConnection::OnRead)) {
  }

  virtual ~ProtocolTestConnection() {}

  void Init(Session* session);
  void Write(const std::string& str);
  void Read();
  void Close();

  // Session::Callback interface.
  virtual void OnStateChange(Session::State state);
 private:
  void DoWrite(scoped_refptr<net::IOBuffer> buf, int size);
  void DoRead();

  void HandleReadResult(int result);

  void OnWritten(int result);
  void OnRead(int result);

  ProtocolTestClient* client_;
  MessageLoop* message_loop_;
  scoped_ptr<Session> session_;
  net::CompletionCallbackImpl<ProtocolTestConnection> write_cb_;
  bool pending_write_;
  net::CompletionCallbackImpl<ProtocolTestConnection> read_cb_;
  scoped_refptr<net::IOBuffer> read_buffer_;
};

class ProtocolTestClient
    : public SignalStrategy::StatusObserver,
      public SessionManager::Listener,
      public base::RefCountedThreadSafe<ProtocolTestClient> {
 public:
  ProtocolTestClient() {
  }

  virtual ~ProtocolTestClient() {}

  void Run(const std::string& username, const std::string& auth_token,
           const std::string& auth_service, const std::string& host_jid);

  void OnConnectionClosed(ProtocolTestConnection* connection);

  // SignalStrategy::StatusObserver interface.
  virtual void OnStateChange(
      SignalStrategy::StatusObserver::State state) OVERRIDE;
  virtual void OnJidChange(const std::string& full_jid) OVERRIDE;

  // SessionManager::Listener interface.
  virtual void OnSessionManagerInitialized() OVERRIDE;
  virtual void OnIncomingSession(
      Session* session,
      SessionManager::IncomingSessionResponse* response) OVERRIDE;

 private:
  typedef std::list<scoped_refptr<ProtocolTestConnection> > ConnectionsList;

  std::string host_jid_;
  scoped_ptr<SignalStrategy> signal_strategy_;
  std::string local_jid_;
  scoped_ptr<JingleSessionManager> session_manager_;
  ConnectionsList connections_;
  base::Lock connections_lock_;
};


void ProtocolTestConnection::Init(Session* session) {
  session_.reset(session);
}

void ProtocolTestConnection::Write(const std::string& str) {
  if (str.empty())
    return;

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(str.length()));
  memcpy(buf->data(), str.c_str(), str.length());
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(
          this, &ProtocolTestConnection::DoWrite, buf, str.length()));
}

void ProtocolTestConnection::DoWrite(
    scoped_refptr<net::IOBuffer> buf, int size) {
  if (pending_write_) {
    LOG(ERROR) << "Cannot write because there is another pending write.";
    return;
  }

  net::Socket* channel = session_->event_channel();
  if (channel != NULL) {
    int result = channel->Write(buf, size, &write_cb_);
    if (result < 0) {
      if (result == net::ERR_IO_PENDING)
        pending_write_ = true;
      else
        LOG(ERROR) << "Write() returned error " << result;
    }
  } else {
    LOG(ERROR) << "Cannot write because the channel isn't intialized yet.";
  }
}

void ProtocolTestConnection::Read() {
  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(
          this, &ProtocolTestConnection::DoRead));
}

void ProtocolTestConnection::DoRead() {
  read_buffer_ = new net::IOBuffer(kBufferSize);
  while (true) {
    int result = session_->event_channel()->Read(
        read_buffer_, kBufferSize, &read_cb_);
    if (result < 0) {
      if (result != net::ERR_IO_PENDING)
        LOG(ERROR) << "Read failed: " << result;
      break;
    } else {
      HandleReadResult(result);
    }
  }
}

void ProtocolTestConnection::Close() {
  session_->Close();
}

void ProtocolTestConnection::OnStateChange(Session::State state) {
  LOG(INFO) << "State of " << session_->jid() << " changed to " << state;
  if (state == Session::CONNECTED) {
    // Start reading after we've connected.
    Read();
  } else if (state == Session::CLOSED) {
    std::cerr << "Connection to " << session_->jid()
              << " closed" << std::endl;
    client_->OnConnectionClosed(this);
  }
}

void ProtocolTestConnection::OnWritten(int result) {
  pending_write_ = false;
  if (result < 0)
      LOG(ERROR) << "Write() returned error " << result;
}

void ProtocolTestConnection::OnRead(int result) {
  HandleReadResult(result);
  DoRead();
}

void ProtocolTestConnection::HandleReadResult(int result) {
  if (result > 0) {
    std::string str(reinterpret_cast<const char*>(read_buffer_->data()),
                    result);
    std::cout << "(" << session_->jid() << "): " << str << std::endl;
  } else {
    LOG(ERROR) << "Read() returned error " << result;
  }
}

void ProtocolTestClient::Run(const std::string& username,
                             const std::string& auth_token,
                             const std::string& auth_service,
                             const std::string& host_jid) {
  remoting::JingleThread jingle_thread;
  jingle_thread.Start();

  signal_strategy_.reset(
      new XmppSignalStrategy(&jingle_thread, username, auth_token,
                             auth_service));
  signal_strategy_->Init(this);
  session_manager_.reset(JingleSessionManager::CreateNotSandboxed(
      jingle_thread.message_loop_proxy()));

  host_jid_ = host_jid;

  while (true) {
    std::string line;
    std::getline(std::cin, line);

    {
      base::AutoLock auto_lock(connections_lock_);

      // Broadcast message to all clients.
      for (ConnectionsList::iterator it = connections_.begin();
           it != connections_.end(); ++it) {
        (*it)->Write(line);
      }
    }

    if (line == "exit")
      break;
  }

  while (!connections_.empty()) {
    connections_.front()->Close();
    connections_.pop_front();
  }

  if (session_manager_.get()) {
    session_manager_->Close();
    session_manager_.reset();
  }

  signal_strategy_->Close();
  jingle_thread.Stop();
}

void ProtocolTestClient::OnConnectionClosed(
    ProtocolTestConnection* connection) {
  connection->Close();
  base::AutoLock auto_lock(connections_lock_);
  for (ConnectionsList::iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    if ((*it) == connection) {
      connections_.erase(it);
      return;
    }
  }
}

void ProtocolTestClient::OnStateChange(
    SignalStrategy::StatusObserver::State state) {
  if (state == SignalStrategy::StatusObserver::CONNECTED) {
    std::cerr << "Connected as " << local_jid_ << std::endl;

    session_manager_->Init(
        local_jid_, signal_strategy_.get(), this, NULL, "", true);
    session_manager_->set_allow_local_ips(true);
  } else if (state == SignalStrategy::StatusObserver::CLOSED) {
    std::cerr << "Connection closed" << std::endl;
  }
}

void ProtocolTestClient::OnJidChange(const std::string& full_jid) {
  local_jid_ = full_jid;
}

void ProtocolTestClient::OnSessionManagerInitialized() {
  if (host_jid_ != "") {
    ProtocolTestConnection* connection = new ProtocolTestConnection(this);
    connection->Init(session_manager_->Connect(
        host_jid_, "", kDummyAuthToken,
        CandidateSessionConfig::CreateDefault(),
        NewCallback(connection, &ProtocolTestConnection::OnStateChange)));
    connections_.push_back(make_scoped_refptr(connection));
  }
}

void ProtocolTestClient::OnIncomingSession(
    Session* session,
    SessionManager::IncomingSessionResponse* response) {
  std::cerr << "Accepting connection from " << session->jid() << std::endl;

  session->set_config(SessionConfig::GetDefault());
  *response = SessionManager::ACCEPT;

  ProtocolTestConnection* test_connection = new ProtocolTestConnection(this);
  session->SetStateChangeCallback(
      NewCallback(test_connection, &ProtocolTestConnection::OnStateChange));
  test_connection->Init(session);
  base::AutoLock auto_lock(connections_lock_);
  connections_.push_back(make_scoped_refptr(test_connection));
}

}  // namespace protocol
}  // namespace remoting

using remoting::protocol::ProtocolTestClient;

void usage(char* command) {
  std::cerr << "Usage: " << command << "--username=<username>" << std::endl
            << "\t--auth_token=<auth_token>" << std::endl
            << "\t[--host_jid=<host_jid>]" << std::endl;
  exit(1);
}

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  if (!cmd_line->GetArgs().empty() || cmd_line->HasSwitch("help"))
    usage(argv[0]);

  base::AtExitManager exit_manager;

  crypto::EnsureNSPRInit();
  crypto::EnsureNSSInit();

#if defined(OS_MACOSX)
  mock_cr_app::RegisterMockCrApp();
#endif  // OS_MACOSX

  std::string host_jid(cmd_line->GetSwitchValueASCII("host_jid"));

  if (!cmd_line->HasSwitch("username"))
    usage(argv[0]);
  std::string username(cmd_line->GetSwitchValueASCII("username"));

  if (!cmd_line->HasSwitch("auth_token"))
    usage(argv[0]);
  std::string auth_token(cmd_line->GetSwitchValueASCII("auth_token"));

  // Default to OAuth2 for the auth token.
  std::string auth_service("oauth2");
  if (cmd_line->HasSwitch("auth_service"))
    auth_service = cmd_line->GetSwitchValueASCII("auth_service");

  scoped_refptr<ProtocolTestClient> client(new ProtocolTestClient());

  client->Run(username, auth_token, host_jid, auth_service);

  return 0;
}
