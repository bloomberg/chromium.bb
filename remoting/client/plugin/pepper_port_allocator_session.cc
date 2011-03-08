// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_port_allocator_session.h"

#include <map>

#include "base/callback.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "remoting/jingle_glue/http_port_allocator.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_entrypoints.h"
#include "remoting/client/plugin/pepper_util.h"

namespace {

static const int kHostPort = 80;
static const int kNumRetries = 5;
static const std::string kCreateSessionURL = "/create_session";

// Define a SessionFactory in the anonymouse namespace so we have a
// shorter name.
// TODO(hclam): Move this to a separate file.
class SessionFactory : public remoting::PortAllocatorSessionFactory {
 public:
  SessionFactory(remoting::ChromotingInstance* instance,
                 MessageLoop* message_loop)
      : instance_(instance),
        jingle_message_loop_(message_loop) {
  }

  virtual cricket::PortAllocatorSession* CreateSession(
      cricket::BasicPortAllocator* allocator,
      const std::string& name,
      const std::string& session_type,
      const std::vector<talk_base::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const std::string& agent) {
    return new remoting::PepperPortAllocatorSession(
        instance_, jingle_message_loop_, allocator, name, session_type,
        stun_hosts, relay_hosts, relay, agent);
  }

 private:
  remoting::ChromotingInstance* instance_;

  // Message loop that jingle runs on.
  MessageLoop* jingle_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(SessionFactory);
};

typedef Callback3<bool, int, const std::string&>::Type FetchCallback;

// Parses the lines in the result of the HTTP request that are of the form
// 'a=b' and returns them in a map.
typedef std::map<std::string, std::string> StringMap;
void ParseMap(const std::string& string, StringMap& map) {
  size_t start_of_line = 0;
  size_t end_of_line = 0;

  for (;;) {  // for each line
    start_of_line = string.find_first_not_of("\r\n", end_of_line);
    if (start_of_line == std::string::npos)
      break;

    end_of_line = string.find_first_of("\r\n", start_of_line);
    if (end_of_line == std::string::npos) {
      end_of_line = string.length();
    }

    size_t equals = string.find('=', start_of_line);
    if ((equals >= end_of_line) || (equals == std::string::npos))
      continue;

    std::string key(string, start_of_line, equals - start_of_line);
    std::string value(string, equals + 1, end_of_line - equals - 1);

    TrimString(key, " \t\r\n", &key);
    TrimString(value, " \t\r\n", &value);

    if ((key.size() > 0) && (value.size() > 0))
      map[key] = value;
  }
}

}  // namespace

namespace remoting {

// A URL Fetcher using Pepper.
// TODO(hclam): Move this to a separate file.
class PepperURLFetcher {
 public:
  PepperURLFetcher() : fetch_callback_(NULL) {
    callback_factory_.Initialize(this);
  }

  void Start(const pp::Instance& instance,
             pp::URLRequestInfo request,
             FetchCallback* fetch_callback) {
    loader_ = pp::URLLoader(instance);

    // Grant access to external origins.
    const struct PPB_URLLoaderTrusted* trusted_loader_interface =
        reinterpret_cast<const PPB_URLLoaderTrusted*>(
            PPP_GetBrowserInterface(PPB_URLLOADERTRUSTED_INTERFACE));
    trusted_loader_interface->GrantUniversalAccess(
        loader_.pp_resource());

    fetch_callback_.reset(fetch_callback);

    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&PepperURLFetcher::DidOpen);
    int rv = loader_.Open(request, callback);
    if (rv != PP_ERROR_WOULDBLOCK)
      callback.Run(rv);
  }

 private:
  void ReadMore() {
    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&PepperURLFetcher::DidRead);
    int rv = loader_.ReadResponseBody(buf_, sizeof(buf_), callback);
    if (rv != PP_ERROR_WOULDBLOCK)
      callback.Run(rv);
  }

  void DidOpen(int32_t result) {
    if (result == PP_OK) {
      ReadMore();
    } else {
      DidFinish(result);
    }
  }

  void DidRead(int32_t result) {
    if (result > 0) {
      data_.append(buf_, result);
      ReadMore();
    } else {
      DidFinish(result);
    }
  }

  void DidFinish(int32_t result) {
    if (fetch_callback_.get()) {
      bool success = result == PP_OK;
      int status_code = 0;
      if (success)
        status_code = loader_.GetResponseInfo().GetStatusCode();
      fetch_callback_->Run(success, status_code, data_);
    }
  }

  pp::CompletionCallbackFactory<PepperURLFetcher> callback_factory_;
  pp::URLLoader loader_;
  scoped_ptr<FetchCallback> fetch_callback_;
  char buf_[4096];
  std::string data_;
};

// A helper function to destruct |fetcher| on pepper thread.
static void DeletePepperURLFetcher(PepperURLFetcher* fetcher) {
  delete fetcher;
}

// A helper class to do HTTP request on the pepper thread and then delegate the
// result to PepperPortAllocatorSession on jingle thread safely.
class PepperCreateSessionTask
    : public base::RefCountedThreadSafe<PepperCreateSessionTask> {
 public:
  PepperCreateSessionTask(
      MessageLoop* jingle_message_loop,
      PepperPortAllocatorSession* allocator_session,
      ChromotingInstance* instance,
      const std::string& host,
      int port,
      const std::string& relay_token,
      const std::string& session_type,
      const std::string& name)
      : jingle_message_loop_(jingle_message_loop),
        allocator_session_(allocator_session),
        instance_(instance),
        host_(host),
        port_(port),
        relay_token_(relay_token),
        session_type_(session_type),
        name_(name) {
  }

  // Start doing the request. The request will start on the pepper thread.
  void Start() {
    if (!CurrentlyOnPluginThread()) {
      RunTaskOnPluginThread(
          NewRunnableMethod(this, &PepperCreateSessionTask::Start));
      return;
    }

    // Perform the request here.
    std::string url = base::StringPrintf("http://%s:%d/create_session",
                                         host_.c_str(), port_);
    pp::URLRequestInfo request(instance_);
    request.SetURL(url.c_str());
    request.SetMethod("GET");
    request.SetHeaders(base::StringPrintf(
        "X-Talk-Google-Relay-Auth: %s\r\n"
        "X-Google-Relay-Auth: %s\r\n"
        "X-Session-Type: %s\r\n"
        "X-Stream-Type: %s\r\n",
        relay_token_.c_str(), relay_token_.c_str(), session_type_.c_str(),
        name_.c_str()));

    url_fetcher_.reset(new PepperURLFetcher());
    url_fetcher_->Start(
        *instance_, request,
        NewCallback(this, &PepperCreateSessionTask::OnRequestDone));
  }

  // Detach this task. This class will not access PepperPortAllocatorSession
  // anymore.
  void Detach() {
    // Set the pointers to zero.
    {
      base::AutoLock auto_lock(lock_);
      jingle_message_loop_ = NULL;
      allocator_session_ = NULL;
      instance_ = NULL;
    }

    // IMPORTANT!
    // Destroy PepperURLFetcher only on pepper thread.
    RunTaskOnPluginThread(
        NewRunnableFunction(&DeletePepperURLFetcher, url_fetcher_.release()));
  }

 private:
  void OnRequestDone(bool success, int status_code,
                     const std::string& response) {
    // IMPORTANT!
    // This method is called on the pepper thread and we want the response to
    // be delegated to the jingle thread. However jignle thread might have
    // been destroyed and |allocator_session_| might be dangling too. So we
    // put a lock here to access |jingle_message_loop_| and then do the
    // remaining work on the jingle thread.
    base::AutoLock auto_lock(lock_);
    if (!jingle_message_loop_)
      return;

    jingle_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &PepperCreateSessionTask::DelegateRequestDone,
                          success, status_code, response));
  }

  void DelegateRequestDone(bool success, int status_code,
                           const std::string& response) {
    if (!allocator_session_)
      return;
    allocator_session_->OnRequestDone(success, status_code, response);
  }

  // Protects |jingle_message_loop_|.
  base::Lock lock_;

  MessageLoop* jingle_message_loop_;
  PepperPortAllocatorSession* allocator_session_;
  ChromotingInstance* instance_;
  std::string host_;
  int port_;
  std::string relay_token_;
  std::string session_type_;
  std::string name_;

  // Pepper resources for URL fetching.
  scoped_ptr<PepperURLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(PepperCreateSessionTask);
};

PepperPortAllocatorSession::PepperPortAllocatorSession(
    ChromotingInstance* instance,
    MessageLoop* message_loop,
    cricket::BasicPortAllocator* allocator,
    const std::string &name,
    const std::string& session_type,
    const std::vector<talk_base::SocketAddress>& stun_hosts,
    const std::vector<std::string>& relay_hosts,
    const std::string& relay_token,
    const std::string& user_agent)
    : BasicPortAllocatorSession(allocator, name, session_type),
      instance_(instance), jingle_message_loop_(message_loop),
      relay_hosts_(relay_hosts), stun_hosts_(stun_hosts),
      relay_token_(relay_token), agent_(user_agent), attempts_(0) {
}

PepperPortAllocatorSession::~PepperPortAllocatorSession() {
  if (create_session_task_) {
    create_session_task_->Detach();
    create_session_task_ = NULL;
  }
}

void PepperPortAllocatorSession::GetPortConfigurations() {
  // Creating relay sessions can take time and is done asynchronously.
  // Creating stun sessions could also take time and could be done aysnc also,
  // but for now is done here and added to the initial config.  Note any later
  // configs will have unresolved stun ips and will be discarded by the
  // AllocationSequence.
  cricket::PortConfiguration* config =
      new cricket::PortConfiguration(stun_hosts_[0], "", "", "");
  ConfigReady(config);
  TryCreateRelaySession();
}

void PepperPortAllocatorSession::TryCreateRelaySession() {
  if (attempts_ == kNumRetries) {
    LOG(ERROR) << "PepperPortAllocator: maximum number of requests reached; "
               << "giving up on relay.";
    return;
  }

  if (relay_hosts_.size() == 0) {
    LOG(ERROR) << "PepperPortAllocator: no relay hosts configured.";
    return;
  }

  // Choose the next host to try.
  std::string host = relay_hosts_[attempts_ % relay_hosts_.size()];
  attempts_++;
  LOG(INFO) << "PepperPortAllocator: sending to relay host " << host;
  if (relay_token_.empty()) {
    LOG(WARNING) << "No relay auth token found.";
  }

  SendSessionRequest(host, kHostPort);
}

void PepperPortAllocatorSession::SendSessionRequest(const std::string& host,
                                                    int port) {
  // Destroy the old PepperCreateSessionTask first.
  if (create_session_task_) {
    create_session_task_->Detach();
    create_session_task_ = NULL;
  }

  // Construct a new one and start it. OnRequestDone() will be called when
  // task has completed.
  create_session_task_ = new PepperCreateSessionTask(
      jingle_message_loop_, this, instance_, host, port, relay_token_,
      session_type(), name());
  create_session_task_->Start();
}

void PepperPortAllocatorSession::OnRequestDone(bool success,
                                               int status_code,
                                               const std::string& response) {
  DCHECK_EQ(jingle_message_loop_, MessageLoop::current());

  if (!success || status_code != 200) {
    LOG(WARNING) << "PepperPortAllocatorSession: failed.";
    TryCreateRelaySession();
    return;
  }

  LOG(INFO) << "PepperPortAllocatorSession: request succeeded.";
  ReceiveSessionResponse(response);
}

void PepperPortAllocatorSession::ReceiveSessionResponse(
    const std::string& response) {
  StringMap map;
  ParseMap(response, map);

  std::string username = map["username"];
  std::string password = map["password"];
  std::string magic_cookie = map["magic_cookie"];

  std::string relay_ip = map["relay.ip"];
  std::string relay_udp_port = map["relay.udp_port"];
  std::string relay_tcp_port = map["relay.tcp_port"];
  std::string relay_ssltcp_port = map["relay.ssltcp_port"];

  cricket::PortConfiguration* config =
      new cricket::PortConfiguration(stun_hosts_[0], username,
                                     password, magic_cookie);

  cricket::PortConfiguration::PortList ports;
  if (!relay_udp_port.empty()) {
    talk_base::SocketAddress address(relay_ip, atoi(relay_udp_port.c_str()));
    ports.push_back(cricket::ProtocolAddress(address, cricket::PROTO_UDP));
  }
  if (!relay_tcp_port.empty()) {
    talk_base::SocketAddress address(relay_ip, atoi(relay_tcp_port.c_str()));
    ports.push_back(cricket::ProtocolAddress(address, cricket::PROTO_TCP));
  }
  if (!relay_ssltcp_port.empty()) {
    talk_base::SocketAddress address(relay_ip, atoi(relay_ssltcp_port.c_str()));
    ports.push_back(cricket::ProtocolAddress(address, cricket::PROTO_SSLTCP));
  }
  config->AddRelay(ports, 0.0f);
  ConfigReady(config);
}

PortAllocatorSessionFactory* CreatePepperPortAllocatorSessionFactory(
    ChromotingInstance* instance) {
  return new SessionFactory(instance, MessageLoop::current());
}

}  // namespace remoting
