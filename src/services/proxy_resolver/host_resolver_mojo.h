// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_
#define SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class AddressList;
class NetLogWithSource;
}  // namespace net

namespace proxy_resolver {

// A HostResolver implementation that converts requests to mojo types and
// forwards them to a mojo Impl interface.
class HostResolverMojo : public net::HostResolver {
 public:
  class Impl {
   public:
    virtual ~Impl() = default;
    virtual void ResolveDns(
        std::unique_ptr<net::HostResolver::RequestInfo> request_info,
        mojom::HostResolverRequestClientPtr) = 0;
  };

  // |impl| must outlive |this|.
  explicit HostResolverMojo(Impl* impl);
  ~HostResolverMojo() override;

  // HostResolver overrides.
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const net::HostPortPair& host,
      const net::NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;
  // Note: |Resolve()| currently ignores |priority|.
  int Resolve(const RequestInfo& info,
              net::RequestPriority priority,
              net::AddressList* addresses,
              net::CompletionOnceCallback callback,
              std::unique_ptr<Request>* request,
              const net::NetLogWithSource& source_net_log) override;
  int ResolveFromCache(const RequestInfo& info,
                       net::AddressList* addresses,
                       const net::NetLogWithSource& source_net_log) override;
  int ResolveStaleFromCache(
      const RequestInfo& info,
      net::AddressList* addresses,
      net::HostCache::EntryStaleness* stale_info,
      const net::NetLogWithSource& source_net_log) override;
  net::HostCache* GetHostCache() override;
  bool HasCached(base::StringPiece hostname,
                 net::HostCache::Entry::Source* source_out,
                 net::HostCache::EntryStaleness* stale_out) const override;

 private:
  class Job;
  class RequestImpl;

  int ResolveFromCacheInternal(const RequestInfo& info,
                               const net::HostCache::Key& key,
                               net::AddressList* addresses);

  Impl* const impl_;

  std::unique_ptr<net::HostCache> host_cache_;
  base::WeakPtrFactory<net::HostCache> host_cache_weak_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverMojo);
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_HOST_RESOLVER_MOJO_H_
