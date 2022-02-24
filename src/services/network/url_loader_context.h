// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_CONTEXT_H_
#define SERVICES_NETWORK_URL_LOADER_CONTEXT_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "services/network/public/cpp/corb/corb_api.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace network {

class ResourceSchedulerClient;

namespace cors {
class OriginAccessList;
}  // namespace cors

namespace mojom {
class CookieAccessObserver;
class CrossOriginEmbedderPolicyReporter;
class DevToolsObserver;
class NetworkContextClient;
class OriginPolicyManager;
class TrustedURLLoaderHeaderClient;
class URLLoaderFactoryParams;
class URLLoaderNetworkServiceObserver;
}  // namespace mojom

// An interface implemented in production code by network::URLLoaderFactory (or
// by URLLoaderContextForTests in unit tests).
class COMPONENT_EXPORT(NETWORK_SERVICE) URLLoaderContext {
 public:
  virtual bool ShouldRequireNetworkIsolationKey() const = 0;
  virtual const cors::OriginAccessList& GetOriginAccessList() const = 0;
  virtual const mojom::URLLoaderFactoryParams& GetFactoryParams() const = 0;
  virtual mojom::CookieAccessObserver* GetCookieAccessObserver() const = 0;
  virtual mojom::CrossOriginEmbedderPolicyReporter* GetCoepReporter() const = 0;
  virtual mojom::DevToolsObserver* GetDevToolsObserver() const = 0;
  virtual mojom::NetworkContextClient* GetNetworkContextClient() const = 0;
  virtual mojom::OriginPolicyManager* GetOriginPolicyManager() const = 0;
  virtual mojom::TrustedURLLoaderHeaderClient* GetUrlLoaderHeaderClient()
      const = 0;
  virtual mojom::URLLoaderNetworkServiceObserver*
  GetURLLoaderNetworkServiceObserver() const = 0;
  virtual net::URLRequestContext* GetUrlRequestContext() const = 0;
  virtual scoped_refptr<ResourceSchedulerClient> GetResourceSchedulerClient()
      const = 0;

  // TODO(https://crbug.com/1276591): Remove GetFactoryId once
  // kOptimizeUpdateLoadInfo is enabled.
  virtual uintptr_t GetFactoryId() const = 0;
  virtual corb::PerFactoryState& GetMutableCorbState() = 0;

 protected:
  // `protected` destructor = can only destruct via concrete implementations
  // (and not via an URLLoaderContext pointer).
  virtual ~URLLoaderContext() {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_CONTEXT_H_
