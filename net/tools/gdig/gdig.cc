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
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace net {

namespace {

class GDig {
 public:
  GDig();

  enum Result {
    RESULT_NO_RESOLVE = -3,
    RESULT_NO_CONFIG = -2,
    RESULT_WRONG_USAGE = -1,
    RESULT_OK = 0,
  };

  Result Main(int argc, const char* argv[]);

 private:
  bool ParseCommandLine(int argc, const char* argv[]);

  void Start();

  void OnDnsConfig(const DnsConfig& dns_config);
  void OnResolveComplete(int val);
  void OnTimeout();

  base::TimeDelta timeout_;
  std::string domain_name_;

  Result result_;
  AddressList addrlist_;

  base::CancelableClosure timeout_closure_;
  scoped_ptr<DnsConfigService> dns_config_service_;
  scoped_ptr<HostResolver> resolver_;
};

GDig::GDig()
    : timeout_(base::TimeDelta::FromSeconds(5)),
      result_(GDig::RESULT_OK) {
}

GDig::Result GDig::Main(int argc, const char* argv[]) {
  if (!ParseCommandLine(argc, argv)) {
      fprintf(stderr,
              "usage: %s [--config_timeout=<seconds>] domain_name\n",
              argv[0]);
      return RESULT_WRONG_USAGE;
  }

#if defined(OS_MACOSX)
  // Without this there will be a mem leak on osx.
  base::mac::ScopedNSAutoreleasePool scoped_pool;
#endif

  base::AtExitManager exit_manager;
  MessageLoopForIO loop;

  Start();

  MessageLoop::current()->Run();

  // Destroy it while MessageLoopForIO is alive.
  dns_config_service_.reset();
  return result_;
}

void GDig::OnResolveComplete(int val) {
  MessageLoop::current()->Quit();
  if (val != OK) {
    fprintf(stderr, "Error trying to resolve hostname %s: %s\n",
            domain_name_.c_str(), ErrorToString(val));
    result_ = RESULT_NO_RESOLVE;
  } else {
    for (size_t i = 0; i < addrlist_.size(); ++i)
      printf("%s\n", addrlist_[i].ToStringWithoutPort().c_str());
  }
}

void GDig::OnTimeout() {
  MessageLoop::current()->Quit();
  fprintf(stderr, "Timed out waiting to load the dns config\n");
  result_ = RESULT_NO_CONFIG;
}

void GDig::Start() {
  dns_config_service_ = DnsConfigService::CreateSystemService();
  dns_config_service_->Read(base::Bind(&GDig::OnDnsConfig,
                                       base::Unretained(this)));

  timeout_closure_.Reset(base::Bind(&GDig::OnTimeout, base::Unretained(this)));

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      timeout_closure_.callback(),
      timeout_);
}

void GDig::OnDnsConfig(const DnsConfig& dns_config) {
  timeout_closure_.Cancel();
  DCHECK(dns_config.IsValid());

  scoped_ptr<DnsClient> dns_client(DnsClient::CreateClient(NULL));
  dns_client->SetConfig(dns_config);
  resolver_.reset(
      new HostResolverImpl(
          HostCache::CreateDefaultCache(),
          PrioritizedDispatcher::Limits(NUM_PRIORITIES, 1),
          HostResolverImpl::ProcTaskParams(NULL, 1),
          scoped_ptr<DnsConfigService>(NULL),
          dns_client.Pass(),
          NULL));

  HostResolver::RequestInfo info(HostPortPair(domain_name_.c_str(), 80));

  CompletionCallback callback = base::Bind(&GDig::OnResolveComplete,
                                           base::Unretained(this));
  int ret = resolver_->Resolve(info, &addrlist_, callback, NULL, BoundNetLog());
  DCHECK(ret == ERR_IO_PENDING);
}

bool GDig::ParseCommandLine(int argc, const char* argv[]) {
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  if (parsed_command_line.GetArgs().size() != 1)
    return false;

#if defined(OS_WIN)
  domain_name_ = WideToASCII(parsed_command_line.GetArgs()[0]);
#else
  domain_name_ = parsed_command_line.GetArgs()[0];
#endif

  if (parsed_command_line.HasSwitch("config_timeout")) {
    int timeout_seconds = 0;
    bool parsed = base::StringToInt(
        parsed_command_line.GetSwitchValueASCII("config_timeout"),
        &timeout_seconds);
    if (parsed && timeout_seconds > 0) {
      timeout_ = base::TimeDelta::FromSeconds(timeout_seconds);
    } else {
      fprintf(stderr,
              "Invalid config_timeout parameter, using the default value\n");
    }
  }

  return true;
}

}  // empty namespace

}  // namespace net

int main(int argc, const char* argv[]) {
  net::GDig dig;
  return dig.Main(argc, argv);
}
