// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/host_resolver_mojo.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"

namespace proxy_resolver {
namespace {

// Default TTL for successful host resolutions.
constexpr auto kCacheEntryTTL = base::TimeDelta::FromSeconds(5);

// Default TTL for unsuccessful host resolutions.
constexpr auto kNegativeCacheEntryTTL = base::TimeDelta();

net::HostCache::Key CacheKeyForRequest(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation) {
  net::AddressFamily address_family = net::ADDRESS_FAMILY_UNSPECIFIED;
  if (operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS ||
      operation == net::ProxyResolveDnsOperation::DNS_RESOLVE) {
    address_family = net::ADDRESS_FAMILY_IPV4;
  }

  return net::HostCache::Key(hostname, address_family,
                             0 /* host_resolver_flags */);
}

}  // namespace

class HostResolverMojo::RequestImpl : public ProxyHostResolver::Request,
                                      public mojom::HostResolverRequestClient {
 public:
  RequestImpl(const std::string& hostname,
              net::ProxyResolveDnsOperation operation,
              base::WeakPtr<net::HostCache> host_cache,
              Impl* impl)
      : hostname_(hostname),
        operation_(operation),
        binding_(this),
        host_cache_(std::move(host_cache)),
        impl_(impl) {}

  ~RequestImpl() override = default;

  // ProxyHostResolver::Request override
  int Start(net::CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(1) << "Resolve " << hostname_;

    int cached_result = ResolveFromCacheInternal();
    if (cached_result != net::ERR_DNS_CACHE_MISS) {
      DVLOG(1) << "Resolved " << hostname_ << " from cache";
      return cached_result;
    }

    callback_ = std::move(callback);

    mojom::HostResolverRequestClientPtr handle;
    binding_.Bind(mojo::MakeRequest(&handle));
    binding_.set_connection_error_handler(base::BindOnce(
        &RequestImpl::OnConnectionError, base::Unretained(this)));

    impl_->ResolveDns(hostname_, operation_, std::move(handle));
    return net::ERR_IO_PENDING;
  }

  const std::vector<net::IPAddress>& GetResults() const override {
    return results_;
  }

  // mojom::HostResolverRequestClient override
  void ReportResult(int32_t error,
                    const std::vector<net::IPAddress>& result) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (error == net::OK)
      results_ = result;
    if (host_cache_) {
      base::TimeDelta ttl =
          error == net::OK ? kCacheEntryTTL : kNegativeCacheEntryTTL;
      net::HostCache::Entry entry(
          error, net::AddressList::CreateFromIPAddressList(result, ""),
          net::HostCache::Entry::SOURCE_UNKNOWN, ttl);
      host_cache_->Set(CacheKeyForRequest(hostname_, operation_), entry,
                       base::TimeTicks::Now(), ttl);
    }
    if (binding_.is_bound())
      binding_.Close();
    std::move(callback_).Run(error);
  }

 private:
  int ResolveFromCacheInternal() {
    DCHECK(host_cache_);

    net::HostCache::Key key = CacheKeyForRequest(hostname_, operation_);
    const std::pair<const net::HostCache::Key, net::HostCache::Entry>*
        cache_result = host_cache_->Lookup(key, base::TimeTicks::Now());
    if (!cache_result)
      return net::ERR_DNS_CACHE_MISS;

    results_ = AddressListToAddresses(cache_result->second.addresses().value());
    return cache_result->second.error();
  }

  void OnConnectionError() { ReportResult(net::ERR_FAILED, {} /* result */); }

  static std::vector<net::IPAddress> AddressListToAddresses(
      net::AddressList address_list) {
    std::vector<net::IPAddress> result;
    for (net::IPEndPoint endpoint : address_list.endpoints()) {
      result.push_back(endpoint.address());
    }
    return result;
  }

  const std::string hostname_;
  const net::ProxyResolveDnsOperation operation_;

  mojo::Binding<mojom::HostResolverRequestClient> binding_;
  net::CompletionOnceCallback callback_;

  base::WeakPtr<net::HostCache> host_cache_;
  Impl* const impl_;
  std::vector<net::IPAddress> results_;

  THREAD_CHECKER(thread_checker_);
};

HostResolverMojo::HostResolverMojo(Impl* impl)
    : impl_(impl),
      host_cache_(net::HostCache::CreateDefaultCache()),
      host_cache_weak_factory_(host_cache_.get()) {}

HostResolverMojo::~HostResolverMojo() = default;

std::unique_ptr<net::ProxyHostResolver::Request>
HostResolverMojo::CreateRequest(const std::string& hostname,
                                net::ProxyResolveDnsOperation operation) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<RequestImpl>(
      hostname, operation, host_cache_weak_factory_.GetWeakPtr(), impl_);
}

}  // namespace proxy_resolver
