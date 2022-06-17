// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/profile_based_browsing_history_driver.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/query_clusters_state.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace history_clusters {

class QueryClustersState;

// Not in an anonymous namespace so that it can be tested.
// TODO(manukh) Try to setup a complete `HistoryClusterHandler` for testing so
//  that we can test the public method `QueryClusters` directly instead.
mojom::QueryResultPtr QueryClustersResultToMojom(
    Profile* profile,
    const std::string& query,
    const std::vector<history::Cluster> clusters_batch,
    bool can_load_more,
    bool is_continuation);

// Handles bidirectional communication between the history clusters page and the
// browser.
class HistoryClustersHandler : public mojom::PageHandler,
                               public HistoryClustersService::Observer,
                               public ProfileBasedBrowsingHistoryDriver {
 public:
  HistoryClustersHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  HistoryClustersHandler(const HistoryClustersHandler&) = delete;
  HistoryClustersHandler& operator=(const HistoryClustersHandler&) = delete;
  ~HistoryClustersHandler() override;

  void SetSidePanelUIEmbedder(
      base::WeakPtr<ui::MojoBubbleWebUIController::Embedder>
          side_panel_embedder);

  // mojom::PageHandler:
  void SetPage(mojo::PendingRemote<mojom::Page> pending_page) override;
  void ShowSidePanelUI() override;
  void ToggleVisibility(bool visible,
                        ToggleVisibilityCallback callback) override;
  void StartQueryClusters(const std::string& query) override;
  void LoadMoreClusters(const std::string& query) override;
  void RemoveVisits(std::vector<mojom::URLVisitPtr> visits,
                    RemoveVisitsCallback callback) override;
  void OpenVisitUrlsInTabGroup(std::vector<mojom::URLVisitPtr> visits) override;
  void RecordVisitAction(mojom::VisitAction visit_action,
                         uint32_t visit_index,
                         mojom::VisitType visit_type) override;
  void RecordRelatedSearchAction(mojom::RelatedSearchAction action,
                                 uint32_t related_search_index) override;
  void RecordClusterAction(mojom::ClusterAction cluster_action,
                           uint32_t cluster_index) override;
  void RecordToggledVisibility(bool visible) override;

  // HistoryClustersService::Observer:
  void OnDebugMessage(const std::string& message) override;

  // ProfileBasedBrowsingHistoryDriver:
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void HistoryDeleted() override;
  Profile* GetProfile() override;

 private:
  // Called with the result of querying clusters. Subsequently, `query_result`
  // is sent to the JS to update the UI.
  void OnClustersQueryResult(mojom::QueryResultPtr query_result);

  base::WeakPtr<ui::MojoBubbleWebUIController::Embedder>
      history_clusters_side_panel_embedder_;

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  // Used to observe the service.
  base::ScopedObservation<HistoryClustersService,
                          HistoryClustersService::Observer>
      service_observation_{this};

  mojo::Remote<mojom::Page> page_;
  mojo::Receiver<mojom::PageHandler> page_handler_;

  // Encapsulates the currently loaded clusters state on the page.
  std::unique_ptr<QueryClustersState> query_clusters_state_;

  // Used only for deleting History properly, and observing deletions that occur
  // from other tabs. It's not used for querying History, because we do our
  // querying with HistoryClustersService.
  std::unique_ptr<history::BrowsingHistoryService> browsing_history_service_;

  // The following variables hold the visits requested to be deleted and the
  // callback for the respective request. `BrowsingHistoryService` can only
  // handle one deletion request at a time.
  std::vector<mojom::URLVisitPtr> pending_remove_visits_;
  RemoveVisitsCallback pending_remove_visits_callback_;

  base::WeakPtrFactory<HistoryClustersHandler> weak_ptr_factory_{this};
};

}  // namespace history_clusters

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HANDLER_H_
