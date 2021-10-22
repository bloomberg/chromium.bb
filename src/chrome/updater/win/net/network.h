// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_NETWORK_H_
#define CHROME_UPDATER_WIN_NET_NETWORK_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"
#include "components/winhttp/scoped_hinternet.h"

namespace winhttp {
class ProxyConfiguration;
}

namespace updater {

class PolicyService;

// Network fetcher factory for WinHTTP.
class NetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  explicit NetworkFetcherFactory(scoped_refptr<PolicyService> policy_service);
  NetworkFetcherFactory(const NetworkFetcherFactory&) = delete;
  NetworkFetcherFactory& operator=(const NetworkFetcherFactory&) = delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactory() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  // Proxy configuration for WinHTTP should be initialized before
  // the session handle.
  scoped_refptr<winhttp::ProxyConfiguration> proxy_configuration_;
  winhttp::ScopedHInternet session_handle_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_NETWORK_H_
