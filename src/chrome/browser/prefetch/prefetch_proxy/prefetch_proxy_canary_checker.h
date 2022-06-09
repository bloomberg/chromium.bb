// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_CANARY_CHECKER_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_CANARY_CHECKER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/lru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class NetworkConnectionTracker;
}  // namespace network

// This class makes DNS lookups to a specified host to verify if the user's ISP
// would like Prefetch Proxy users to first probe the prefetched host before
// using a prefetched resource. This allows ISP to perform filtering even if
// a response has been fetched via an encrypted tunnel through the Prefetch
// Proxy.
class PrefetchProxyCanaryChecker {
 public:
  // Callers who wish to use this class should add a value to this enum. This
  // enum is mapped to a string value which is then used in histograms and
  // prefs. Be sure to update the |PrefetchProxy.CanaryChecker.Clients|
  // histogram suffix in
  // //tools/metrics/histograms/metadata/prefetch/histograms.xml whenever a
  // change is made to this enum.
  //
  // Please add the header file of the client when new items are added.
  enum class CheckType {
    // chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h
    kDNS = 0,
    kTLS = 1,
    kMaxValue = kTLS,
  };

  struct RetryPolicy {
    RetryPolicy();
    RetryPolicy(const RetryPolicy& other);
    ~RetryPolicy();

    // The maximum number of retries (not including the original check) to
    // attempt.
    size_t max_retries = 0;

    // Backoff policy to use to compute how long we should wait between the end
    // of last retry and start of next retry.
    net::BackoffEntry::Policy backoff_policy = {
        .num_errors_to_ignore = 0,
        .initial_delay_ms = 100,
        .multiply_factor = 2,
        .jitter_factor = 0.2,
        // No maximum backoff.
        .maximum_backoff_ms = -1,
        .entry_lifetime_ms = -1,
        .always_use_initial_delay = false,
    };
  };

  // Cache entry representing a canary check result.
  struct CacheEntry {
    bool success;
    base::Time last_modified;
  };

  PrefetchProxyCanaryChecker(Profile* profile,
                             CheckType name,
                             const GURL& url,
                             const RetryPolicy& retry_policy,
                             const base::TimeDelta check_timeout,
                             base::TimeDelta revalidate_cache_after);

  PrefetchProxyCanaryChecker(const PrefetchProxyCanaryChecker&) = delete;
  PrefetchProxyCanaryChecker& operator=(const PrefetchProxyCanaryChecker&) =
      delete;

  ~PrefetchProxyCanaryChecker();

  base::WeakPtr<PrefetchProxyCanaryChecker> AsWeakPtr() const;

  // Returns the successfulness of the last canary check, if there was one. If
  // the last status was not cached or was cached and needs to be revalidated,
  // this may trigger new checks. This updates the
  // PrefetchProxy.CanaryChecker.CacheLookupStatus histogram, so avoid calling
  // this method repeatedly when its result can be reused.
  absl::optional<bool> CanaryCheckSuccessful();

  // Triggers new canary checks if there is no cached status or if the cached
  // status is stale. Use this method over CanaryCheckSuccessful if you only
  // want to freshen the cache (as opposed to look up the cached value), as the
  // CanaryCheckSuccessful method updates the CacheLookupStatus histogram, but
  // RunChecksIfNeeded does not.
  void RunChecksIfNeeded();

  // True if checks are being attempted, including retries.
  bool IsActive() const { return time_when_set_active_.has_value(); }

 protected:
  // Exposes |tick_clock| and |clock| for testing.
  PrefetchProxyCanaryChecker(Profile* profile,
                             CheckType name,
                             const GURL& url,
                             const RetryPolicy& retry_policy,
                             const base::TimeDelta check_timeout,
                             base::TimeDelta revalidate_cache_after,
                             const base::TickClock* tick_clock,
                             const base::Clock* clock);

 private:
  void ResetState();
  void StartDNSResolution(const GURL& url);
  void OnDNSResolved(
      int net_error,
      const absl::optional<net::AddressList>& resolved_addresses);
  void ProcessTimeout();
  void ProcessFailure(int net_error);
  void ProcessSuccess();
  void RecordResult(bool success);
  std::string GetCacheKeyForCurrentNetwork() const;
  std::string AppendNameToHistogram(const std::string& histogram) const;
  absl::optional<bool> LookupAndRunChecksIfNeeded();
  // Sends a check now if the checker is currently inactive. If the check is
  // active (i.e.: there are DNS resolutions in flight), this is a no-op.
  void SendNowIfInactive();

  // This is called whenever the canary check is done. This is caused whenever
  // the check succeeds, fails and there are no more retries, or the delegate
  // stops the probing.
  void OnCheckEnd(bool success);

  // The current profile, not owned.
  Profile* profile_;

  // Pipe to allow cancelling an ongoing DNS resolution request. This is set
  // when we fire off a DNS request to the network service. We send the
  // receiving end to the network service as part of the parameters of the
  // ResolveHost call. The network service then listens to this pipe to
  // potentially cancel the request. The pipe is reset as when the request
  // completes (success or failure).
  mojo::Remote<network::mojom::ResolveHostHandle> resolver_control_handle_;

  // The name given to this checker instance. Used in metrics.
  const std::string name_;

  // The URL that will be DNS resolved.
  const GURL url_;

  // The retry policy to use in this checker.
  const RetryPolicy retry_policy_;

  // The exponential backoff state for retries. This gets reset at the end of
  // each check.
  net::BackoffEntry backoff_entry_;

  // How long before we should timeout a DNS check and retry.
  const base::TimeDelta check_timeout_;

  // How long to allow a cached entry to be valid until it is revalidated in the
  // background.
  const base::TimeDelta revalidate_cache_after_;

  // If a retry is being attempted, this will be running until the next attempt.
  std::unique_ptr<base::OneShotTimer> retry_timer_;

  // If a check is being attempted, this will be running until the TTL.
  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // The tick clock used within this class.
  const base::TickClock* tick_clock_;

  // The time clock used within this class.
  const base::Clock* clock_;

  // Remembers the last time the checker became active.
  absl::optional<base::Time> time_when_set_active_;

  // This reference is kept around for unregistering |this| as an observer on
  // any thread.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Small LRU cache holding the result of canary checks made for different
  // networks. This cache is not persisted across browser restarts.
  base::LRUCache<std::string, PrefetchProxyCanaryChecker::CacheEntry> cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchProxyCanaryChecker> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_CANARY_CHECKER_H_
