// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "base/command_line.h"
#include "base/nss_util.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/protocol/jingle_session_manager.h"

using remoting::kChromotingTokenServiceName;

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
  ProtocolTestConnection(ProtocolTestClient* client, MessageLoop* message_loop)
      : client_(client),
        message_loop_(message_loop),
        session_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            write_cb_(this, &ProtocolTestConnection::OnWritten)),
        pending_write_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            read_cb_(this, &ProtocolTestConnection::OnRead)),
        closed_event_(true, false) {
  }

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

  void OnFinishedClosing();

  ProtocolTestClient* client_;
  MessageLoop* message_loop_;
  scoped_refptr<Session> session_;
  net::CompletionCallbackImpl<ProtocolTestConnection> write_cb_;
  bool pending_write_;
  net::CompletionCallbackImpl<ProtocolTestConnection> read_cb_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  base::WaitableEvent closed_event_;
};

class ProtocolTestClient
    : public JingleClient::Callback,
      public base::RefCountedThreadSafe<ProtocolTestClient> {
 public:
  ProtocolTestClient()
      : closed_event_(true, false) {
  }

  virtual ~ProtocolTestClient() {}

  void Run(const std::string& username, const std::string& auth_token,
           const std::string& host_jid);

  void OnConnectionClosed(ProtocolTestConnection* connection);

  // JingleClient::Callback interface.
  virtual void OnStateChange(JingleClient* client, JingleClient::State state);

  // callback for JingleSessionManager interface.
  virtual void OnNewSession(
      Session* session,
      SessionManager::IncomingSessionResponse* response);

 private:
  typedef std::list<scoped_refptr<ProtocolTestConnection> > ConnectionsList;

  void OnFinishedClosing();
  void DestroyConnection(scoped_refptr<ProtocolTestConnection> connection);

  std::string host_jid_;
  scoped_refptr<JingleClient> client_;
  scoped_refptr<JingleSessionManager> session_manager_;
  ConnectionsList connections_;
  base::Lock connections_lock_;
  base::WaitableEvent closed_event_;
};


void ProtocolTestConnection::Init(Session* session) {
  session_ = session;
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
  session_->Close(
      NewRunnableMethod(this, &ProtocolTestConnection::OnFinishedClosing));
  closed_event_.Wait();
}

void ProtocolTestConnection::OnFinishedClosing() {
  closed_event_.Signal();
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
                             const std::string& host_jid) {
  remoting::JingleThread jingle_thread;
  jingle_thread.Start();
  client_ = new JingleClient(&jingle_thread);
  client_->Init(username, auth_token, kChromotingTokenServiceName, this);

  session_manager_ = new JingleSessionManager(&jingle_thread);

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

  if (session_manager_) {
    session_manager_->Close(
        NewRunnableMethod(this, &ProtocolTestClient::OnFinishedClosing));
    closed_event_.Wait();
  }

  client_->Close();
  jingle_thread.Stop();
}

void ProtocolTestClient::OnConnectionClosed(
    ProtocolTestConnection* connection) {
  client_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(
          this, &ProtocolTestClient::DestroyConnection,
          scoped_refptr<ProtocolTestConnection>(connection)));
}

void ProtocolTestClient::OnStateChange(
    JingleClient* client, JingleClient::State state) {
  if (state == JingleClient::CONNECTED) {
    std::cerr << "Connected as " << client->GetFullJid() << std::endl;

    session_manager_->Init(
        client_->GetFullJid(), client_->session_manager(),
        NewCallback(this, &ProtocolTestClient::OnNewSession));
    session_manager_->set_allow_local_ips(true);

    if (host_jid_ != "") {
      ProtocolTestConnection* connection =
          new ProtocolTestConnection(this, client_->message_loop());
      connection->Init(session_manager_->Connect(
          host_jid_, kDummyAuthToken, CandidateSessionConfig::CreateDefault(),
          NewCallback(connection,
                      &ProtocolTestConnection::OnStateChange)));
      connections_.push_back(make_scoped_refptr(connection));
    }
  } else if (state == JingleClient::CLOSED) {
    std::cerr << "Connection closed" << std::endl;
  }
}

void ProtocolTestClient::OnNewSession(
    Session* session,
    SessionManager::IncomingSessionResponse* response) {
  std::cerr << "Accepting connection from " << session->jid() << std::endl;

  session->set_config(SessionConfig::CreateDefault());
  *response = SessionManager::ACCEPT;

  ProtocolTestConnection* test_connection =
      new ProtocolTestConnection(this, client_->message_loop());
  session->SetStateChangeCallback(
      NewCallback(test_connection, &ProtocolTestConnection::OnStateChange));
  test_connection->Init(session);
  base::AutoLock auto_lock(connections_lock_);
  connections_.push_back(make_scoped_refptr(test_connection));
}

void ProtocolTestClient::OnFinishedClosing() {
  closed_event_.Signal();
}

void ProtocolTestClient::DestroyConnection(
    scoped_refptr<ProtocolTestConnection> connection) {
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

  if (!cmd_line->args().empty() || cmd_line->HasSwitch("help"))
    usage(argv[0]);

  base::AtExitManager exit_manager;

  base::EnsureNSPRInit();
  base::EnsureNSSInit();

  std::string host_jid(cmd_line->GetSwitchValueASCII("host_jid"));

  if (!cmd_line->HasSwitch("username"))
    usage(argv[0]);
  std::string username(cmd_line->GetSwitchValueASCII("username"));

  if (!cmd_line->HasSwitch("auth_token"))
    usage(argv[0]);
  std::string auth_token(cmd_line->GetSwitchValueASCII("auth_token"));

  scoped_refptr<ProtocolTestClient> client(new ProtocolTestClient());

  client->Run(username, auth_token, host_jid);

  return 0;
}
