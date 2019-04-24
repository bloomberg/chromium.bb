// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_impl.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/top_sites_cache.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/history/core/browser/top_sites_provider.h"
#include "components/history/core/browser/url_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace history {
namespace {

void RunOrPostGetMostVisitedURLsCallback(
    base::TaskRunner* task_runner,
    const TopSitesImpl::GetMostVisitedURLsCallback& callback,
    const MostVisitedURLList& urls) {
  if (task_runner->RunsTasksInCurrentSequence())
    callback.Run(urls);
  else
    task_runner->PostTask(FROM_HERE, base::BindOnce(callback, urls));
}

// Checks if the titles stored in |old_list| and |new_list| have changes.
bool DoTitlesDiffer(const MostVisitedURLList& old_list,
                    const MostVisitedURLList& new_list) {
  // If the two lists have different sizes, the most visited titles are
  // considered to have changes.
  if (old_list.size() != new_list.size())
    return true;

  return !std::equal(std::begin(old_list), std::end(old_list),
                     std::begin(new_list),
                     [](const auto& old_item_ptr, const auto& new_item_ptr) {
                       return old_item_ptr.title == new_item_ptr.title;
                     });
}

// The delay for the first HistoryService query at startup.
constexpr base::TimeDelta kFirstDelayAtStartup =
    base::TimeDelta::FromSeconds(15);

// The delay for the all HistoryService queries other than the first one.
#if defined(OS_IOS) || defined(OS_ANDROID)
// On mobile, having the max at 60 minutes results in the topsites database
// being not updated often enough since the app isn't usually running for long
// stretches of time.
constexpr base::TimeDelta kDelayForUpdates = base::TimeDelta::FromMinutes(5);
#else
constexpr base::TimeDelta kDelayForUpdates = base::TimeDelta::FromMinutes(60);
#endif  // defined(OS_IOS) || defined(OS_ANDROID)

// Key for preference listing the URLs that should not be shown as most visited
// tiles.
const char kMostVisitedURLsBlacklist[] = "ntp.most_visited_blacklist";

}  // namespace

// Initially, histogram is not recorded.
bool TopSitesImpl::histogram_recorded_ = false;

TopSitesImpl::TopSitesImpl(PrefService* pref_service,
                           HistoryService* history_service,
                           std::unique_ptr<TopSitesProvider> provider,
                           const PrepopulatedPageList& prepopulated_pages,
                           const CanAddURLToHistoryFn& can_add_url_to_history)
    : backend_(nullptr),
      cache_(std::make_unique<TopSitesCache>()),
      thread_safe_cache_(std::make_unique<TopSitesCache>()),
      prepopulated_pages_(prepopulated_pages),
      pref_service_(pref_service),
      history_service_(history_service),
      provider_(std::move(provider)),
      can_add_url_to_history_(can_add_url_to_history),
      loaded_(false),
      history_service_observer_(this) {
  DCHECK(pref_service_);
  DCHECK(!can_add_url_to_history_.is_null());
  DCHECK(provider_);
}

void TopSitesImpl::Init(const base::FilePath& db_name) {
  // Create the backend here, rather than in the constructor, so unit tests that
  // do not need the backend can run without a problem.
  backend_ = new TopSitesBackend();
  backend_->Init(db_name);
  backend_->GetMostVisitedSites(
      base::BindOnce(&TopSitesImpl::OnGotMostVisitedURLs,
                     base::Unretained(this)),
      &cancelable_task_tracker_);
}

// WARNING: this function may be invoked on any thread.
void TopSitesImpl::GetMostVisitedURLs(
    const GetMostVisitedURLsCallback& callback) {
  MostVisitedURLList filtered_urls;
  {
    base::AutoLock lock(lock_);
    if (!loaded_) {
      // A request came in before we finished loading. Store the callback and
      // we'll run it on current thread when we finish loading.
      pending_callbacks_.push_back(base::Bind(
          &RunOrPostGetMostVisitedURLsCallback,
          base::RetainedRef(base::ThreadTaskRunnerHandle::Get()), callback));
      return;
    }
    filtered_urls = thread_safe_cache_->top_sites();
  }
  callback.Run(filtered_urls);
}

// Returns the index of |url| in |urls|, or -1 if not found.
static int IndexOf(const MostVisitedURLList& urls, const GURL& url) {
  for (size_t i = 0; i < urls.size(); i++) {
    if (urls[i].url == url)
      return i;
  }
  return -1;
}

void TopSitesImpl::SyncWithHistory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (loaded_)
    StartQueryForMostVisited();
}

bool TopSitesImpl::HasBlacklistedItems() const {
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return blacklist && !blacklist->empty();
}

void TopSitesImpl::AddBlacklistedURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto dummy = std::make_unique<base::Value>();
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->SetWithoutPathExpansion(GetURLHash(url), std::move(dummy));
  }

  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

void TopSitesImpl::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->RemoveWithoutPathExpansion(GetURLHash(url), nullptr);
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

bool TopSitesImpl::IsBlacklisted(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return blacklist && blacklist->HasKey(GetURLHash(url));
}

void TopSitesImpl::ClearBlacklistedURLs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->Clear();
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

bool TopSitesImpl::IsKnownURL(const GURL& url) {
  return loaded_ && cache_->IsKnownURL(url);
}

bool TopSitesImpl::IsFull() {
  return loaded_ && cache_->GetNumURLs() >= kTopSitesNumber;
}

PrepopulatedPageList TopSitesImpl::GetPrepopulatedPages() {
  return prepopulated_pages_;
}

bool TopSitesImpl::loaded() const {
  return loaded_;
}

void TopSitesImpl::OnNavigationCommitted(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!loaded_)
    return;

  if (can_add_url_to_history_.Run(url))
    ScheduleUpdateTimer();
}

void TopSitesImpl::ShutdownOnUIThread() {
  history_service_ = nullptr;
  history_service_observer_.RemoveAll();
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  cancelable_task_tracker_.TryCancelAll();
  if (backend_)
    backend_->Shutdown();
}

// static
void TopSitesImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMostVisitedURLsBlacklist);
}

TopSitesImpl::~TopSitesImpl() = default;

void TopSitesImpl::StartQueryForMostVisited() {
  DCHECK(loaded_);
  timer_.Stop();

  if (!history_service_)
    return;

  provider_->ProvideTopSites(
      num_results_to_request_from_history(),
      base::Bind(&TopSitesImpl::OnTopSitesAvailableFromHistory,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

// static
void TopSitesImpl::DiffMostVisited(const MostVisitedURLList& old_list,
                                   const MostVisitedURLList& new_list,
                                   TopSitesDelta* delta) {
  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  for (size_t i = 0; i < old_list.size(); i++)
    all_old_urls[old_list[i].url] = i;

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  const size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  int rank = -1;
  for (size_t i = 0; i < new_list.size(); i++) {
    rank++;
    auto found = all_old_urls.find(new_list[i].url);
    if (found == all_old_urls.end()) {
      MostVisitedURLWithRank added;
      added.url = new_list[i];
      added.rank = rank;
      delta->added.push_back(added);
    } else {
      DCHECK(found->second != kAlreadyFoundMarker)
          << "Same URL appears twice in the new list.";
      int old_rank = found->second;
      if (old_rank != rank) {
        MostVisitedURLWithRank moved;
        moved.url = new_list[i];
        moved.rank = rank;
        delta->moved.push_back(moved);
      }
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (const std::pair<GURL, size_t>& old_url : all_old_urls) {
    if (old_url.second != kAlreadyFoundMarker)
      delta->deleted.push_back(old_list[old_url.second]);
  }
}

// static
int TopSitesImpl::GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                            const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

bool TopSitesImpl::AddPrepopulatedPages(MostVisitedURLList* urls) const {
  bool added = false;
  for (const auto& prepopulated_page : prepopulated_pages_) {
    if (urls->size() < kTopSitesNumber &&
        IndexOf(*urls, prepopulated_page.most_visited.url) == -1) {
      urls->push_back(prepopulated_page.most_visited);
      added = true;
    }
  }
  return added;
}

void TopSitesImpl::ApplyBlacklist(const MostVisitedURLList& urls,
                                  MostVisitedURLList* out) {
  // Log the number of times ApplyBlacklist is called so we can compute the
  // average number of blacklisted items per user.
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  UMA_HISTOGRAM_BOOLEAN("TopSites.NumberOfApplyBlacklist", true);
  UMA_HISTOGRAM_COUNTS_100("TopSites.NumberOfBlacklistedItems",
      (blacklist ? blacklist->size() : 0));
  size_t num_urls = 0;
  for (size_t i = 0; i < urls.size(); ++i) {
    if (!IsBlacklisted(urls[i].url)) {
      if (num_urls >= kTopSitesNumber)
        break;
      num_urls++;
      out->push_back(urls[i]);
    }
  }
}

// static
std::string TopSitesImpl::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return base::MD5String(url.spec());
}

void TopSitesImpl::SetTopSites(const MostVisitedURLList& new_top_sites,
                               const CallLocation location) {
  DCHECK(thread_checker_.CalledOnValidThread());

  MostVisitedURLList top_sites(new_top_sites);
  AddPrepopulatedPages(&top_sites);

  TopSitesDelta delta;
  DiffMostVisited(cache_->top_sites(), top_sites, &delta);

  TopSitesBackend::RecordHistogram record_or_not =
      TopSitesBackend::RECORD_HISTOGRAM_NO;

  // Record the delta size into a histogram if this function is called from
  // function OnGotMostVisitedURLs and no histogram value has been recorded
  // before.
  if (location == CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_URLS &&
      !histogram_recorded_) {
    size_t delta_size =
        delta.deleted.size() + delta.added.size() + delta.moved.size();
    UMA_HISTOGRAM_COUNTS_100("History.FirstSetTopSitesDeltaSize", delta_size);
    // Will be passed to TopSitesBackend to let it record the histogram too.
    record_or_not = TopSitesBackend::RECORD_HISTOGRAM_YES;
    // Change it to true so that the histogram will not be recorded any more.
    histogram_recorded_ = true;
  }

  bool should_notify_observers = false;
  // If there is a change in urls, update the db and notify observers.
  if (!delta.deleted.empty() || !delta.added.empty() || !delta.moved.empty()) {
    backend_->UpdateTopSites(delta, record_or_not);
    should_notify_observers = true;
  }
  // If there is no url change in top sites, check if the titles have changes.
  // Notify observers if there's a change in titles.
  if (!should_notify_observers)
    should_notify_observers = DoTitlesDiffer(cache_->top_sites(), top_sites);

  // We always do the following steps (setting top sites in cache, and resetting
  // thread safe cache ...) as this method is invoked during startup at which
  // point the caches haven't been updated yet.
  cache_->SetTopSites(top_sites);

  ResetThreadSafeCache();

  if (should_notify_observers)
    NotifyTopSitesChanged(TopSitesObserver::ChangeReason::MOST_VISITED);
}

int TopSitesImpl::num_results_to_request_from_history() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return kTopSitesNumber + (blacklist ? blacklist->size() : 0);
}

void TopSitesImpl::MoveStateToLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  MostVisitedURLList urls;
  PendingCallbacks pending_callbacks;
  {
    base::AutoLock lock(lock_);

    if (loaded_)
      return;  // Don't do anything if we're already loaded.
    loaded_ = true;

    // Now that we're loaded we can service the queued up callbacks. Copy them
    // here and service them outside the lock.
    if (!pending_callbacks_.empty()) {
      urls = thread_safe_cache_->top_sites();
      pending_callbacks.swap(pending_callbacks_);
    }
  }

  for (size_t i = 0; i < pending_callbacks.size(); i++)
    pending_callbacks[i].Run(urls);

  if (history_service_)
    history_service_observer_.Add(history_service_);

  NotifyTopSitesLoaded();
}

void TopSitesImpl::ResetThreadSafeCache() {
  base::AutoLock lock(lock_);
  MostVisitedURLList cached;
  ApplyBlacklist(cache_->top_sites(), &cached);
  thread_safe_cache_->SetTopSites(cached);
}

void TopSitesImpl::ScheduleUpdateTimer() {
  if (timer_.IsRunning())
    return;

  timer_.Start(FROM_HERE, kDelayForUpdates, this,
               &TopSitesImpl::StartQueryForMostVisited);
}

void TopSitesImpl::OnGotMostVisitedURLs(
    const scoped_refptr<MostVisitedThreadSafe>& sites) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Set the top sites directly in the cache so that SetTopSites diffs
  // correctly.
  cache_->SetTopSites(sites->data);
  SetTopSites(sites->data, CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_URLS);

  MoveStateToLoaded();

  // Start a timer that refreshes top sites from history.
  timer_.Start(FROM_HERE, kFirstDelayAtStartup, this,
               &TopSitesImpl::StartQueryForMostVisited);
}

void TopSitesImpl::OnTopSitesAvailableFromHistory(
    const MostVisitedURLList* pages) {
  DCHECK(pages);
  SetTopSites(*pages, CALL_LOCATION_FROM_OTHER_PLACES);
}

void TopSitesImpl::OnURLsDeleted(HistoryService* history_service,
                                 const DeletionInfo& deletion_info) {
  if (!loaded_)
    return;

  if (deletion_info.IsAllHistory()) {
    SetTopSites(MostVisitedURLList(), CALL_LOCATION_FROM_OTHER_PLACES);
    backend_->ResetDatabase();
  } else {
    std::set<size_t> indices_to_delete;  // Indices into top_sites_.
    for (const auto& row : deletion_info.deleted_rows()) {
      if (cache_->IsKnownURL(row.url()))
        indices_to_delete.insert(cache_->GetURLIndex(row.url()));
    }

    if (indices_to_delete.empty())
      return;

    MostVisitedURLList new_top_sites(cache_->top_sites());
    for (auto i = indices_to_delete.rbegin(); i != indices_to_delete.rend();
         i++) {
      new_top_sites.erase(new_top_sites.begin() + *i);
    }
    SetTopSites(new_top_sites, CALL_LOCATION_FROM_OTHER_PLACES);
  }
  StartQueryForMostVisited();
}

}  // namespace history
