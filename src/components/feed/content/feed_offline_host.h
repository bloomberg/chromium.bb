// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CONTENT_FEED_OFFLINE_HOST_H_
#define COMPONENTS_FEED_CONTENT_FEED_OFFLINE_HOST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/prefetch/suggestions_provider.h"

class GURL;

namespace offline_pages {
class PrefetchService;
}  // namespace offline_pages

namespace feed {

class FeedSchedulerHost;

// Responsible for wiring up connections for Feed operations pertaining to
// articles that can be loaded from Offline Pages component. Most significantly
// this class connects Prefetch and the Feed, and tracks offlined articles the
// Feed may have badged for this user. This knowledge is later used when Feed
// articles are opened to populate load params.
class FeedOfflineHost : public offline_pages::SuggestionsProvider,
                        public offline_pages::OfflinePageModel::Observer {
 public:
  FeedOfflineHost(offline_pages::OfflinePageModel* offline_page_model,
                  offline_pages::PrefetchService* prefetch_service,
                  FeedSchedulerHost* feed_scheduler_host);
  ~FeedOfflineHost() override;

  // offline_pages::SuggestionsProvider:
  void GetCurrentArticleSuggestions(
      offline_pages::SuggestionsProvider::SuggestionCallback
          suggestions_callback) override;
  void ReportArticleListViewed() override;
  void ReportArticleViewed(GURL article_url) override;

  // offline_pages::OfflinePageModel::Observer:
  void OfflinePageModelLoaded(offline_pages::OfflinePageModel* model) override;
  void OfflinePageAdded(
      offline_pages::OfflinePageModel* model,
      const offline_pages::OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const offline_pages::OfflinePageModel::DeletedPageInfo& page_info)
      override;

 private:
  // The following objects all outlive us, so it is safe to hold raw pointers to
  // them. This is guaranteed by the FeedHostServiceFactory.
  offline_pages::OfflinePageModel* offline_page_model_;
  offline_pages::PrefetchService* prefetch_service_;
  FeedSchedulerHost* feed_scheduler_host_;

  base::WeakPtrFactory<FeedOfflineHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedOfflineHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CONTENT_FEED_OFFLINE_HOST_H_
