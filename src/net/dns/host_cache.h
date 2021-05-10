// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_CACHE_H_
#define NET_DNS_HOST_CACHE_H_

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/numerics/clamped_math.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/expiring_cache.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/public/dns_query_type.h"
#include "net/log/net_log_capture_mode.h"

namespace base {
class ListValue;
class TickClock;
}  // namespace base

namespace net {

// Cache used by HostResolver to map hostnames to their resolved result.
class NET_EXPORT HostCache {
 public:
  struct NET_EXPORT Key {
    Key(const std::string& hostname,
        DnsQueryType dns_query_type,
        HostResolverFlags host_resolver_flags,
        HostResolverSource host_resolver_source,
        const NetworkIsolationKey& network_isolation_key);
    Key();
    Key(const Key& key);
    Key(Key&& key);

    // This is a helper used in comparing keys. The order of comparisons of
    // |Key| fields is arbitrary, but the tuple is constructed with
    // |dns_query_type| and |host_resolver_flags| before |hostname| under the
    // assumption that integer comparisons are faster than string comparisons.
    auto GetTuple(const Key* key) const {
      return std::tie(key->dns_query_type, key->host_resolver_flags,
                      key->hostname, key->host_resolver_source,
                      key->network_isolation_key, key->secure);
    }

    bool operator==(const Key& other) const {
      return GetTuple(this) == GetTuple(&other);
    }

    bool operator<(const Key& other) const {
      return GetTuple(this) < GetTuple(&other);
    }

    std::string hostname;
    DnsQueryType dns_query_type = DnsQueryType::UNSPECIFIED;
    HostResolverFlags host_resolver_flags = 0;
    HostResolverSource host_resolver_source = HostResolverSource::ANY;
    NetworkIsolationKey network_isolation_key;
    bool secure = false;
  };

  struct NET_EXPORT EntryStaleness {
    // Time since the entry's TTL has expired. Negative if not expired.
    base::TimeDelta expired_by;

    // Number of network changes since this result was cached.
    int network_changes;

    // Number of hits to the cache entry while stale (expired or past-network).
    int stale_hits;

    bool is_stale() const {
      return network_changes > 0 || expired_by >= base::TimeDelta();
    }
  };

  // Stores the latest address list that was looked up for a hostname.
  class NET_EXPORT Entry {
   public:
    enum Source : int {
      // Address list was obtained from an unknown source.
      SOURCE_UNKNOWN,
      // Address list was obtained via a DNS lookup.
      SOURCE_DNS,
      // Address list was obtained by searching a HOSTS file.
      SOURCE_HOSTS,
    };

    // |ttl=base::nullopt| for unknown TTL.
    template <typename T>
    Entry(int error,
          T&& results,
          Source source,
          base::Optional<base::TimeDelta> ttl)
        : error_(error),
          source_(source),
          ttl_(ttl ? ttl.value() : base::TimeDelta::FromSeconds(-1)) {
      DCHECK(!ttl || ttl.value() >= base::TimeDelta());
      SetResult(std::forward<T>(results));
    }

    // Use when |ttl| is unknown.
    template <typename T>
    Entry(int error, T&& results, Source source)
        : Entry(error, std::forward<T>(results), source, base::nullopt) {}

    // For errors with no |results|.
    Entry(int error,
          Source source,
          base::Optional<base::TimeDelta> ttl = base::nullopt);

    Entry(const Entry& entry);
    Entry(Entry&& entry);
    ~Entry();

    Entry& operator=(const Entry& entry);
    Entry& operator=(Entry&& entry);

    int error() const { return error_; }
    bool did_complete() const {
      return error_ != ERR_NETWORK_CHANGED &&
             error_ != ERR_HOST_RESOLVER_QUEUE_TOO_LARGE;
    }
    void set_error(int error) { error_ = error; }
    const base::Optional<AddressList>& addresses() const { return addresses_; }
    void set_addresses(const base::Optional<AddressList>& addresses) {
      addresses_ = addresses;
    }
    const base::Optional<std::vector<std::string>>& text_records() const {
      return text_records_;
    }
    void set_text_records(
        base::Optional<std::vector<std::string>> text_records) {
      text_records_ = std::move(text_records);
    }
    const base::Optional<std::vector<HostPortPair>>& hostnames() const {
      return hostnames_;
    }
    void set_hostnames(base::Optional<std::vector<HostPortPair>> hostnames) {
      hostnames_ = std::move(hostnames);
    }
    const base::Optional<std::vector<bool>>& experimental_results() const {
      return experimental_results_;
    }
    void set_experimental_results(
        base::Optional<std::vector<bool>> experimental_results) {
      experimental_results_ = std::move(experimental_results);
    }

    Source source() const { return source_; }
    bool has_ttl() const { return ttl_ >= base::TimeDelta(); }
    base::TimeDelta ttl() const { return ttl_; }
    base::Optional<base::TimeDelta> GetOptionalTtl() const;
    void set_ttl(base::TimeDelta ttl) { ttl_ = ttl; }

    base::TimeTicks expires() const { return expires_; }

    // Public for the net-internals UI.
    int network_changes() const { return network_changes_; }

    // Merge |front| and |back|, representing results from multiple transactions
    // for the same overall host resolution query.
    //
    // Merges lists, placing elements from |front| before elements from |back|.
    // Further, dedupes address lists and moves IPv6 addresses before IPv4
    // addresses (maintaining stable order otherwise).
    //
    // Fields that cannot be merged take precedence from |front|.
    static Entry MergeEntries(Entry front, Entry back);

    // Creates a value representation of the entry for use with NetLog.
    base::Value NetLogParams() const;

    // Creates a copy of |this| with the port of all address and hostname values
    // set to |port| if the current port is 0. Preserves any non-zero ports.
    HostCache::Entry CopyWithDefaultPort(uint16_t port) const;

   private:
    friend class HostCache;

    Entry(const Entry& entry,
          base::TimeTicks now,
          base::TimeDelta ttl,
          int network_changes);

    Entry(int error,
          const base::Optional<AddressList>& addresses,
          base::Optional<std::vector<std::string>>&& text_results,
          base::Optional<std::vector<HostPortPair>>&& hostnames,
          base::Optional<std::vector<bool>>&& experimental_results,
          Source source,
          base::TimeTicks expires,
          int network_changes);

    void PrepareForCacheInsertion();

    void SetResult(AddressList addresses) { addresses_ = std::move(addresses); }
    void SetResult(std::vector<std::string> text_records) {
      text_records_ = std::move(text_records);
    }
    void SetResult(std::vector<HostPortPair> hostnames) {
      hostnames_ = std::move(hostnames);
    }
    void SetResult(std::vector<bool> experimental_results) {
      experimental_results_ = std::move(experimental_results);
    }

    int total_hits() const { return total_hits_; }
    int stale_hits() const { return stale_hits_; }

    bool IsStale(base::TimeTicks now, int network_changes) const;
    void CountHit(bool hit_is_stale);
    void GetStaleness(base::TimeTicks now,
                      int network_changes,
                      EntryStaleness* out) const;

    // Merges addresses from |source| into the stored list of addresses and
    // deduplicates. The address list can be accessed with |addresses()|. This
    // method performs a stable sort to ensure IPv6 addresses precede IPv4
    // addresses. IP versions being equal, addresses from |*this| will precede
    // those from |source|.
    //
    // Only non-failure entries (`error_` is OK or ERR_NAME_NOT_RESOLVED) can be
    // merged. Because an ERR_NAME_NOT_RESOLVED represents success without any
    // results, merging an OK entry with an ERR_NAME_NOT_RESOLVED entry
    // represents merging a non-empty entry with an empty entry, resulting in
    // non-empty and therefore OK.
    void MergeAddressesFrom(const HostCache::Entry& source);

    // Merges DNS aliases from |source| into the stored list of DNS aliases and
    // deduplicates.
    void MergeDnsAliasesFrom(const HostCache::Entry& source);

    base::Value GetAsValue(bool include_staleness) const;

    // The resolve results for this entry.
    int error_ = ERR_FAILED;
    base::Optional<AddressList> addresses_;
    base::Optional<std::vector<std::string>> text_records_;
    base::Optional<std::vector<HostPortPair>> hostnames_;
    base::Optional<std::vector<bool>> experimental_results_;
    // Where results were obtained (e.g. DNS lookup, hosts file, etc).
    Source source_ = SOURCE_UNKNOWN;
    // TTL obtained from the nameserver. Negative if unknown.
    base::TimeDelta ttl_ = base::TimeDelta::FromSeconds(-1);

    base::TimeTicks expires_;
    // Copied from the cache's network_changes_ when the entry is set; can
    // later be compared to it to see if the entry was received on the current
    // network.
    int network_changes_ = -1;
    // Use clamped math to cap hit counts at INT_MAX.
    base::ClampedNumeric<int> total_hits_ = 0;
    base::ClampedNumeric<int> stale_hits_ = 0;
  };

  // Interface for interacting with persistent storage, to be provided by the
  // embedder. Does not include support for writes that must happen immediately.
  class PersistenceDelegate {
   public:
    // Calling ScheduleWrite() signals that data has changed and should be
    // written to persistent storage. The write might be delayed.
    virtual void ScheduleWrite() = 0;
  };

  using EntryMap = std::map<Key, Entry>;

  // The two ways to serialize the cache to a value.
  enum class SerializationType {
    // Entries with transient NetworkIsolationKeys are not serialized, and
    // RestoreFromListValue() can load the returned value.
    kRestorable,
    // Entries with transient NetworkIsolationKeys are serialized, and
    // RestoreFromListValue() cannot load the returned value, since the debug
    // serialization of NetworkIsolationKeys is used instead of the
    // deserializable representation.
    kDebug,
  };

  // A HostCache::EntryStaleness representing a non-stale (fresh) cache entry.
  static const HostCache::EntryStaleness kNotStale;

  // Constructs a HostCache that stores up to |max_entries|.
  explicit HostCache(size_t max_entries);

  ~HostCache();

  // Returns a pointer to the matching (key, entry) pair, which is valid at time
  // |now|. If |ignore_secure| is true, ignores the secure field in |key| when
  // looking for a match. If there is no matching entry, returns NULL.
  const std::pair<const Key, Entry>* Lookup(const Key& key,
                                            base::TimeTicks now,
                                            bool ignore_secure = false);

  // Returns a pointer to the matching (key, entry) pair, whether it is valid or
  // stale at time |now|. Fills in |stale_out| with information about how stale
  // it is. If |ignore_secure| is true, ignores the secure field in |key| when
  // looking for a match. If there is no matching entry, returns NULL.
  const std::pair<const Key, Entry>* LookupStale(const Key& key,
                                                 base::TimeTicks now,
                                                 EntryStaleness* stale_out,
                                                 bool ignore_secure = false);

  // Overwrites or creates an entry for |key|.
  // |entry| is the value to set, |now| is the current time
  // |ttl| is the "time to live".
  void Set(const Key& key,
           const Entry& entry,
           base::TimeTicks now,
           base::TimeDelta ttl);

  // Checks whether an entry exists for |hostname|.
  // If so, returns the matching key and writes the source (e.g. DNS, HOSTS
  // file, etc.) to |source_out| and the staleness to |stale_out| (if they are
  // not null). It tries using two common address_family and host_resolver_flag
  // combinations when performing lookups in the cache; this means false
  // negatives are possible, but unlikely. It also ignores the secure field
  // while searching for matches. If no entry exists, returns nullptr.
  const HostCache::Key* GetMatchingKey(base::StringPiece hostname,
                                       HostCache::Entry::Source* source_out,
                                       HostCache::EntryStaleness* stale_out);

  // Marks all entries as stale on account of a network change.
  void Invalidate();

  void set_persistence_delegate(PersistenceDelegate* delegate);

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Empties the cache.
  void clear();

  // Clears hosts matching |host_filter| from the cache.
  void ClearForHosts(
      const base::RepeatingCallback<bool(const std::string&)>& host_filter);

  // Fills the provided base::ListValue with the contents of the cache for
  // serialization. |entry_list| must be non-null and will be cleared before
  // adding the cache contents.
  void GetAsListValue(base::ListValue* entry_list,
                      bool include_staleness,
                      SerializationType serialization_type) const;
  // Takes a base::ListValue representing cache entries and stores them in the
  // cache, skipping any that already have entries. Returns true on success,
  // false on failure.
  bool RestoreFromListValue(const base::ListValue& old_cache);
  // Returns the number of entries that were restored in the last call to
  // RestoreFromListValue().
  size_t last_restore_size() const { return restore_size_; }

  // Returns the number of entries in the cache.
  size_t size() const;

  // Following are used by net_internals UI.
  size_t max_entries() const;
  int network_changes() const { return network_changes_; }
  const EntryMap& entries() const { return entries_; }

  // Creates a default cache.
  static std::unique_ptr<HostCache> CreateDefaultCache();

 private:
  FRIEND_TEST_ALL_PREFIXES(HostCacheTest, NoCache);

  enum SetOutcome : int;
  enum LookupOutcome : int;
  enum EraseReason : int;

  // Returns the result that is least stale, based on the number of network
  // changes since the result was cached. If the results are equally stale,
  // prefers a securely retrieved result. Returns nullptr if both results are
  // nullptr.
  static std::pair<const HostCache::Key, HostCache::Entry>*
  GetLessStaleMoreSecureResult(
      base::TimeTicks now,
      std::pair<const HostCache::Key, HostCache::Entry>* result1,
      std::pair<const HostCache::Key, HostCache::Entry>* result2);

  // Returns matching key and entry from cache and nullptr if no match. Ignores
  // the secure field in |initial_key| if |ignore_secure| is true.
  std::pair<const Key, Entry>* LookupInternalIgnoringFields(
      const Key& initial_key,
      base::TimeTicks now,
      bool ignore_secure);

  // Returns matching key and entry from cache and nullptr if no match. An exact
  // match for |key| is required.
  std::pair<const Key, Entry>* LookupInternal(const Key& key);

  // Returns true if this HostCache can contain no entries.
  bool caching_is_disabled() const { return max_entries_ == 0; }

  void EvictOneEntry(base::TimeTicks now);
  // Helper to insert an Entry into the cache.
  void AddEntry(const Key& key, Entry&& entry);

  // Map from hostname (presumably in lowercase canonicalized format) to
  // a resolved result entry.
  EntryMap entries_;
  size_t max_entries_;
  int network_changes_;
  // Number of cache entries that were restored in the last call to
  // RestoreFromListValue(). Used in histograms.
  size_t restore_size_;

  PersistenceDelegate* delegate_;
  // Shared tick clock, overridden for testing.
  const base::TickClock* tick_clock_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(HostCache);
};

}  // namespace net

#endif  // NET_DNS_HOST_CACHE_H_
