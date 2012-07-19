// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/tools/gdig/file_net_log.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace net {

namespace {

bool StringToIPEndPoint(const std::string& ip_address_and_port,
                        IPEndPoint* ip_end_point) {
  DCHECK(ip_end_point);

  std::string ip;
  int port;
  if (!ParseHostAndPort(ip_address_and_port, &ip, &port))
    return false;
  if (port == -1)
    port = dns_protocol::kDefaultPort;

  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip, &ip_number))
    return false;

  *ip_end_point = net::IPEndPoint(ip_number, port);
  return true;
}

// Convert DnsConfig to human readable text omitting the hosts member.
std::string DnsConfigToString(const DnsConfig& dns_config) {
  std::string output;
  output.append("search ");
  for (size_t i = 0; i < dns_config.search.size(); ++i) {
    output.append(dns_config.search[i] + " ");
  }
  output.append("\n");

  for (size_t i = 0; i < dns_config.nameservers.size(); ++i) {
    output.append("nameserver ");
    output.append(dns_config.nameservers[i].ToString()).append("\n");
  }

  base::StringAppendF(&output, "options ndots:%d\n", dns_config.ndots);
  base::StringAppendF(&output, "options timeout:%d\n",
                      static_cast<int>(dns_config.timeout.InMilliseconds()));
  base::StringAppendF(&output, "options attempts:%d\n", dns_config.attempts);
  if (dns_config.rotate)
    output.append("options rotate\n");
  if (dns_config.edns0)
    output.append("options edns0\n");
  return output;
}

// Convert DnsConfig hosts member to a human readable text.
std::string DnsHostsToString(const DnsHosts& dns_hosts) {
  std::string output;
  for (DnsHosts::const_iterator i = dns_hosts.begin();
       i != dns_hosts.end();
       ++i) {
    const DnsHostsKey& key = i->first;
    std::string host_name = key.first;
    output.append(IPEndPoint(i->second, -1).ToStringWithoutPort());
    output.append(" ").append(host_name).append("\n");
  }
  return output;
}

class GDig {
 public:
  GDig();

  enum Result {
    RESULT_NO_RESOLVE = -3,
    RESULT_NO_CONFIG = -2,
    RESULT_WRONG_USAGE = -1,
    RESULT_OK = 0,
    RESULT_PENDING = 1,
  };

  Result Main(int argc, const char* argv[]);

 private:
  bool ParseCommandLine(int argc, const char* argv[]);

  void Start();
  void Finish(Result);

  void OnDnsConfig(const DnsConfig& dns_config_const);
  void OnResolveComplete(int val);
  void OnTimeout();

  base::TimeDelta config_timeout_;
  std::string domain_name_;
  bool print_config_;
  bool print_hosts_;
  net::IPEndPoint nameserver_;
  base::TimeDelta timeout_;

  Result result_;
  AddressList addrlist_;

  base::CancelableClosure timeout_closure_;
  scoped_ptr<DnsConfigService> dns_config_service_;
  scoped_ptr<FileNetLog> log_;
  scoped_ptr<HostResolver> resolver_;
};

GDig::GDig()
    : config_timeout_(base::TimeDelta::FromSeconds(5)),
      print_config_(false),
      print_hosts_(false) {
}

GDig::Result GDig::Main(int argc, const char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
      fprintf(stderr,
              "usage: %s [--net_log[=<basic|no_bytes|all>]]"
              " [--print_config] [--print_hosts]"
              " [--nameserver=<ip_address[:port]>]"
              " [--timeout=<milliseconds>] [--config_timeout=<seconds>]"
              " domain_name\n",
              argv[0]);
      return RESULT_WRONG_USAGE;
  }

#if defined(OS_MACOSX)
  // Without this there will be a mem leak on osx.
  base::mac::ScopedNSAutoreleasePool scoped_pool;
#endif

  base::AtExitManager exit_manager;
  MessageLoopForIO loop;

  result_ = RESULT_PENDING;
  Start();
  if (result_ == RESULT_PENDING)
    MessageLoop::current()->Run();

  // Destroy it while MessageLoopForIO is alive.
  dns_config_service_.reset();
  return result_;
}

bool GDig::ParseCommandLine(int argc, const char* argv[]) {
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  if (parsed_command_line.HasSwitch("config_timeout")) {
    int timeout_seconds = 0;
    bool parsed = base::StringToInt(
        parsed_command_line.GetSwitchValueASCII("config_timeout"),
        &timeout_seconds);
    if (parsed && timeout_seconds > 0) {
      config_timeout_ = base::TimeDelta::FromSeconds(timeout_seconds);
    } else {
      fprintf(stderr, "Invalid config_timeout parameter\n");
      return false;
    }
  }

  if (parsed_command_line.HasSwitch("net_log")) {
    std::string log_param = parsed_command_line.GetSwitchValueASCII("net_log");
    NetLog::LogLevel level = NetLog::LOG_ALL_BUT_BYTES;

    if (log_param.length() > 0) {
      std::map<std::string, NetLog::LogLevel> log_levels;
      log_levels["all"] = NetLog::LOG_ALL;
      log_levels["no_bytes"] = NetLog::LOG_ALL_BUT_BYTES;
      log_levels["basic"] = NetLog::LOG_BASIC;

      if (log_levels.find(log_param) != log_levels.end()) {
        level = log_levels[log_param];
      } else {
        fprintf(stderr, "Invalid net_log parameter\n");
        return false;
      }
    }
    log_.reset(new FileNetLog(stderr, level));
  }

  print_config_ = parsed_command_line.HasSwitch("print_config");
  print_hosts_ = parsed_command_line.HasSwitch("print_hosts");

  if (parsed_command_line.HasSwitch("nameserver")) {
    std::string nameserver =
      parsed_command_line.GetSwitchValueASCII("nameserver");
    if (!StringToIPEndPoint(nameserver, &nameserver_)) {
      fprintf(stderr,
              "Cannot parse the namerserver string into an IPEndPoint\n");
      return false;
    }
  }

  if (parsed_command_line.HasSwitch("timeout")) {
    int timeout_millis = 0;
    bool parsed = base::StringToInt(
        parsed_command_line.GetSwitchValueASCII("timeout"),
        &timeout_millis);
    if (parsed && timeout_millis > 0) {
      timeout_ = base::TimeDelta::FromMilliseconds(timeout_millis);
    } else {
      fprintf(stderr, "Invalid timeout parameter\n");
      return false;
    }
  }

  if (parsed_command_line.GetArgs().size() == 1) {
#if defined(OS_WIN)
    domain_name_ = WideToASCII(parsed_command_line.GetArgs()[0]);
#else
    domain_name_ = parsed_command_line.GetArgs()[0];
#endif
  } else if (parsed_command_line.GetArgs().size() != 0) {
    return false;
  }
  return print_config_ || print_hosts_ || domain_name_.length() > 0;
}

void GDig::Start() {
  if (nameserver_.address().size() > 0) {
    DnsConfig dns_config;
    dns_config.attempts = 1;
    dns_config.nameservers.push_back(nameserver_);
    OnDnsConfig(dns_config);
  } else {
    dns_config_service_ = DnsConfigService::CreateSystemService();
    dns_config_service_->Read(base::Bind(&GDig::OnDnsConfig,
                                         base::Unretained(this)));
    timeout_closure_.Reset(base::Bind(&GDig::OnTimeout,
                                      base::Unretained(this)));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        timeout_closure_.callback(),
        config_timeout_);
  }
}

void GDig::Finish(Result result) {
  DCHECK_NE(RESULT_PENDING, result);
  result_ = result;
  if (MessageLoop::current())
    MessageLoop::current()->Quit();
}

void GDig::OnDnsConfig(const DnsConfig& dns_config_const) {
  timeout_closure_.Cancel();
  DCHECK(dns_config_const.IsValid());
  DnsConfig dns_config = dns_config_const;

  if (timeout_.InMilliseconds() > 0)
    dns_config.timeout = timeout_;
  if (print_config_)
    printf("# Dns Configuration\n"
           "%s", DnsConfigToString(dns_config).c_str());
  if (print_hosts_)
    printf("# Host Database\n"
           "%s", DnsHostsToString(dns_config.hosts).c_str());

  // If the user didn't specify a name to resolve we can stop here.
  if (domain_name_.length() == 0) {
    Finish(RESULT_OK);
    return;
  }

  scoped_ptr<DnsClient> dns_client(DnsClient::CreateClient(NULL));
  dns_client->SetConfig(dns_config);
  resolver_.reset(
      new HostResolverImpl(
          HostCache::CreateDefaultCache(),
          PrioritizedDispatcher::Limits(NUM_PRIORITIES, 1),
          HostResolverImpl::ProcTaskParams(NULL, 1),
          scoped_ptr<DnsConfigService>(NULL),
          dns_client.Pass(),
          log_.get()));

  HostResolver::RequestInfo info(HostPortPair(domain_name_.c_str(), 80));

  CompletionCallback callback = base::Bind(&GDig::OnResolveComplete,
                                           base::Unretained(this));
  int ret = resolver_->Resolve(
      info, &addrlist_, callback, NULL,
      BoundNetLog::Make(log_.get(), net::NetLog::SOURCE_NONE));
  switch (ret) {
  case OK:
    OnResolveComplete(ret);
    break;
  case ERR_IO_PENDING: break;
  default:
    Finish(RESULT_NO_RESOLVE);
    fprintf(stderr, "Error calling resolve  %s\n", ErrorToString(ret));
  }
}

void GDig::OnResolveComplete(int val) {
  if (val != OK) {
    fprintf(stderr, "Error trying to resolve hostname %s: %s\n",
            domain_name_.c_str(), ErrorToString(val));
    Finish(RESULT_NO_RESOLVE);
  } else {
    for (size_t i = 0; i < addrlist_.size(); ++i)
      printf("%s\n", addrlist_[i].ToStringWithoutPort().c_str());
    Finish(RESULT_OK);
  }
}

void GDig::OnTimeout() {
  fprintf(stderr, "Timed out waiting to load the dns config\n");
  Finish(RESULT_NO_CONFIG);
}

}  // empty namespace

}  // namespace net

int main(int argc, const char* argv[]) {
  net::GDig dig;
  return dig.Main(argc, argv);
}
