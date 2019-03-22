// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/host_resolver_mojo.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

namespace proxy_resolver {
namespace {

// Default TTL for successful host resolutions.
const int kCacheEntryTTLSeconds = 5;

// Default TTL for unsuccessful host resolutions.
const int kNegativeCacheEntryTTLSeconds = 0;

net::HostCache::Key CacheKeyForRequest(
    const net::HostResolver::RequestInfo& info) {
  return net::HostCache::Key(info.hostname(), info.address_family(),
                             info.host_resolver_flags());
}

}  // namespace

class HostResolverMojo::Job : public mojom::HostResolverRequestClient {
 public:
  Job(const net::HostCache::Key& key,
      net::AddressList* addresses,
      net::CompletionOnceCallback callback,
      mojo::InterfaceRequest<mojom::HostResolverRequestClient> request,
      base::WeakPtr<net::HostCache> host_cache);

 private:
  // mojom::HostResolverRequestClient override.
  void ReportResult(int32_t error,
                    const net::AddressList& address_list) override;

  // Mojo error handler.
  void OnConnectionError();

  const net::HostCache::Key key_;
  net::AddressList* addresses_;
  net::CompletionOnceCallback callback_;
  mojo::Binding<mojom::HostResolverRequestClient> binding_;
  base::WeakPtr<net::HostCache> host_cache_;
};

class HostResolverMojo::RequestImpl : public HostResolver::Request {
 public:
  explicit RequestImpl(std::unique_ptr<Job> job) : job_(std::move(job)) {}

  ~RequestImpl() override = default;

  void ChangeRequestPriority(net::RequestPriority priority) override {}

 private:
  std::unique_ptr<Job> job_;
};

HostResolverMojo::HostResolverMojo(Impl* impl)
    : impl_(impl),
      host_cache_(net::HostCache::CreateDefaultCache()),
      host_cache_weak_factory_(host_cache_.get()) {}

HostResolverMojo::~HostResolverMojo() = default;

std::unique_ptr<net::HostResolver::ResolveHostRequest>
HostResolverMojo::CreateRequest(
    const net::HostPortPair& host,
    const net::NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  // TODO(crbug.com/821021): Implement.
  NOTIMPLEMENTED();
  return nullptr;
}

int HostResolverMojo::Resolve(const RequestInfo& info,
                              net::RequestPriority priority,
                              net::AddressList* addresses,
                              net::CompletionOnceCallback callback,
                              std::unique_ptr<Request>* request,
                              const net::NetLogWithSource& source_net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(request);
  DVLOG(1) << "Resolve " << info.host_port_pair().ToString();

  net::HostCache::Key key = CacheKeyForRequest(info);
  int cached_result = ResolveFromCacheInternal(info, key, addresses);
  if (cached_result != net::ERR_DNS_CACHE_MISS) {
    DVLOG(1) << "Resolved " << info.host_port_pair().ToString()
             << " from cache";
    return cached_result;
  }

  mojom::HostResolverRequestClientPtr handle;
  std::unique_ptr<Job> job(new Job(key, addresses, std::move(callback),
                                   mojo::MakeRequest(&handle),
                                   host_cache_weak_factory_.GetWeakPtr()));
  request->reset(new RequestImpl(std::move(job)));

  impl_->ResolveDns(std::make_unique<HostResolver::RequestInfo>(info),
                    std::move(handle));
  return net::ERR_IO_PENDING;
}

int HostResolverMojo::ResolveFromCache(
    const RequestInfo& info,
    net::AddressList* addresses,
    const net::NetLogWithSource& source_net_log) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "ResolveFromCache " << info.host_port_pair().ToString();
  return ResolveFromCacheInternal(info, CacheKeyForRequest(info), addresses);
}

int HostResolverMojo::ResolveStaleFromCache(
    const RequestInfo& info,
    net::AddressList* addresses,
    net::HostCache::EntryStaleness* stale_info,
    const net::NetLogWithSource& net_log) {
  NOTREACHED();
  return net::ERR_UNEXPECTED;
}

net::HostCache* HostResolverMojo::GetHostCache() {
  return host_cache_.get();
}

bool HostResolverMojo::HasCached(
    base::StringPiece hostname,
    net::HostCache::Entry::Source* source_out,
    net::HostCache::EntryStaleness* stale_out) const {
  if (!host_cache_)
    return false;

  return host_cache_->HasEntry(hostname, source_out, stale_out);
}

int HostResolverMojo::ResolveFromCacheInternal(const RequestInfo& info,
                                               const net::HostCache::Key& key,
                                               net::AddressList* addresses) {
  if (!info.allow_cached_response())
    return net::ERR_DNS_CACHE_MISS;

  const net::HostCache::Entry* entry =
      host_cache_->Lookup(key, base::TimeTicks::Now());
  if (!entry)
    return net::ERR_DNS_CACHE_MISS;

  *addresses =
      net::AddressList::CopyWithPort(entry->addresses().value(), info.port());
  return entry->error();
}

HostResolverMojo::Job::Job(
    const net::HostCache::Key& key,
    net::AddressList* addresses,
    net::CompletionOnceCallback callback,
    mojo::InterfaceRequest<mojom::HostResolverRequestClient> request,
    base::WeakPtr<net::HostCache> host_cache)
    : key_(key),
      addresses_(addresses),
      callback_(std::move(callback)),
      binding_(this, std::move(request)),
      host_cache_(host_cache) {
  binding_.set_connection_error_handler(base::Bind(
      &HostResolverMojo::Job::OnConnectionError, base::Unretained(this)));
}

void HostResolverMojo::Job::ReportResult(int32_t error,
                                         const net::AddressList& address_list) {
  if (error == net::OK)
    *addresses_ = address_list;
  if (host_cache_) {
    base::TimeDelta ttl = base::TimeDelta::FromSeconds(
        error == net::OK ? kCacheEntryTTLSeconds
                         : kNegativeCacheEntryTTLSeconds);
    net::HostCache::Entry entry(error, *addresses_,
                                net::HostCache::Entry::SOURCE_UNKNOWN, ttl);
    host_cache_->Set(key_, entry, base::TimeTicks::Now(), ttl);
  }
  if (binding_.is_bound())
    binding_.Close();
  std::move(callback_).Run(error);
}

void HostResolverMojo::Job::OnConnectionError() {
  ReportResult(net::ERR_FAILED, net::AddressList());
}

}  // namespace proxy_resolver
