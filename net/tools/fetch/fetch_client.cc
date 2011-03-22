// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"

void usage(const char* program_name) {
  printf("usage: %s --url=<url>  [--n=<clients>] [--stats] [--use_cache]\n",
         program_name);
  exit(1);
}

// Test Driver
class Driver {
 public:
  Driver()
      : clients_(0) {}

  void ClientStarted() { clients_++; }
  void ClientStopped() {
    if (!--clients_) {
      MessageLoop::current()->Quit();
    }
  }

 private:
  int clients_;
};

static base::LazyInstance<Driver> g_driver(base::LINKER_INITIALIZED);

// A network client
class Client {
 public:
  Client(net::HttpTransactionFactory* factory, const std::string& url) :
      url_(url),
      buffer_(new net::IOBuffer(kBufferSize)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &Client::OnConnectComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &Client::OnReadComplete)) {
    int rv = factory->CreateTransaction(&transaction_);
    DCHECK_EQ(net::OK, rv);
    buffer_->AddRef();
    g_driver.Get().ClientStarted();
    request_info_.url = url_;
    request_info_.method = "GET";
    int state = transaction_->Start(
        &request_info_, &connect_callback_, net::BoundNetLog());
    DCHECK(state == net::ERR_IO_PENDING);
  };

 private:
  void OnConnectComplete(int result) {
    // Do work here.
    int state = transaction_->Read(buffer_.get(), kBufferSize, &read_callback_);
    if (state == net::ERR_IO_PENDING)
      return;  // IO has started.
    if (state < 0)
      return;  // ERROR!
    OnReadComplete(state);
  }

  void OnReadComplete(int result) {
    if (result == 0) {
      OnRequestComplete(result);
      return;
    }

    // Deal with received data here.
    base::StatsCounter bytes_read("FetchClient.bytes_read");
    bytes_read.Add(result);

    // Issue a read for more data.
    int state = transaction_->Read(buffer_.get(), kBufferSize, &read_callback_);
    if (state == net::ERR_IO_PENDING)
      return;  // IO has started.
    if (state < 0)
      return;  // ERROR!
    OnReadComplete(state);
  }

  void OnRequestComplete(int result) {
    base::StatsCounter requests("FetchClient.requests");
    requests.Increment();
    g_driver.Get().ClientStopped();
    printf(".");
  }

  static const int kBufferSize = (16 * 1024);
  GURL url_;
  net::HttpRequestInfo request_info_;
  scoped_ptr<net::HttpTransaction> transaction_;
  scoped_refptr<net::IOBuffer> buffer_;
  net::CompletionCallbackImpl<Client> connect_callback_;
  net::CompletionCallbackImpl<Client> read_callback_;
};

int main(int argc, char**argv) {
  base::AtExitManager exit;
  base::StatsTable table("fetchclient", 50, 1000);
  table.set_current(&table);

  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::string url = parsed_command_line.GetSwitchValueASCII("url");
  if (!url.length())
    usage(argv[0]);
  int client_limit = 1;
  if (parsed_command_line.HasSwitch("n")) {
    base::StringToInt(parsed_command_line.GetSwitchValueASCII("n"),
                      &client_limit);
  }
  bool use_cache = parsed_command_line.HasSwitch("use-cache");

  // Do work here.
  MessageLoop loop(MessageLoop::TYPE_IO);

  scoped_ptr<net::HostResolver> host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL));

  scoped_ptr<net::CertVerifier> cert_verifier(new net::CertVerifier);
  scoped_refptr<net::ProxyService> proxy_service(
      net::ProxyService::CreateDirect());
  scoped_refptr<net::SSLConfigService> ssl_config_service(
      net::SSLConfigService::CreateSystemSSLConfigService());
  net::HttpTransactionFactory* factory = NULL;
  scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver.get()));

  net::HttpNetworkSession::Params session_params;
  session_params.host_resolver = host_resolver.get();
  session_params.cert_verifier = cert_verifier.get();
  session_params.proxy_service = proxy_service;
  session_params.http_auth_handler_factory = http_auth_handler_factory.get();
  session_params.ssl_config_service = ssl_config_service;

  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  if (use_cache) {
    factory = new net::HttpCache(network_session,
                                 net::HttpCache::DefaultBackend::InMemory(0));
  } else {
    factory = new net::HttpNetworkLayer(network_session);
  }

  {
    base::StatsCounterTimer driver_time("FetchClient.total_time");
    base::StatsScope<base::StatsCounterTimer> scope(driver_time);

    Client** clients = new Client*[client_limit];
    for (int i = 0; i < client_limit; i++)
      clients[i] = new Client(factory, url);

    MessageLoop::current()->Run();
  }

  // Print Statistics here.
  int num_clients = table.GetCounterValue("c:FetchClient.requests");
  int test_time = table.GetCounterValue("t:FetchClient.total_time");
  int bytes_read = table.GetCounterValue("c:FetchClient.bytes_read");

  printf("\n");
  printf("Clients     : %d\n", num_clients);
  printf("Time        : %dms\n", test_time);
  printf("Bytes Read  : %d\n", bytes_read);
  if (test_time > 0) {
    const char *units = "bps";
    double bps = static_cast<float>(bytes_read * 8) /
        (static_cast<float>(test_time) / 1000.0);

    if (bps > (1024*1024)) {
      bps /= (1024*1024);
      units = "Mbps";
    } else if (bps > 1024) {
      bps /= 1024;
      units = "Kbps";
    }
    printf("Bandwidth   : %.2f%s\n", bps, units);
  }

  if (parsed_command_line.HasSwitch("stats")) {
    // Dump the stats table.
    printf("<stats>\n");
    int counter_max = table.GetMaxCounters();
    for (int index = 0; index < counter_max; index++) {
      std::string name(table.GetRowName(index));
      if (name.length() > 0) {
        int value = table.GetRowValue(index);
        printf("%s:\t%d\n", name.c_str(), value);
      }
    }
    printf("</stats>\n");
  }
  return 0;
}
