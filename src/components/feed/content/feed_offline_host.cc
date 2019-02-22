// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/content/feed_offline_host.h"

#include <utility>

#include "base/bind.h"
#include "base/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "url/gurl.h"

namespace feed {

using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModel;
using offline_pages::PrefetchService;
using offline_pages::PrefetchSuggestion;
using offline_pages::SuggestionsProvider;

namespace {

// |url| is always set. Sometimes |original_url| is set. If |original_url| is
// set it is returned by this method, otherwise fall back to |url|.
const GURL& PreferOriginal(const OfflinePageItem& item) {
  return item.original_url.is_empty() ? item.url : item.original_url;
}

// Aggregates multiple callbacks from OfflinePageModel, storing the offline url.
// When all callbacks have been invoked, tracked by ref counting, then
// |on_completeion_| is finally invoked, sending all results together.
class CallbackAggregator : public base::RefCounted<CallbackAggregator> {
 public:
  using ReportStatusCallback =
      base::OnceCallback<void(std::vector<std::string>)>;
  using CacheIdCallback =
      base::RepeatingCallback<void(const std::string&, int64_t)>;

  CallbackAggregator(ReportStatusCallback on_completion,
                     CacheIdCallback on_each_result)
      : on_completion_(std::move(on_completion)),
        on_each_result_(std::move(on_each_result)),
        start_time_(base::Time::Now()) {}

  void OnGetPages(const std::vector<OfflinePageItem>& pages) {
    if (!pages.empty()) {
      OfflinePageItem newest =
          *std::max_element(pages.begin(), pages.end(), [](auto lhs, auto rhs) {
            return lhs.creation_time < rhs.creation_time;
          });
      const std::string& url = PreferOriginal(newest).spec();
      urls_.push_back(url);
      on_each_result_.Run(url, newest.offline_id);
    }
  }

 private:
  friend class base::RefCounted<CallbackAggregator>;

  ~CallbackAggregator() {
    base::TimeDelta duration = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_TIMES("ContentSuggestions.Feed.Offline.GetStatusDuration",
                        duration);
    std::move(on_completion_).Run(std::move(urls_));
  }

  // To be called once all callbacks are run or destroyed.
  ReportStatusCallback on_completion_;

  // The urls of the offlined pages seen so far. Ultimately will be given to
  // |on_completeion_|.
  std::vector<std::string> urls_;

  // Is not run if there are no results for a given url.
  CacheIdCallback on_each_result_;

  // The time the aggregator was created, before any requests were sent to the
  // OfflinePageModel.
  base::Time start_time_;
};

// Consumes |metadataVector|, moving as many of the fields as possible.
std::vector<PrefetchSuggestion> ConvertMetadataToSuggestions(
    std::vector<ContentMetadata> metadataVector) {
  std::vector<PrefetchSuggestion> suggestionsVector;
  for (ContentMetadata& metadata : metadataVector) {
    // TODO(skym): Copy over published time when PrefetchSuggestion adds
    // support.
    PrefetchSuggestion suggestion;
    suggestion.article_url = GURL(metadata.url);
    suggestion.article_title = std::move(metadata.title);
    suggestion.article_attribution = std::move(metadata.publisher);
    suggestion.article_snippet = std::move(metadata.snippet);
    suggestion.thumbnail_url = GURL(metadata.image_url);
    suggestion.favicon_url = GURL(metadata.favicon_url);
    suggestionsVector.push_back(std::move(suggestion));
  }
  return suggestionsVector;
}

void RunSuggestionCallbackWithConversion(
    SuggestionsProvider::SuggestionCallback suggestions_callback,
    std::vector<ContentMetadata> metadataVector) {
  std::move(suggestions_callback)
      .Run(ConvertMetadataToSuggestions(std::move(metadataVector)));
}

}  //  namespace

FeedOfflineHost::FeedOfflineHost(OfflinePageModel* offline_page_model,
                                 PrefetchService* prefetch_service,
                                 base::RepeatingClosure on_suggestion_consumed,
                                 base::RepeatingClosure on_suggestions_shown)
    : offline_page_model_(offline_page_model),
      prefetch_service_(prefetch_service),
      on_suggestion_consumed_(on_suggestion_consumed),
      on_suggestions_shown_(on_suggestions_shown),
      weak_factory_(this) {
  DCHECK(offline_page_model_);
  DCHECK(prefetch_service_);
  DCHECK(!on_suggestion_consumed_.is_null());
  DCHECK(!on_suggestions_shown_.is_null());
}

FeedOfflineHost::~FeedOfflineHost() {
  // Safe to call RemoveObserver() even if AddObserver() has not been called.
  offline_page_model_->RemoveObserver(this);
}

void FeedOfflineHost::Initialize(
    const base::RepeatingClosure& trigger_get_known_content,
    const NotifyStatusChangeCallback& notify_status_change) {
  DCHECK(trigger_get_known_content_.is_null());
  DCHECK(!trigger_get_known_content.is_null());
  DCHECK(notify_status_change_.is_null());
  DCHECK(!notify_status_change.is_null());
  trigger_get_known_content_ = trigger_get_known_content;
  notify_status_change_ = notify_status_change;
  offline_page_model_->AddObserver(this);
  // The host guarantees that the two callbacks passed into this method will not
  // be invoked until Initialize() has exited. To guarantee this, the host
  // cannot call SetSuggestionProvider() in task, because that would give
  // Prefetch the ability to run |trigger_get_known_content_| immediately.
  // PostTask is used to delay when SetSuggestionProvider() is called.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FeedOfflineHost::SetSuggestionProvider,
                                weak_factory_.GetWeakPtr()));
}

void FeedOfflineHost::SetSuggestionProvider() {
  prefetch_service_->SetSuggestionProvider(this);
}

base::Optional<int64_t> FeedOfflineHost::GetOfflineId(const std::string& url) {
  auto iter = url_hash_to_id_.find(base::Hash(url));
  return iter == url_hash_to_id_.end() ? base::Optional<int64_t>()
                                       : base::Optional<int64_t>(iter->second);
}

void FeedOfflineHost::GetOfflineStatus(
    std::vector<std::string> urls,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  UMA_HISTOGRAM_EXACT_LINEAR("ContentSuggestions.Feed.Offline.GetStatusCount",
                             urls.size(), 50);

  scoped_refptr<CallbackAggregator> aggregator =
      base::MakeRefCounted<CallbackAggregator>(
          std::move(callback),
          base::BindRepeating(&FeedOfflineHost::CacheOfflinePageUrlAndId,
                              weak_factory_.GetWeakPtr()));

  for (std::string url : urls) {
    offline_page_model_->GetPagesByURL(
        GURL(url), base::BindOnce(&CallbackAggregator::OnGetPages, aggregator));
  }
}

void FeedOfflineHost::OnContentRemoved(std::vector<std::string> urls) {
  for (const std::string& url : urls) {
    prefetch_service_->RemoveSuggestion(GURL(url));
  }
}

void FeedOfflineHost::OnNewContentReceived() {
  prefetch_service_->NewSuggestionsAvailable();
}

void FeedOfflineHost::OnNoListeners() {
  url_hash_to_id_.clear();
}

void FeedOfflineHost::OnGetKnownContentDone(
    std::vector<ContentMetadata> suggestions) {
  // While |suggestions| are movable, there might be multiple callbacks in
  // |pending_known_content_callbacks_|. All but one callback will need to
  // receive a full copy of |suggestions|, and the last one will received a
  // moved copy.
  for (size_t i = 0; i < pending_known_content_callbacks_.size(); i++) {
    bool can_move = (i + 1 == pending_known_content_callbacks_.size());
    std::move(pending_known_content_callbacks_[i])
        .Run(can_move ? std::move(suggestions) : suggestions);
  }
  pending_known_content_callbacks_.clear();
}

void FeedOfflineHost::GetCurrentArticleSuggestions(
    SuggestionsProvider::SuggestionCallback suggestions_callback) {
  DCHECK(!trigger_get_known_content_.is_null());
  pending_known_content_callbacks_.push_back(base::BindOnce(
      &RunSuggestionCallbackWithConversion, std::move(suggestions_callback)));
  // Trigger after push_back() in case triggering results in a synchronous
  // response via OnGetKnownContentDone().
  if (pending_known_content_callbacks_.size() <= 1) {
    trigger_get_known_content_.Run();
  }
}

void FeedOfflineHost::ReportArticleListViewed() {
  on_suggestion_consumed_.Run();
}

void FeedOfflineHost::ReportArticleViewed(GURL article_url) {
  on_suggestions_shown_.Run();
}

void FeedOfflineHost::OfflinePageModelLoaded(OfflinePageModel* model) {
  // Ignored.
}

void FeedOfflineHost::OfflinePageAdded(OfflinePageModel* model,
                                       const OfflinePageItem& added_page) {
  DCHECK(!notify_status_change_.is_null());
  const std::string& url = PreferOriginal(added_page).spec();
  CacheOfflinePageUrlAndId(url, added_page.offline_id);
  notify_status_change_.Run(url, true);
}

void FeedOfflineHost::OfflinePageDeleted(
    const OfflinePageModel::DeletedPageInfo& page_info) {
  DCHECK(!notify_status_change_.is_null());
  const std::string& url = page_info.url.spec();
  EvictOfflinePageUrl(url);
  notify_status_change_.Run(url, false);
}

void FeedOfflineHost::CacheOfflinePageUrlAndId(const std::string& url,
                                               int64_t id) {
  url_hash_to_id_[base::Hash(url)] = id;
}

void FeedOfflineHost::EvictOfflinePageUrl(const std::string& url) {
  url_hash_to_id_.erase(base::Hash(url));
}

}  // namespace feed
