// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/url_request_context_getter.h"

#include "base/message_loop/message_loop_proxy.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context_builder.h"
#include "remoting/base/vlog_net_log.h"

#if defined(OS_WIN)
#include "net/proxy/proxy_config_service_win.h"
#elif defined(OS_IOS)
#include "net/proxy/proxy_config_service_ios.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_config_service_mac.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/proxy/proxy_config_service_linux.h"
#endif

namespace remoting {

namespace {

// Config getter that always returns direct settings.
class ProxyConfigServiceDirect : public net::ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) OVERRIDE {
    *config = net::ProxyConfig::CreateDirect();
    return CONFIG_VALID;
  }
};

net::ProxyConfigService* CreateSystemProxyConfigService(
    base::SingleThreadTaskRunner* io_thread_task_runner) {
#if defined(OS_WIN)
  return new net::ProxyConfigServiceWin();
#elif defined(OS_IOS)
    return new net::ProxyConfigServiceIOS();
#elif defined(OS_MACOSX)
  return new net::ProxyConfigServiceMac(io_thread_task_runner);
#elif defined(OS_CHROMEOS)
  NOTREACHED() << "ChromeOS is not a supported target for Chromoting host";
  return NULL;
#elif defined(OS_LINUX)
  // TODO(sergeyu): Currently ProxyConfigServiceLinux depends on
  // base::OneShotTimer that doesn't work properly on main NPAPI
  // thread. Fix that and uncomment the code below.
  //
  // net::ProxyConfigServiceLinux* linux_config_service =
  //     new net::ProxyConfigServiceLinux();
  // linux_config_service->SetupAndFetchInitialConfig(
  //     ui_message_loop_, io_message_loop->message_loop_proxy(),
  //     file_message_loop);

  // return linux_config_service;
  return new ProxyConfigServiceDirect();
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return new ProxyConfigServiceDirect();
#endif
}

}  // namespace

URLRequestContextGetter::URLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : network_task_runner_(network_task_runner) {
  proxy_config_service_.reset(CreateSystemProxyConfigService(
      network_task_runner_.get()));
}

net::URLRequestContext* URLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get()) {
    net::URLRequestContextBuilder builder;
    builder.set_net_log(new VlogNetLog());
    builder.DisableHttpCache();
    builder.set_proxy_config_service(proxy_config_service_.release());
    url_request_context_.reset(builder.Build());
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

URLRequestContextGetter::~URLRequestContextGetter() {
}

}  // namespace remoting
