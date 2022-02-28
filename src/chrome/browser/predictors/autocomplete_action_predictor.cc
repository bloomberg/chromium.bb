// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor.h"

#include <math.h>
#include <stddef.h>
#include <queue>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"

namespace {

const float kConfidenceCutoff[] = {
  0.8f,
  0.5f
};

static_assert(base::size(kConfidenceCutoff) ==
                  predictors::AutocompleteActionPredictor::LAST_PREDICT_ACTION,
              "kConfidenceCutoff count should match LAST_PREDICT_ACTION");

const int kMinimumNumberOfHits = 3;
const size_t kMaximumTransitionalMatchesSize = 1024 * 1024;  // 1 MB.

// As of February 2019, 99% of users on Windows have less than 2000 entries in
// the database.
const size_t kMaximumCacheSize = 2000;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DatabaseAction {
  kAdd = 0,
  kUpdate = 1,
  kDeleteSome = 2,
  kDeleteAll = 3,
  kMaxValue = kDeleteAll,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NoStatePrefetchStatus {
  // No state prefetch was not started at all for this omnibox interaction.
  kNotStarted = 0,
  // The no state prefetch was cancelled because the user did not select a URL
  // in the omnibox.
  kCancelled = 1,
  // The no state prefetch was unused because the user navigated to a different
  // URL.
  kUnused = 2,
  // The no state prefetch was used and had time to finish before the user
  // selected a URL.
  kHitFinished = 3,
  // The no state prefetch was used but had not completed before the user
  // selected a URL.
  kHitUnfinished = 4,
  kMaxValue = kHitUnfinished,
};

}  // namespace

namespace predictors {

const int AutocompleteActionPredictor::kMaximumDaysToKeepEntry = 14;
const size_t AutocompleteActionPredictor::kMinimumUserTextLength = 1;
const size_t AutocompleteActionPredictor::kMaximumStringLength = 1024;

AutocompleteActionPredictor::AutocompleteActionPredictor(Profile* profile)
    : profile_(profile),
      main_profile_predictor_(nullptr),
      incognito_predictor_(nullptr),
      initialized_(false) {
  if (profile_->IsOffTheRecord()) {
    main_profile_predictor_ = AutocompleteActionPredictorFactory::GetForProfile(
        profile_->GetOriginalProfile());
    DCHECK(main_profile_predictor_);
    main_profile_predictor_->incognito_predictor_ = this;
    if (main_profile_predictor_->initialized_)
      CopyFromMainProfile();
  } else {
    // Request the in-memory database from the history to force it to load so
    // it's available as soon as possible.
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service)
      history_service->InMemoryDatabase();

    table_ =
        PredictorDatabaseFactory::GetForProfile(profile_)->autocomplete_table();

    // Create local caches using the database as loaded. We will garbage collect
    // rows from the caches and the database once the history service is
    // available.
    auto rows =
        std::make_unique<std::vector<AutocompleteActionPredictorTable::Row>>();
    auto* rows_ptr = rows.get();
    table_->GetTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&AutocompleteActionPredictorTable::GetAllRows, table_,
                       rows_ptr),
        base::BindOnce(&AutocompleteActionPredictor::CreateCaches, AsWeakPtr(),
                       std::move(rows)));
  }
}

AutocompleteActionPredictor::~AutocompleteActionPredictor() {
  if (main_profile_predictor_)
    main_profile_predictor_->incognito_predictor_ = nullptr;
  else if (incognito_predictor_)
    incognito_predictor_->main_profile_predictor_ = nullptr;
  if (no_state_prefetch_handle_.get())
    no_state_prefetch_handle_->OnCancel();
}

void AutocompleteActionPredictor::RegisterTransitionalMatches(
    const std::u16string& user_text,
    const AutocompleteResult& result) {
  if (user_text.length() < kMinimumUserTextLength ||
      user_text.length() > kMaximumStringLength) {
    return;
  }
  const std::u16string lower_user_text(base::i18n::ToLower(user_text));

  // Merge this in to an existing match if we already saw |user_text|
  auto match_it = std::find(transitional_matches_.begin(),
                            transitional_matches_.end(), lower_user_text);

  if (match_it == transitional_matches_.end()) {
    if (transitional_matches_size_ + lower_user_text.length() >
        kMaximumTransitionalMatchesSize) {
      return;
    }
    transitional_matches_.emplace_back(lower_user_text);
    transitional_matches_size_ += lower_user_text.length();
    match_it = transitional_matches_.end() - 1;
  }

  for (const auto& match : result) {
    const GURL& url = match.destination_url;
    const size_t size = url.spec().size();
    if (!base::Contains(match_it->urls, url) && size <= kMaximumStringLength &&
        transitional_matches_size_ + size <= kMaximumTransitionalMatchesSize) {
      match_it->urls.push_back(url);
      transitional_matches_size_ += size;
    }
  }
}

void AutocompleteActionPredictor::ClearTransitionalMatches() {
  transitional_matches_.clear();
  transitional_matches_size_ = 0;
}

void AutocompleteActionPredictor::CancelPrerender() {
  // If the prefetch has already been abandoned, leave it to its own timeout;
  // this normally gets called immediately after OnOmniboxOpenedUrl.
  if (no_state_prefetch_handle_ && !no_state_prefetch_handle_->IsAbandoned()) {
    UMA_HISTOGRAM_ENUMERATION(
        "AutocompleteActionPredictor.NoStatePrefetchStatus",
        NoStatePrefetchStatus::kCancelled);
    no_state_prefetch_handle_->OnCancel();
    no_state_prefetch_handle_.reset();
  }

  // TODO(https://crbug.com/1166085): Find a proper way to reset
  // prerender_handle_ here.
}

void AutocompleteActionPredictor::StartPrerendering(
    const GURL& url,
    content::WebContents& web_contents,
    const gfx::Size& size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (blink::features::IsPrerender2Enabled() &&
      base::FeatureList::IsEnabled(features::kOmniboxTriggerForPrerender2)) {
    // Check whether preloading is enabled. If users disable this
    // setting, it means users do not want to preload pages.
    // TODO(https://crbug.com/1269204): Move this check into
    // WebContentsDelegate::IsPrerender2Supported after exposing TriggerType to
    // embedders.
    if (!prefetch::IsSomePreloadingEnabled(*profile_->GetPrefs())) {
      return;
    }

    // TODO(https://crbug.com/1166085): Reset the handle upon prerendering
    // activation/cancellation. There is a case that a user input the same url
    // after activation/cancellation, and this mechanism prevents the user from
    // starting a new prerendering.
    if (prerender_handle_) {
      // `url` has already been prerendered. Avoid starting new prerendering.
      if (prerender_handle_->GetInitialPrerenderingUrl() == url) {
        return;
      }
      // `url` does not matched with previously prerendered url. Reset the
      // handle to trigger cancellation.
      prerender_handle_.reset();
    }

    prerender_handle_ = web_contents.StartPrerendering(
        url, content::PrerenderTriggerType::kEmbedder, "_DirectURLInput");
  } else if (base::FeatureList::IsEnabled(
                 features::kOmniboxTriggerForNoStatePrefetch)) {
    content::SessionStorageNamespace* session_storage_namespace =
        web_contents.GetController().GetDefaultSessionStorageNamespace();
    // Only cancel the old prefetch after starting the new one, so if the URLs
    // are the same, the underlying prefetcher will be reused.
    std::unique_ptr<prerender::NoStatePrefetchHandle>
        old_no_state_prefetch_handle = std::move(no_state_prefetch_handle_);
    prerender::NoStatePrefetchManager* no_state_prefetch_manager =
        prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
            profile_);
    if (no_state_prefetch_manager) {
      no_state_prefetch_handle_ =
          no_state_prefetch_manager->StartPrefetchingFromOmnibox(
              url, session_storage_namespace, size);
    }
    if (old_no_state_prefetch_handle)
      old_no_state_prefetch_handle->OnCancel();
  }
}

AutocompleteActionPredictor::Action
AutocompleteActionPredictor::RecommendAction(
    const std::u16string& user_text,
    const AutocompleteMatch& match) const {
  bool is_in_db = false;
  const double confidence = CalculateConfidence(user_text, match, &is_in_db);
  DCHECK(confidence >= 0.0 && confidence <= 1.0);

  UMA_HISTOGRAM_BOOLEAN("AutocompleteActionPredictor.MatchIsInDb", is_in_db);

  if (is_in_db) {
    UMA_HISTOGRAM_COUNTS_100("AutocompleteActionPredictor.Confidence",
                             confidence * 100);
  }

  // Map the confidence to an action.
  Action action = ACTION_NONE;
  for (int i = 0; i < LAST_PREDICT_ACTION; ++i) {
    if (confidence >= kConfidenceCutoff[i]) {
      action = static_cast<Action>(i);
      break;
    }
  }

  // Downgrade prefetch to preconnect if this is a search match.
  if (action == ACTION_PRERENDER && AutocompleteMatch::IsSearchType(match.type))
    action = ACTION_PRECONNECT;

  return action;
}

// static
bool AutocompleteActionPredictor::IsPreconnectable(
    const AutocompleteMatch& match) {
  return AutocompleteMatch::IsSearchType(match.type);
}

void AutocompleteActionPredictor::OnOmniboxOpenedUrl(const OmniboxLog& log) {
  if (!initialized_)
    return;

  // TODO(dominich): The body of this method doesn't need to be run
  // synchronously. Investigate posting it as a task to be run later.

  if (log.text.length() < kMinimumUserTextLength ||
      log.text.length() > kMaximumStringLength) {
    return;
  }

  // Do not attempt to learn from omnibox interactions where the omnibox
  // dropdown is closed.  In these cases the user text (|log.text|) that we
  // learn from is either empty or effectively identical to the destination
  // string.  In either case, it can't teach us much.  Also do not attempt
  // to learn from paste-and-go actions even if the popup is open because
  // the paste-and-go destination has no relation to whatever text the user
  // may have typed.
  if (!log.is_popup_open || log.is_paste_and_go)
    return;

  const AutocompleteMatch& match = log.result.match_at(log.selected_index);
  const GURL& opened_url = match.destination_url;

  // Abandon the current prefetch. If it is to be used, it will be used very
  // soon, so use the lower timeout.
  if (no_state_prefetch_handle_) {
    if (no_state_prefetch_handle_->contents() &&
        no_state_prefetch_handle_->contents()->prerender_url() == opened_url) {
      if (no_state_prefetch_handle_->IsFinishedLoading()) {
        UMA_HISTOGRAM_ENUMERATION(
            "AutocompleteActionPredictor.NoStatePrefetchStatus",
            NoStatePrefetchStatus::kHitFinished);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "AutocompleteActionPredictor.NoStatePrefetchStatus",
            NoStatePrefetchStatus::kHitUnfinished);
      }
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "AutocompleteActionPredictor.NoStatePrefetchStatus",
          NoStatePrefetchStatus::kUnused);
    }
    no_state_prefetch_handle_->OnNavigateAway();
    // Don't release |no_state_prefetch_handle_| so it is canceled if it
    // survives to the next StartPrerendering call.
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "AutocompleteActionPredictor.NoStatePrefetchStatus",
        NoStatePrefetchStatus::kNotStarted);
  }

  // TODO(https://crbug.com/1166085): Find a proper way to reset
  // prerender_handle_ here.

  UpdateDatabaseFromTransitionalMatches(opened_url);
}

void AutocompleteActionPredictor::UpdateDatabaseFromTransitionalMatches(
    const GURL& opened_url) {
  std::vector<AutocompleteActionPredictorTable::Row> rows_to_add;
  std::vector<AutocompleteActionPredictorTable::Row> rows_to_update;
  for (const TransitionalMatch& transitional_match : transitional_matches_) {
    DCHECK_GE(transitional_match.user_text.length(), kMinimumUserTextLength);
    DCHECK_LE(transitional_match.user_text.length(), kMaximumStringLength);
    // Add entries to the database for those matches.
    for (const GURL& url : transitional_match.urls) {
      DCHECK_LE(url.spec().length(), kMaximumStringLength);

      const DBCacheKey key = {transitional_match.user_text, url};
      const bool is_hit = !opened_url.is_empty() && (url == opened_url);

      AutocompleteActionPredictorTable::Row row;
      row.user_text = key.user_text;
      row.url = key.url;

      auto it = db_cache_.find(key);
      if (it == db_cache_.end()) {
        row.id = base::GenerateGUID();
        row.number_of_hits = is_hit ? 1 : 0;
        row.number_of_misses = is_hit ? 0 : 1;

        rows_to_add.push_back(row);
      } else {
        DCHECK(db_id_cache_.find(key) != db_id_cache_.end());
        row.id = db_id_cache_.find(key)->second;
        row.number_of_hits = it->second.number_of_hits + (is_hit ? 1 : 0);
        row.number_of_misses = it->second.number_of_misses + (is_hit ? 0 : 1);

        rows_to_update.push_back(row);
      }
    }
  }
  if (!rows_to_add.empty() || !rows_to_update.empty())
    AddAndUpdateRows(rows_to_add, rows_to_update);

  std::vector<AutocompleteActionPredictorTable::Row::Id> ids_to_delete;
  if (db_cache_.size() > kMaximumCacheSize) {
    DeleteLowestConfidenceRowsFromCaches(db_cache_.size() - kMaximumCacheSize,
                                         &ids_to_delete);
  }

  if (!ids_to_delete.empty() && table_.get()) {
    table_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AutocompleteActionPredictorTable::DeleteRows,
                                  table_, std::move(ids_to_delete)));
  }

  ClearTransitionalMatches();
}

void AutocompleteActionPredictor::DeleteAllRows() {
  DCHECK(initialized_);

  db_cache_.clear();
  db_id_cache_.clear();

  if (table_.get()) {
    table_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AutocompleteActionPredictorTable::DeleteAllRows,
                       table_));
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                            DatabaseAction::kDeleteAll);
}

void AutocompleteActionPredictor::DeleteRowsFromCaches(
    const history::URLRows& rows,
    std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
  DCHECK(initialized_);
  DCHECK(id_list);

  for (auto it = db_cache_.begin(); it != db_cache_.end();) {
    if (std::find_if(rows.begin(), rows.end(),
                     history::URLRow::URLRowHasURL(it->first.url)) !=
        rows.end()) {
      const DBIdCacheMap::iterator id_it = db_id_cache_.find(it->first);
      DCHECK(id_it != db_id_cache_.end());
      id_list->push_back(id_it->second);
      db_id_cache_.erase(id_it);
      db_cache_.erase(it++);
    } else {
      ++it;
    }
  }
}

void AutocompleteActionPredictor::AddAndUpdateRows(
    const AutocompleteActionPredictorTable::Rows& rows_to_add,
    const AutocompleteActionPredictorTable::Rows& rows_to_update) {
  if (!initialized_)
    return;

  for (auto it = rows_to_add.begin(); it != rows_to_add.end(); ++it) {
    const DBCacheKey key = { it->user_text, it->url };
    DBCacheValue value = { it->number_of_hits, it->number_of_misses };

    DCHECK(db_cache_.find(key) == db_cache_.end());

    db_cache_[key] = value;
    db_id_cache_[key] = it->id;
    UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                              DatabaseAction::kAdd);
  }
  for (auto it = rows_to_update.begin(); it != rows_to_update.end(); ++it) {
    const DBCacheKey key = { it->user_text, it->url };

    auto db_it = db_cache_.find(key);
    DCHECK(db_it != db_cache_.end());
    DCHECK(db_id_cache_.find(key) != db_id_cache_.end());

    db_it->second.number_of_hits = it->number_of_hits;
    db_it->second.number_of_misses = it->number_of_misses;
    UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                              DatabaseAction::kUpdate);
  }

  if (table_.get()) {
    table_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AutocompleteActionPredictorTable::AddAndUpdateRows,
                       table_, rows_to_add, rows_to_update));
  }
}

void AutocompleteActionPredictor::CreateCaches(
    std::unique_ptr<std::vector<AutocompleteActionPredictorTable::Row>> rows) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(!initialized_);
  DCHECK(db_cache_.empty());
  DCHECK(db_id_cache_.empty());

  for (std::vector<AutocompleteActionPredictorTable::Row>::const_iterator it =
       rows->begin(); it != rows->end(); ++it) {
    const DBCacheKey key = { it->user_text, it->url };
    const DBCacheValue value = { it->number_of_hits, it->number_of_misses };
    db_cache_[key] = value;
    db_id_cache_[key] = it->id;
  }

  // If the history service is ready, delete any old or invalid entries.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    TryDeleteOldEntries(history_service);
    history_service_observation_.Observe(history_service);
  }
}

void AutocompleteActionPredictor::TryDeleteOldEntries(
    history::HistoryService* service) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(!initialized_);

  if (!service)
    return;

  history::URLDatabase* url_db = service->InMemoryDatabase();
  if (!url_db)
    return;

  DeleteOldEntries(url_db);
}

void AutocompleteActionPredictor::DeleteOldEntries(
    history::URLDatabase* url_db) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(!initialized_);
  DCHECK(table_.get());

  std::vector<AutocompleteActionPredictorTable::Row::Id> ids_to_delete;
  DeleteOldIdsFromCaches(url_db, &ids_to_delete);

  if (db_cache_.size() > kMaximumCacheSize) {
    DeleteLowestConfidenceRowsFromCaches(db_cache_.size() - kMaximumCacheSize,
                                         &ids_to_delete);
  }

  if (!ids_to_delete.empty()) {
    table_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AutocompleteActionPredictorTable::DeleteRows,
                                  table_, std::move(ids_to_delete)));
  }

  FinishInitialization();
  if (incognito_predictor_)
    incognito_predictor_->CopyFromMainProfile();
}

void AutocompleteActionPredictor::DeleteOldIdsFromCaches(
    history::URLDatabase* url_db,
    std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(url_db);
  DCHECK(id_list);

  for (auto it = db_cache_.begin(); it != db_cache_.end();) {
    history::URLRow url_row;

    if ((url_db->GetRowForURL(it->first.url, &url_row) == 0) ||
        ((base::Time::Now() - url_row.last_visit()).InDays() >
         kMaximumDaysToKeepEntry)) {
      const DBIdCacheMap::iterator id_it = db_id_cache_.find(it->first);
      DCHECK(id_it != db_id_cache_.end());
      id_list->push_back(id_it->second);
      db_id_cache_.erase(id_it);
      db_cache_.erase(it++);
    } else {
      ++it;
    }
  }
}

void AutocompleteActionPredictor::DeleteLowestConfidenceRowsFromCaches(
    size_t count,
    std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(id_list);

  auto compare_confidence = [](const DBCacheMap::iterator& lhs,
                               const DBCacheMap::iterator& rhs) {
    const DBCacheValue& lhs_value = lhs->second;
    const DBCacheValue& rhs_value = rhs->second;
    // Compare by confidence scores first. In case of equality, compare by
    // number of hits.
    int lhs_confidence_to_compare =
        lhs_value.number_of_hits *
        (rhs_value.number_of_hits + rhs_value.number_of_misses);
    int rhs_confidence_to_compare =
        rhs_value.number_of_hits *
        (lhs_value.number_of_hits + lhs_value.number_of_misses);
    return std::tie(lhs_confidence_to_compare, lhs_value.number_of_hits) <
           std::tie(rhs_confidence_to_compare, rhs_value.number_of_hits);
  };
  // Use max heap to find |count| smallest elements in |db_cache_|.
  std::priority_queue<DBCacheMap::iterator, std::vector<DBCacheMap::iterator>,
                      decltype(compare_confidence)>
      max_heap(compare_confidence);

  for (auto it = db_cache_.begin(); it != db_cache_.end(); ++it) {
    max_heap.push(it);
    if (max_heap.size() > count)
      max_heap.pop();
  }

  // Only iterators to the erased elements are invalidated, so it's safe to keep
  // using remaining iterators.
  while (!max_heap.empty()) {
    auto entry_to_delete = max_heap.top();
    auto id_it = db_id_cache_.find(entry_to_delete->first);
    DCHECK(id_it != db_id_cache_.end());

    id_list->push_back(id_it->second);
    db_id_cache_.erase(id_it);
    db_cache_.erase(entry_to_delete);
    max_heap.pop();
  }
}

void AutocompleteActionPredictor::CopyFromMainProfile() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(profile_->IsOffTheRecord());
  DCHECK(!initialized_);
  DCHECK(main_profile_predictor_);
  DCHECK(main_profile_predictor_->initialized_);

  db_cache_ = main_profile_predictor_->db_cache_;
  db_id_cache_ = main_profile_predictor_->db_id_cache_;
  FinishInitialization();
}

void AutocompleteActionPredictor::FinishInitialization() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!initialized_);
  initialized_ = true;
}

double AutocompleteActionPredictor::CalculateConfidence(
    const std::u16string& user_text,
    const AutocompleteMatch& match,
    bool* is_in_db) const {
  const DBCacheKey key = { user_text, match.destination_url };

  *is_in_db = false;
  if (user_text.length() < kMinimumUserTextLength)
    return 0.0;

  const DBCacheMap::const_iterator iter = db_cache_.find(key);
  if (iter == db_cache_.end())
    return 0.0;

  *is_in_db = true;
  return CalculateConfidenceForDbEntry(iter);
}

double AutocompleteActionPredictor::CalculateConfidenceForDbEntry(
    DBCacheMap::const_iterator iter) const {
  const DBCacheValue& value = iter->second;
  if (value.number_of_hits < kMinimumNumberOfHits)
    return 0.0;

  const double number_of_hits = static_cast<double>(value.number_of_hits);
  return number_of_hits / (number_of_hits + value.number_of_misses);
}

void AutocompleteActionPredictor::Shutdown() {
  history_service_observation_.Reset();
}

void AutocompleteActionPredictor::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK(initialized_);

  if (deletion_info.IsAllHistory()) {
    DeleteAllRows();
    return;
  }

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  DeleteRowsFromCaches(deletion_info.deleted_rows(), &id_list);

  if (!deletion_info.is_from_expiration() && history_service) {
    auto* url_db = history_service->InMemoryDatabase();
    if (url_db)
      DeleteOldIdsFromCaches(url_db, &id_list);
  }

  if (table_.get()) {
    table_->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&AutocompleteActionPredictorTable::DeleteRows,
                                  table_, std::move(id_list)));
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                            DatabaseAction::kDeleteSome);
}

void AutocompleteActionPredictor::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  if (!initialized_)
    TryDeleteOldEntries(history_service);
}

AutocompleteActionPredictor::TransitionalMatch::TransitionalMatch() {
}

AutocompleteActionPredictor::TransitionalMatch::TransitionalMatch(
    const std::u16string in_user_text)
    : user_text(in_user_text) {}

AutocompleteActionPredictor::TransitionalMatch::TransitionalMatch(
    const TransitionalMatch& other) = default;

AutocompleteActionPredictor::TransitionalMatch::~TransitionalMatch() {
}

}  // namespace predictors
