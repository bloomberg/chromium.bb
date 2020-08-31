// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/db/v4_local_database_manager.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

using CommandLineSwitchAndThreatType = std::pair<std::string, ThreatType>;

// The expiration time of the full hash stored in the artificial database.
const int64_t kFullHashExpiryTimeInMinutes = 60;

const ThreatSeverity kLeastSeverity =
    std::numeric_limits<ThreatSeverity>::max();

// The list of the name of any store files that are no longer used and can be
// safely deleted from the disk. There's no overlap allowed between the files
// on this list and the list returned by GetListInfos().
const char* const kStoreFileNamesToDelete[] = {
    "AnyIpMalware.store", "ChromeFilenameClientIncident.store",
    "UrlSuspiciousSiteId.store"};

ListInfos GetListInfos() {
// NOTE(vakh): When adding a store here, add the corresponding store-specific
// histograms also.
// The first argument to ListInfo specifies whether to sync hash prefixes for
// that list. This can be false for two reasons:
// - The server doesn't support that list yet. Once the server adds support
//   for it, it can be changed to true.
// - The list doesn't have hash prefixes to match. All requests lead to full
//   hash checks. For instance: GetChromeUrlApiId()

#if defined(OS_IOS)
  const bool kSyncOnlyOnChromeBuilds = false;
  const bool kSyncOnlyOnDesktopBuilds = false;
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const bool kSyncOnlyOnChromeBuilds = true;
  const bool kSyncOnlyOnDesktopBuilds = true;
#else
  const bool kSyncOnlyOnChromeBuilds = false;
  const bool kSyncOnlyOnDesktopBuilds = true;
#endif

  const bool kSyncAlways = true;
  const bool kSyncNever = false;
  return ListInfos({
      ListInfo(kSyncOnlyOnDesktopBuilds, "IpMalware.store", GetIpMalwareId(),
               SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncAlways, "UrlSoceng.store", GetUrlSocEngId(),
               SB_THREAT_TYPE_URL_PHISHING),
      ListInfo(kSyncAlways, "UrlMalware.store", GetUrlMalwareId(),
               SB_THREAT_TYPE_URL_MALWARE),
      ListInfo(kSyncOnlyOnDesktopBuilds, "UrlUws.store", GetUrlUwsId(),
               SB_THREAT_TYPE_URL_UNWANTED),
      ListInfo(kSyncOnlyOnDesktopBuilds, "UrlMalBin.store", GetUrlMalBinId(),
               SB_THREAT_TYPE_URL_BINARY_MALWARE),
      ListInfo(kSyncOnlyOnDesktopBuilds, "ChromeExtMalware.store",
               GetChromeExtMalwareId(), SB_THREAT_TYPE_EXTENSION),
      ListInfo(kSyncOnlyOnChromeBuilds, "CertCsdDownloadWhitelist.store",
               GetCertCsdDownloadWhitelistId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncOnlyOnChromeBuilds, "ChromeUrlClientIncident.store",
               GetChromeUrlClientIncidentId(),
               SB_THREAT_TYPE_BLACKLISTED_RESOURCE),
      ListInfo(kSyncAlways, "UrlBilling.store", GetUrlBillingId(),
               SB_THREAT_TYPE_BILLING),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlCsdDownloadWhitelist.store",
               GetUrlCsdDownloadWhitelistId(), SB_THREAT_TYPE_UNUSED),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlCsdWhitelist.store",
               GetUrlCsdWhitelistId(), SB_THREAT_TYPE_CSD_WHITELIST),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlSubresourceFilter.store",
               GetUrlSubresourceFilterId(), SB_THREAT_TYPE_SUBRESOURCE_FILTER),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlSuspiciousSite.store",
               GetUrlSuspiciousSiteId(), SB_THREAT_TYPE_SUSPICIOUS_SITE),
      ListInfo(kSyncNever, "", GetChromeUrlApiId(), SB_THREAT_TYPE_API_ABUSE),
      ListInfo(kSyncOnlyOnChromeBuilds, "UrlHighConfidenceAllowlist.store",
               GetUrlHighConfidenceAllowlistId(),
               SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST),
  });
  // NOTE(vakh): IMPORTANT: Please make sure that the server already supports
  // any list before adding it to this list otherwise the prefix updates break
  // for all Canary users.
}

std::vector<CommandLineSwitchAndThreatType> GetSwitchAndThreatTypes() {
  static const std::vector<CommandLineSwitchAndThreatType>
      command_line_switch_and_threat_type = {
          {"mark_as_phishing", SOCIAL_ENGINEERING},
          {"mark_as_malware", MALWARE_THREAT},
          {"mark_as_uws", UNWANTED_SOFTWARE}};
  return command_line_switch_and_threat_type;
}

// Returns the severity information about a given SafeBrowsing list. The lowest
// value is 0, which represents the most severe list.
ThreatSeverity GetThreatSeverity(const ListIdentifier& list_id) {
  switch (list_id.threat_type()) {
    case MALWARE_THREAT:
    case SOCIAL_ENGINEERING:
    case MALICIOUS_BINARY:
      return 0;
    case UNWANTED_SOFTWARE:
      return 1;
    case API_ABUSE:
    case CLIENT_INCIDENT:
    case SUBRESOURCE_FILTER:
      return 2;
    case CSD_WHITELIST:
    case HIGH_CONFIDENCE_ALLOWLIST:
      return 3;
    case SUSPICIOUS:
      return 4;
    case BILLING:
      return 15;
    default:
      NOTREACHED() << "Unexpected ThreatType encountered: "
                   << list_id.threat_type();
      return kLeastSeverity;
  }
}

// This is only valid for types that are passed to GetBrowseUrl().
ListIdentifier GetUrlIdFromSBThreatType(SBThreatType sb_threat_type) {
  switch (sb_threat_type) {
    case SB_THREAT_TYPE_URL_MALWARE:
      return GetUrlMalwareId();

    case SB_THREAT_TYPE_URL_PHISHING:
      return GetUrlSocEngId();

    case SB_THREAT_TYPE_URL_UNWANTED:
      return GetUrlUwsId();

    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return GetUrlSuspiciousSiteId();

    case SB_THREAT_TYPE_BILLING:
      return GetUrlBillingId();

    default:
      NOTREACHED();
      // Compiler requires a return statement here.
      return GetUrlMalwareId();
  }
}

StoresToCheck CreateStoresToCheckFromSBThreatTypeSet(
    const SBThreatTypeSet& threat_types) {
  StoresToCheck stores_to_check;
  for (SBThreatType sb_threat_type : threat_types) {
    stores_to_check.insert(GetUrlIdFromSBThreatType(sb_threat_type));
  }
  return stores_to_check;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum StoreAvailabilityResult {
  // Unknown availability. This is unexpected.
  UNKNOWN = 0,

  // The local database is not enabled.
  NOT_ENABLED = 1,

  // The database is still being loaded.
  DATABASE_UNAVAILABLE = 2,

  // The requested store is unavailable.
  STORE_UNAVAILABLE = 3,

  // The store is available.
  AVAILABLE = 4,
  COUNT,
};

void RecordTimeSinceLastUpdateHistograms(const base::Time& last_response_time) {
  bool response_received = !last_response_time.is_null();
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.V4LocalDatabaseManager.HasReceivedUpdateResponse",
      response_received);

  if (!response_received)
    return;

  base::TimeDelta time_since_update = base::Time::Now() - last_response_time;
  UMA_HISTOGRAM_LONG_TIMES_100(
      "SafeBrowsing.V4LocalDatabaseManager.TimeSinceLastUpdateResponse",
      time_since_update);
}

}  // namespace

V4LocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::vector<GURL>& urls)
    : client(client),
      client_callback_type(client_callback_type),
      most_severe_threat_type(SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check),
      urls(urls) {
  for (const auto& url : urls) {
    V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  }
  full_hash_threat_types.assign(full_hashes.size(), SB_THREAT_TYPE_SAFE);
}

V4LocalDatabaseManager::PendingCheck::PendingCheck(
    Client* client,
    ClientCallbackType client_callback_type,
    const StoresToCheck& stores_to_check,
    const std::set<FullHash>& full_hashes_set)
    : client(client),
      client_callback_type(client_callback_type),
      most_severe_threat_type(SB_THREAT_TYPE_SAFE),
      stores_to_check(stores_to_check) {
  full_hashes.assign(full_hashes_set.begin(), full_hashes_set.end());
  DCHECK(full_hashes.size());
  full_hash_threat_types.assign(full_hashes.size(), SB_THREAT_TYPE_SAFE);
}

V4LocalDatabaseManager::PendingCheck::~PendingCheck() {}

// static
const V4LocalDatabaseManager*
    V4LocalDatabaseManager::current_local_database_manager_;

// static
scoped_refptr<V4LocalDatabaseManager> V4LocalDatabaseManager::Create(
    const base::FilePath& base_path,
    ExtendedReportingLevelCallback extended_reporting_level_callback) {
  return base::WrapRefCounted(new V4LocalDatabaseManager(
      base_path, extended_reporting_level_callback, nullptr));
}

void V4LocalDatabaseManager::CollectDatabaseManagerInfo(
    DatabaseManagerInfo* database_manager_info,
    FullHashCacheInfo* full_hash_cache_info) const {
  if (v4_update_protocol_manager_) {
    v4_update_protocol_manager_->CollectUpdateInfo(
        database_manager_info->mutable_update_info());
  }
  if (v4_database_) {
    v4_database_->CollectDatabaseInfo(
        database_manager_info->mutable_database_info());
  }
  if (v4_get_hash_protocol_manager_) {
    v4_get_hash_protocol_manager_->CollectFullHashCacheInfo(
        full_hash_cache_info);
  }
}

V4LocalDatabaseManager::V4LocalDatabaseManager(
    const base::FilePath& base_path,
    ExtendedReportingLevelCallback extended_reporting_level_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_tests)
    : base_path_(base_path),
      extended_reporting_level_callback_(extended_reporting_level_callback),
      list_infos_(GetListInfos()),
      task_runner_(task_runner_for_tests
                       ? task_runner_for_tests
                       : base::ThreadPool::CreateSequencedTaskRunner(
                             {base::MayBlock(),
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());

  DeleteUnusedStoreFiles();

  DVLOG(1) << "V4LocalDatabaseManager::V4LocalDatabaseManager: "
           << "base_path_: " << base_path_.AsUTF8Unsafe();
}

V4LocalDatabaseManager::~V4LocalDatabaseManager() {
  DCHECK(!enabled_);
}

//
// Start: SafeBrowsingDatabaseManager implementation
//

void V4LocalDatabaseManager::CancelCheck(Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  DCHECK(enabled_);

  auto pending_it = std::find_if(
      std::begin(pending_checks_), std::end(pending_checks_),
      [client](const PendingCheck* check) { return check->client == client; });
  if (pending_it != pending_checks_.end()) {
    pending_checks_.erase(pending_it);
  }

  auto queued_it =
      std::find_if(std::begin(queued_checks_), std::end(queued_checks_),
                   [&client](const std::unique_ptr<PendingCheck>& check) {
                     return check->client == client;
                   });
  if (queued_it != queued_checks_.end()) {
    queued_checks_.erase(queued_it);
  }
}

bool V4LocalDatabaseManager::CanCheckResourceType(
    blink::mojom::ResourceType resource_type) const {
  // We check all types since most checks are fast.
  return true;
}

bool V4LocalDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIsWSOrWSS();
}

bool V4LocalDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool V4LocalDatabaseManager::CheckBrowseUrl(const GURL& url,
                                            const SBThreatTypeSet& threat_types,
                                            Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  DCHECK(!threat_types.empty());
  DCHECK(SBThreatTypeSetIsValidForCheckBrowseUrl(threat_types));

  if (!enabled_ || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_BROWSE_URL,
      CreateStoresToCheckFromSBThreatTypeSet(threat_types),
      std::vector<GURL>(1, url));

  bool safe_synchronously = HandleCheck(std::move(check));
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.CheckBrowseUrl.HasLocalMatch",
                        !safe_synchronously);
  RecordTimeSinceLastUpdateHistograms(
      v4_update_protocol_manager_->last_response_time());
  return safe_synchronously;
}

bool V4LocalDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  if (!enabled_ || url_chain.empty()) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_DOWNLOAD_URLS,
      StoresToCheck({GetUrlMalBinId()}), url_chain);

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckExtensionIDs(
    const std::set<FullHash>& extension_ids,
    Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  if (!enabled_) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_EXTENSION_IDS,
      StoresToCheck({GetChromeExtMalwareId()}), extension_ids);

  return HandleCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckResourceUrl(const GURL& url, Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  StoresToCheck stores_to_check({GetChromeUrlClientIncidentId()});

  if (!CanCheckUrl(url) || !AreAllStoresAvailableNow(stores_to_check)) {
    // Fail open: Mark resource as safe immediately.
    // TODO(nparker): This should queue the request if the DB isn't yet
    // loaded, and later decide if this store is available.
    // Currently this is the only store that requires full-hash-checks
    // AND isn't supported on Chromium, so it's unique.
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_RESOURCE_URL, stores_to_check,
      std::vector<GURL>(1, url));

  return HandleCheck(std::move(check));
}

AsyncMatch V4LocalDatabaseManager::CheckUrlForHighConfidenceAllowlist(
    const GURL& url,
    Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  StoresToCheck stores_to_check({GetUrlHighConfidenceAllowlistId()});
  if (!enabled_ || !CanCheckUrl(url) ||
      !AreAllStoresAvailableNow(stores_to_check)) {
    // NOTE(vakh): If Safe Browsing isn't enabled yet, or if the URL isn't a
    // navigation URL, or if the allowlist isn't ready yet, return MATCH.
    // The full URL check won't be performed, but hash-based check will still
    // be done.
    return AsyncMatch::MATCH;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_HIGH_CONFIDENCE_ALLOWLIST,
      stores_to_check, std::vector<GURL>(1, url));

  return HandleWhitelistCheck(std::move(check));
}

bool V4LocalDatabaseManager::CheckUrlForSubresourceFilter(const GURL& url,
                                                          Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  StoresToCheck stores_to_check(
      {GetUrlSocEngId(), GetUrlSubresourceFilterId()});
  if (!AreAnyStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    return true;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_URL_FOR_SUBRESOURCE_FILTER,
      stores_to_check, std::vector<GURL>(1, url));

  return HandleCheck(std::move(check));
}

AsyncMatch V4LocalDatabaseManager::CheckCsdWhitelistUrl(const GURL& url,
                                                        Client* client) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  StoresToCheck stores_to_check({GetUrlCsdWhitelistId()});
  if (!AreAllStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    // Fail open: Whitelist everything. Otherwise we may run the
    // CSD phishing/malware detector on popular domains and generate
    // undue load on the client and server, or send Password Reputation
    // requests on popular sites. This has the effect of disabling
    // CSD phishing/malware detection and password reputation service
    // until the store is first synced and/or loaded from disk.
    return AsyncMatch::MATCH;
  }

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      client, ClientCallbackType::CHECK_CSD_WHITELIST, stores_to_check,
      std::vector<GURL>(1, url));

  return HandleWhitelistCheck(std::move(check));
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  StoresToCheck stores_to_check({GetCertCsdDownloadWhitelistId()});
  if (!AreAllStoresAvailableNow(stores_to_check)) {
    // Fail close: Whitelist nothing. This may generate download-protection
    // pings for whitelisted binaries, but that's fine.
    return false;
  }

  return HandleHashSynchronously(crypto::SHA256HashString(str),
                                 stores_to_check);
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistUrl(const GURL& url) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  StoresToCheck stores_to_check({GetUrlCsdDownloadWhitelistId()});

  if (!AreAllStoresAvailableNow(stores_to_check) || !CanCheckUrl(url)) {
    // Fail close: Whitelist nothing. This may generate download-protection
    // pings for whitelisted domains, but that's fine.
    return false;
  }

  return HandleUrlSynchronously(url, stores_to_check);
}

bool V4LocalDatabaseManager::MatchMalwareIP(const std::string& ip_address) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  if (!enabled_ || !v4_database_) {
    return false;
  }

  FullHash hashed_encoded_ip;
  if (!V4ProtocolManagerUtil::IPAddressToEncodedIPV6Hash(ip_address,
                                                         &hashed_encoded_ip)) {
    return false;
  }

  return HandleHashSynchronously(hashed_encoded_ip,
                                 StoresToCheck({GetIpMalwareId()}));
}

ThreatSource V4LocalDatabaseManager::GetThreatSource() const {
  return ThreatSource::LOCAL_PVER4;
}

bool V4LocalDatabaseManager::IsDownloadProtectionEnabled() const {
  // TODO(vakh): Investigate the possibility of using a command line switch for
  // this instead.
  return true;
}

bool V4LocalDatabaseManager::IsSupported() const {
  return true;
}

void V4LocalDatabaseManager::StartOnIOThread(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnIOThread(url_loader_factory, config);

  db_updated_callback_ = base::BindRepeating(
      &V4LocalDatabaseManager::DatabaseUpdated, weak_factory_.GetWeakPtr());

  SetupUpdateProtocolManager(url_loader_factory, config);
  SetupDatabase();

  enabled_ = true;

  current_local_database_manager_ = this;
}

void V4LocalDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  enabled_ = false;

  current_local_database_manager_ = nullptr;

  pending_checks_.clear();

  RespondSafeToQueuedChecks();

  // Delete the V4Database. Any pending writes to disk are completed.
  // This operation happens on the task_runner on which v4_database_ operates
  // and doesn't block the IO thread.
  V4Database::Destroy(std::move(v4_database_));

  // Delete the V4UpdateProtocolManager.
  // This cancels any in-flight update request.
  v4_update_protocol_manager_.reset();

  db_updated_callback_.Reset();

  SafeBrowsingDatabaseManager::StopOnIOThread(shutdown);
}

//
// End: SafeBrowsingDatabaseManager implementation
//

void V4LocalDatabaseManager::DatabaseReadyForChecks(
    std::unique_ptr<V4Database> v4_database) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // The following check is needed because it is possible that by the time the
  // database is ready, StopOnIOThread has been called.
  if (enabled_) {
    V4Database::Destroy(std::move(v4_database_));
    v4_database_ = std::move(v4_database);

    v4_database_->RecordFileSizeHistograms();

    PopulateArtificialDatabase();

    // The consistency of the stores read from the disk needs to verified. Post
    // that task on the task runner. It calls |DatabaseReadyForUpdates|
    // callback with the stores to reset, if any, and then we can schedule the
    // database updates.
    v4_database_->VerifyChecksum(
        base::BindOnce(&V4LocalDatabaseManager::DatabaseReadyForUpdates,
                       weak_factory_.GetWeakPtr()));

    ProcessQueuedChecks();
  } else {
    // Schedule the deletion of v4_database off IO thread.
    V4Database::Destroy(std::move(v4_database));
  }
}

void V4LocalDatabaseManager::DatabaseReadyForUpdates(
    const std::vector<ListIdentifier>& stores_to_reset) {
  if (enabled_) {
    v4_database_->ResetStores(stores_to_reset);
    UpdateListClientStates(GetStoreStateMap());

    // The database is ready to process updates. Schedule them now.
    v4_update_protocol_manager_->ScheduleNextUpdate(GetStoreStateMap());
  }
}

void V4LocalDatabaseManager::DatabaseUpdated() {
  if (enabled_) {
    v4_update_protocol_manager_->ScheduleNextUpdate(GetStoreStateMap());

    v4_database_->RecordFileSizeHistograms();
    UpdateListClientStates(GetStoreStateMap());

    base::PostTask(
        FROM_HERE, CreateTaskTraits(ThreadID::UI),
        base::BindOnce(
            &SafeBrowsingDatabaseManager::NotifyDatabaseUpdateFinished, this));
  }
}

void V4LocalDatabaseManager::DeleteUnusedStoreFiles() {
  for (auto* const store_filename_to_delete : kStoreFileNamesToDelete) {
    // Is the file marked for deletion also being used for a valid V4Store?
    auto it = std::find_if(std::begin(list_infos_), std::end(list_infos_),
                           [&store_filename_to_delete](ListInfo const& li) {
                             return li.filename() == store_filename_to_delete;
                           });
    if (list_infos_.end() == it) {
      const base::FilePath store_path =
          base_path_.AppendASCII(store_filename_to_delete);
      bool path_exists = base::PathExists(store_path);
      base::UmaHistogramBoolean("SafeBrowsing.V4UnusedStoreFileExists" +
                                    GetUmaSuffixForStore(store_path),
                                path_exists);
      if (!path_exists) {
        continue;
      }
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                    store_path, false /* recursive */));
    } else {
      NOTREACHED() << "Trying to delete a store file that's in use: "
                   << store_filename_to_delete;
    }
  }
}

void V4LocalDatabaseManager::GetArtificialPrefixMatches(
    const std::unique_ptr<PendingCheck>& check) {
  if (artificially_marked_store_and_hash_prefixes_.empty()) {
    return;
  }
  for (const auto& full_hash : check->full_hashes) {
    for (const StoreAndHashPrefix& artificial_store_and_hash_prefix :
         artificially_marked_store_and_hash_prefixes_) {
      FullHash artificial_full_hash =
          artificial_store_and_hash_prefix.hash_prefix;
      DCHECK_EQ(crypto::kSHA256Length, artificial_full_hash.size());
      if (artificial_full_hash == full_hash) {
        (check->artificial_full_hash_to_store_and_hash_prefixes)[full_hash] = {
            artificial_store_and_hash_prefix};
      }
    }
  }
}

bool V4LocalDatabaseManager::GetPrefixMatches(
    const std::unique_ptr<PendingCheck>& check) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  DCHECK(enabled_);

  check->full_hash_to_store_and_hash_prefixes.clear();
  for (const auto& full_hash : check->full_hashes) {
    StoreAndHashPrefixes matched_store_and_hash_prefixes;
    v4_database_->GetStoresMatchingFullHash(full_hash, check->stores_to_check,
                                            &matched_store_and_hash_prefixes);
    if (!matched_store_and_hash_prefixes.empty()) {
      (check->full_hash_to_store_and_hash_prefixes)[full_hash] =
          matched_store_and_hash_prefixes;
    }
  }

  return !check->full_hash_to_store_and_hash_prefixes.empty();
}

void V4LocalDatabaseManager::GetSeverestThreatTypeAndMetadata(
    const std::vector<FullHashInfo>& full_hash_infos,
    const std::vector<FullHash>& full_hashes,
    std::vector<SBThreatType>* full_hash_threat_types,
    SBThreatType* most_severe_threat_type,
    ThreatMetadata* metadata,
    FullHash* matching_full_hash) {
  UMA_HISTOGRAM_COUNTS_100("SafeBrowsing.V4LocalDatabaseManager.ThreatInfoSize",
                           full_hash_infos.size());
  ThreatSeverity most_severe_yet = kLeastSeverity;
  for (const FullHashInfo& fhi : full_hash_infos) {
    ThreatSeverity severity = GetThreatSeverity(fhi.list_id);
    SBThreatType threat_type = GetSBThreatTypeForList(fhi.list_id);

    const auto& it =
        std::find(full_hashes.begin(), full_hashes.end(), fhi.full_hash);
    DCHECK(it != full_hashes.end());
    (*full_hash_threat_types)[it - full_hashes.begin()] = threat_type;

    if (severity < most_severe_yet) {
      most_severe_yet = severity;
      *most_severe_threat_type = threat_type;
      *metadata = fhi.metadata;
      *matching_full_hash = fhi.full_hash;
    }
  }
}

StoresToCheck V4LocalDatabaseManager::GetStoresForFullHashRequests() {
  StoresToCheck stores_for_full_hash;
  for (const auto& info : list_infos_) {
    stores_for_full_hash.insert(info.list_id());
  }
  return stores_for_full_hash;
}

std::unique_ptr<StoreStateMap> V4LocalDatabaseManager::GetStoreStateMap() {
  return v4_database_->GetStoreStateMap();
}

// Returns the SBThreatType corresponding to a given SafeBrowsing list.
SBThreatType V4LocalDatabaseManager::GetSBThreatTypeForList(
    const ListIdentifier& list_id) {
  auto it = std::find_if(
      std::begin(list_infos_), std::end(list_infos_),
      [&list_id](ListInfo const& li) { return li.list_id() == list_id; });
  DCHECK(list_infos_.end() != it);
  DCHECK_NE(SB_THREAT_TYPE_SAFE, it->sb_threat_type());
  DCHECK_NE(SB_THREAT_TYPE_UNUSED, it->sb_threat_type());
  return it->sb_threat_type();
}

AsyncMatch V4LocalDatabaseManager::HandleWhitelistCheck(
    std::unique_ptr<PendingCheck> check) {
  // We don't bother queuing whitelist checks since the DB will
  // normally be available already -- whitelists are used after page load,
  // and navigations are blocked until the DB is ready and dequeues checks.
  // The caller should have already checked that the DB is ready.
  DCHECK(v4_database_);

  if (!GetPrefixMatches(check)) {
    return AsyncMatch::NO_MATCH;
  }

  // Look for any full-length hash in the matches. If there is one,
  // there's no need for a full-hash check. This saves bandwidth for
  // very popular sites since they'll have full-length hashes locally.
  // These loops will have exactly 1 entry most of the time.
  for (const auto& entry : check->full_hash_to_store_and_hash_prefixes) {
    for (const auto& store_and_prefix : entry.second) {
      if (store_and_prefix.hash_prefix.size() == kMaxHashPrefixLength) {
        return AsyncMatch::MATCH;
      }
    }
  }

  ScheduleFullHashCheck(std::move(check));
  return AsyncMatch::ASYNC;
}

bool V4LocalDatabaseManager::HandleCheck(std::unique_ptr<PendingCheck> check) {
  if (!v4_database_) {
    queued_checks_.push_back(std::move(check));
    return false;
  }

  GetPrefixMatches(check);
  GetArtificialPrefixMatches(check);
  if (check->full_hash_to_store_and_hash_prefixes.empty() &&
      check->artificial_full_hash_to_store_and_hash_prefixes.empty()) {
    return true;
  }

  ScheduleFullHashCheck(std::move(check));
  return false;
}

void V4LocalDatabaseManager::PopulateArtificialDatabase() {
  for (const auto& switch_and_threat_type : GetSwitchAndThreatTypes()) {
    const std::string raw_artificial_urls =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switch_and_threat_type.first);
    base::StringTokenizer tokenizer(raw_artificial_urls, ",");
    while (tokenizer.GetNext()) {
      ListIdentifier artificial_list_id(GetCurrentPlatformType(), URL,
                                        switch_and_threat_type.second);
      FullHash full_hash =
          V4ProtocolManagerUtil::GetFullHash(GURL(tokenizer.token()));
      artificially_marked_store_and_hash_prefixes_.emplace_back(
          artificial_list_id, full_hash);
    }
  }
}

void V4LocalDatabaseManager::ScheduleFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // Add check to pending_checks_ before scheduling PerformFullHashCheck so that
  // even if the client calls CancelCheck before PerformFullHashCheck gets
  // called, the check can be found in pending_checks_.
  pending_checks_.insert(check.get());

  // If the full hash matches one from the artificial list, don't send the
  // request to the server.
  if (!check->artificial_full_hash_to_store_and_hash_prefixes.empty()) {
    std::vector<FullHashInfo> full_hash_infos;
    for (const auto& entry :
         check->artificial_full_hash_to_store_and_hash_prefixes) {
      for (const auto& store_and_prefix : entry.second) {
        ListIdentifier list_id = store_and_prefix.list_id;
        base::Time next = base::Time::Now() + base::TimeDelta::FromMinutes(
                                                  kFullHashExpiryTimeInMinutes);
        full_hash_infos.emplace_back(entry.first, list_id, next);
      }
    }
    base::PostTask(FROM_HERE, CreateTaskTraits(ThreadID::IO),
                   base::BindOnce(&V4LocalDatabaseManager::OnFullHashResponse,
                                  weak_factory_.GetWeakPtr(), std::move(check),
                                  full_hash_infos));
  } else {
    // Post on the IO thread to enforce async behavior.
    base::PostTask(
        FROM_HERE, CreateTaskTraits(ThreadID::IO),
        base::BindOnce(&V4LocalDatabaseManager::PerformFullHashCheck,
                       weak_factory_.GetWeakPtr(), std::move(check)));
  }
}

bool V4LocalDatabaseManager::HandleHashSynchronously(
    const FullHash& hash,
    const StoresToCheck& stores_to_check) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  std::set<FullHash> hashes{hash};
  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check, hashes);

  return GetPrefixMatches(check);
}

bool V4LocalDatabaseManager::HandleUrlSynchronously(
    const GURL& url,
    const StoresToCheck& stores_to_check) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  std::unique_ptr<PendingCheck> check = std::make_unique<PendingCheck>(
      nullptr, ClientCallbackType::CHECK_OTHER, stores_to_check,
      std::vector<GURL>(1, url));

  return GetPrefixMatches(check);
}

void V4LocalDatabaseManager::OnFullHashResponse(
    std::unique_ptr<PendingCheck> check,
    const std::vector<FullHashInfo>& full_hash_infos) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  if (!enabled_) {
    DCHECK(pending_checks_.empty());
    return;
  }

  const auto it = pending_checks_.find(check.get());
  if (it == pending_checks_.end()) {
    // The check has since been cancelled.
    return;
  }

  // Find out the most severe threat, if any, to report to the client.
  GetSeverestThreatTypeAndMetadata(
      full_hash_infos, check->full_hashes, &check->full_hash_threat_types,
      &check->most_severe_threat_type, &check->url_metadata,
      &check->matching_full_hash);
  pending_checks_.erase(it);
  RespondToClient(std::move(check));
}

void V4LocalDatabaseManager::PerformFullHashCheck(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  DCHECK(enabled_);
  DCHECK(!check->full_hash_to_store_and_hash_prefixes.empty());

  FullHashToStoreAndHashPrefixesMap full_hash_to_store_and_hash_prefixes =
      check->full_hash_to_store_and_hash_prefixes;
  v4_get_hash_protocol_manager_->GetFullHashes(
      full_hash_to_store_and_hash_prefixes, list_client_states_,
      base::BindOnce(&V4LocalDatabaseManager::OnFullHashResponse,
                     weak_factory_.GetWeakPtr(), std::move(check)));
}

void V4LocalDatabaseManager::ProcessQueuedChecks() {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);

  for (auto& it : checks) {
    if (!GetPrefixMatches(it)) {
      RespondToClient(std::move(it));
    } else {
      pending_checks_.insert(it.get());
      PerformFullHashCheck(std::move(it));
    }
  }
}

void V4LocalDatabaseManager::RespondSafeToQueuedChecks() {
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // Steal the queue to protect against reentrant CancelCheck() calls.
  QueuedChecks checks;
  checks.swap(queued_checks_);

  for (std::unique_ptr<PendingCheck>& it : checks) {
    RespondToClient(std::move(it));
  }
}

void V4LocalDatabaseManager::RespondToClient(
    std::unique_ptr<PendingCheck> check) {
  DCHECK(check);

  switch (check->client_callback_type) {
    case ClientCallbackType::CHECK_BROWSE_URL:
    case ClientCallbackType::CHECK_URL_FOR_SUBRESOURCE_FILTER:
      DCHECK_EQ(1u, check->urls.size());
      check->client->OnCheckBrowseUrlResult(
          check->urls[0], check->most_severe_threat_type, check->url_metadata);
      break;

    case ClientCallbackType::CHECK_DOWNLOAD_URLS:
      check->client->OnCheckDownloadUrlResult(check->urls,
                                              check->most_severe_threat_type);
      break;

    case ClientCallbackType::CHECK_HIGH_CONFIDENCE_ALLOWLIST: {
      DCHECK_EQ(1u, check->urls.size());
      bool did_match_allowlist = check->most_severe_threat_type ==
                                 SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST;
      DCHECK(did_match_allowlist ||
             check->most_severe_threat_type == SB_THREAT_TYPE_SAFE);
      check->client->OnCheckUrlForHighConfidenceAllowlist(did_match_allowlist);
      break;
    }

    case ClientCallbackType::CHECK_RESOURCE_URL:
      DCHECK_EQ(1u, check->urls.size());
      check->client->OnCheckResourceUrlResult(check->urls[0],
                                              check->most_severe_threat_type,
                                              check->matching_full_hash);
      break;

    case ClientCallbackType::CHECK_CSD_WHITELIST: {
      DCHECK_EQ(1u, check->urls.size());
      bool did_match_allowlist =
          check->most_severe_threat_type == SB_THREAT_TYPE_CSD_WHITELIST;
      DCHECK(did_match_allowlist ||
             check->most_severe_threat_type == SB_THREAT_TYPE_SAFE);
      check->client->OnCheckWhitelistUrlResult(did_match_allowlist);
      break;
    }

    case ClientCallbackType::CHECK_EXTENSION_IDS: {
      DCHECK_EQ(check->full_hash_threat_types.size(),
                check->full_hashes.size());
      std::set<FullHash> unsafe_extension_ids;
      for (size_t i = 0; i < check->full_hash_threat_types.size(); i++) {
        if (check->full_hash_threat_types[i] == SB_THREAT_TYPE_EXTENSION) {
          unsafe_extension_ids.insert(check->full_hashes[i]);
        }
      }
      check->client->OnCheckExtensionsResult(unsafe_extension_ids);
      break;
    }
    case ClientCallbackType::CHECK_OTHER:
      NOTREACHED() << "Unexpected client_callback_type encountered";
  }
}

void V4LocalDatabaseManager::SetupDatabase() {
  DCHECK(!base_path_.empty());
  DCHECK(!list_infos_.empty());
  DCHECK(CurrentlyOnThread(ThreadID::IO));

  // Do not create the database on the IO thread since this may be an expensive
  // operation. Instead, do that on the task_runner and when the new database
  // has been created, swap it out on the IO thread.
  NewDatabaseReadyCallback db_ready_callback =
      base::BindOnce(&V4LocalDatabaseManager::DatabaseReadyForChecks,
                     weak_factory_.GetWeakPtr());
  V4Database::Create(task_runner_, base_path_, list_infos_,
                     std::move(db_ready_callback));
}

void V4LocalDatabaseManager::SetupUpdateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const V4ProtocolConfig& config) {
  V4UpdateCallback update_callback =
      base::BindRepeating(&V4LocalDatabaseManager::UpdateRequestCompleted,
                          weak_factory_.GetWeakPtr());

  v4_update_protocol_manager_ = V4UpdateProtocolManager::Create(
      url_loader_factory, config, update_callback,
      extended_reporting_level_callback_);
}

void V4LocalDatabaseManager::UpdateRequestCompleted(
    std::unique_ptr<ParsedServerResponse> parsed_server_response) {
  DCHECK(CurrentlyOnThread(ThreadID::IO));
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            db_updated_callback_);
}

bool V4LocalDatabaseManager::AreAllStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  StoreAvailabilityResult result = StoreAvailabilityResult::AVAILABLE;
  if (!enabled_) {
    result = StoreAvailabilityResult::NOT_ENABLED;
  } else if (!v4_database_) {
    result = StoreAvailabilityResult::DATABASE_UNAVAILABLE;
  } else if (!v4_database_->AreAllStoresAvailable(stores_to_check)) {
    result = StoreAvailabilityResult::STORE_UNAVAILABLE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4LocalDatabaseManager.AreAllStoresAvailableNow", result,
      StoreAvailabilityResult::COUNT);
  return (result == StoreAvailabilityResult::AVAILABLE);
}

bool V4LocalDatabaseManager::AreAnyStoresAvailableNow(
    const StoresToCheck& stores_to_check) const {
  return enabled_ && v4_database_ &&
         v4_database_->AreAnyStoresAvailable(stores_to_check);
}

void V4LocalDatabaseManager::UpdateListClientStates(
    const std::unique_ptr<StoreStateMap>& store_state_map) {
  list_client_states_.clear();
  V4ProtocolManagerUtil::GetListClientStatesFromStoreStateMap(
      store_state_map, &list_client_states_);
}

}  // namespace safe_browsing
