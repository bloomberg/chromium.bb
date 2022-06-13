// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"

#include <functional>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#else  // defined(OS_ANDROID)
#include "base/containers/contains.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/ntp_tiles/custom_links_store.h"
#endif  // defined(OS_ANDROID)

namespace {

using UrlAndVisitCallback =
    base::OnceCallback<void(history::URLRow, history::VisitVector)>;

// Gets the 2 most recent visits to a URL. Used to associate a memories visit
// with its history rows and compute the `duration_since_last_visit`
// context annotation.
class GetMostRecentVisitsToUrl : public history::HistoryDBTask {
 public:
  GetMostRecentVisitsToUrl(const GURL& url, UrlAndVisitCallback callback)
      : url_(url), callback_(std::move(callback)) {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    if (backend->GetURL(url_, &url_row_))
      backend->GetMostRecentVisitsForURL(url_row_.id(), 2, &visits_);
    return true;
  }

  void DoneRunOnMainThread() override {
    if (!visits_.empty()) {
      DCHECK_LE(visits_.size(), 2u);
      std::move(callback_).Run(url_row_, visits_);
    }
  }

 private:
  const GURL url_;
  history::URLRow url_row_;
  history::VisitVector visits_;
  UrlAndVisitCallback callback_;
};

bool IsPageInTabGroup(content::WebContents* contents) {
  DCHECK(contents);

#if !defined(OS_ANDROID)
  if (Browser* browser = chrome::FindBrowserWithWebContents(contents)) {
    int tab_index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
    if (tab_index != TabStripModel::kNoTab &&
        browser->tab_strip_model()->GetTabGroupForTab(tab_index).has_value()) {
      return true;
    }
  }
  return false;
#else   // defined(OS_ANDROID)
  TabAndroid* const tab = TabAndroid::FromWebContents(contents);
  if (!tab)
    return false;
  return TabModelJniBridge::HasOtherRelatedTabs(tab);
#endif  // defined(OS_ANDROID)
}

// Pass in a separate `url` parameter to ensure that we check the same URL that
// is being logged in History.
bool IsPageBookmarked(content::WebContents* contents, const GURL& url) {
  DCHECK(contents);
  DCHECK(contents->GetBrowserContext());

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(contents->GetBrowserContext());
  return model && model->IsBookmarked(url);
}

base::TimeDelta TimeElapsedBetweenVisits(const history::VisitRow& visit1,
                                         const history::VisitRow& visit2) {
  base::TimeDelta delta = visit2.visit_time - visit1.visit_time;
  // Clamp to 30 days maximum to match the UKM retention period.
  const base::TimeDelta kMaxDurationClamp = base::Days(30);
  return delta < kMaxDurationClamp ? delta : kMaxDurationClamp;
}

// Returns with the provided `url` matches the provided `history_url`
// which must be either the basic history URL or history_clusters URL.
bool IsHistoryPage(GURL url, GURL history_url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements) == history_url;
}

}  // namespace

HistoryClustersTabHelper::HistoryClustersTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryClustersTabHelper>(*web_contents) {}

HistoryClustersTabHelper::~HistoryClustersTabHelper() = default;

void HistoryClustersTabHelper::OnOmniboxUrlCopied() {
  // It's possible that there have been no navigations if certain builtin pages
  // were opened in a new tab (e.g. chrome://crash or chrome://invalid-page).
  if (navigation_ids_.empty())
    return;

  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  // It's possible that the last navigation is complete if the tab crashed and a
  // new navigation hasn't began.
  if (!history_clusters_service->HasIncompleteVisitContextAnnotations(
          navigation_ids_.back())) {
    return;
  }

  history_clusters_service
      ->GetIncompleteVisitContextAnnotations(navigation_ids_.back())
      .context_annotations.omnibox_url_copied = true;
}

void HistoryClustersTabHelper::OnOmniboxUrlShared() {
  // TODO(crbug.com/1171352): possibly update a different context annotation.
  OnOmniboxUrlCopied();
}

void HistoryClustersTabHelper::OnUpdatedHistoryForNavigation(
    int64_t navigation_id,
    const GURL& url) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  StartNewNavigationIfNeeded(navigation_id);

  auto& incomplete_visit_context_annotations =
      history_clusters_service->GetOrCreateIncompleteVisitContextAnnotations(
          navigation_id);
  incomplete_visit_context_annotations.context_annotations
      .is_existing_part_of_tab_group = IsPageInTabGroup(web_contents());
  incomplete_visit_context_annotations.context_annotations
      .is_existing_bookmark = IsPageBookmarked(web_contents(), url);

  if (auto* history_service = GetHistoryService()) {
    // This `GetMostRecentVisitsToUrl` task will find at least 1 visit since
    // `HistoryTabHelper::UpdateHistoryForNavigation()`, invoked prior to
    // `OnUpdatedHistoryForNavigation()`, will have posted a task to add the
    // visit associated to `incomplete_visit_context_annotations`.
    history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetMostRecentVisitsToUrl>(
            url,
            base::BindOnce(
                [](HistoryClustersTabHelper* history_clusters_tab_helper,
                   history_clusters::HistoryClustersService*
                       history_clusters_service,
                   int64_t navigation_id,
                   history_clusters::IncompleteVisitContextAnnotations&
                       incomplete_visit_context_annotations,
                   history::URLRow url_row, history::VisitVector visits) {
                  DCHECK(history_clusters_tab_helper);
                  DCHECK(history_clusters_service);
                  DCHECK(url_row.id());
                  DCHECK(visits[0].visit_id);
                  DCHECK_EQ(url_row.id(), visits[0].url_id);
                  incomplete_visit_context_annotations.url_row = url_row;
                  incomplete_visit_context_annotations.visit_row = visits[0];
                  if (visits.size() > 1) {
                    incomplete_visit_context_annotations.context_annotations
                        .duration_since_last_visit =
                        TimeElapsedBetweenVisits(visits[1], visits[0]);
                  }
                  // If the navigation has already ended, record the page end
                  // metrics.
                  incomplete_visit_context_annotations.status.history_rows =
                      true;
                  if (incomplete_visit_context_annotations.status
                          .navigation_ended) {
                    DCHECK(!incomplete_visit_context_annotations.status
                                .navigation_end_signals);
                    history_clusters_tab_helper->RecordPageEndMetricsIfNeeded(
                        navigation_id);
                  }
                },
                this, history_clusters_service, navigation_id,
                std::ref(incomplete_visit_context_annotations))),
        &task_tracker_);
  }
}

void HistoryClustersTabHelper::TagNavigationAsExpectingUkmNavigationComplete(
    int64_t navigation_id) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  history_clusters_service
      ->GetOrCreateIncompleteVisitContextAnnotations(navigation_id)
      .status.expect_ukm_page_end_signals = true;
  StartNewNavigationIfNeeded(navigation_id);
}

history::VisitContextAnnotations
HistoryClustersTabHelper::OnUkmNavigationComplete(
    int64_t navigation_id,
    base::TimeDelta total_foreground_duration,
    const page_load_metrics::PageEndReason page_end_reason) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return history::VisitContextAnnotations();

  auto& incomplete_visit_context_annotations =
      history_clusters_service->GetIncompleteVisitContextAnnotations(
          navigation_id);
  incomplete_visit_context_annotations.context_annotations.page_end_reason =
      page_end_reason;
  incomplete_visit_context_annotations.context_annotations
      .total_foreground_duration = total_foreground_duration;
  // `RecordPageEndMetricsIfNeeded()` will fail to complete the
  // `IncompleteVisitContextAnnotations` as `ukm_page_end_signals` hasn't been
  // set yet, but it will record metrics if needed (i.e. not already recorded)
  // and possible (i.e. the history request has resolved and `history_rows` have
  // been recorded).
  RecordPageEndMetricsIfNeeded(navigation_id);
  // Make a copy of the context annotations as the referenced
  // incomplete_visit_context_annotations may be destroyed in
  // `CompleteVisitContextAnnotationsIfReady()`.
  auto context_annotations_copy =
      incomplete_visit_context_annotations.context_annotations;
  DCHECK(
      incomplete_visit_context_annotations.status.expect_ukm_page_end_signals);
  incomplete_visit_context_annotations.status.ukm_page_end_signals = true;
  history_clusters_service->CompleteVisitContextAnnotationsIfReady(
      navigation_id);
  return context_annotations_copy;
}

void HistoryClustersTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Will detect:
  // 1) When the history clusters page was toggled to the basic history page.
  // 2) A link was followed in the same web contents from the history clusters
  //    page.
  // And will update this page's associated `HistoryClustersMetricsLogger`.

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // We only care if the previously committed navigation was on the
  // HistoryClusters UI.
  if (!IsHistoryPage(navigation_handle->GetWebContents()->GetLastCommittedURL(),
                     GURL(chrome::kChromeUIHistoryClustersURL))) {
    return;
  }

  if (navigation_handle->IsSameDocument()) {
    if (IsHistoryPage(navigation_handle->GetURL(),
                      GURL(chrome::kChromeUIHistoryURL))) {
      history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
          navigation_handle->GetWebContents()->GetPrimaryPage())
          ->increment_toggles_to_basic_history();
    }
    return;
  }

  if (!IsHistoryPage(navigation_handle->GetURL(),
                     GURL(chrome::kChromeUIHistoryClustersURL)) &&
      PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_LINK)) {
    // If the previously committed navigation was on the history clusters page,
    // the current navigation is not on the history clusters UI and the
    // transition type is a link click, then we know the user clicked on a
    // result on the clusters page.
    auto* logger =
        history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
            navigation_handle->GetWebContents()->GetPrimaryPage());
    logger->set_final_state(
        history_clusters::HistoryClustersFinalState::kLinkClick);
    if (CanAddURLToHistory(navigation_handle->GetURL()))
      logger->IncrementLinksOpenedCount();
  }
}

void HistoryClustersTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Will detect when the history clusters page was navigated to, either
  // directly (e.g., through the omnibox, by page refresh, or by page
  // back/forward), or indirectly (e.g. through the side bar on the history
  // page). And will update this page's associated
  // `HistoryClustersMetricsLogger`.

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (!IsHistoryPage(navigation_handle->GetURL(),
                     GURL(chrome::kChromeUIHistoryClustersURL))) {
    return;
  }

  auto* logger =
      history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
          navigation_handle->GetWebContents()->GetPrimaryPage());
  logger->set_navigation_id(navigation_handle->GetNavigationId());

  // If the transition type is typed (meaning directly entered into the
  // address bar), PAGE_TRANSITION_TYPED, or is partially typed and selected
  // from the omnibox history, which results in PAGE_TRANSITION_RELOADS, this
  // usage of the history clusters UI is considered a "direct" navigation.
  auto initial_state =
      PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_TYPED) ||
              PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                       ui::PAGE_TRANSITION_RELOAD)
          ? history_clusters::HistoryClustersInitialState::kDirectNavigation
          : history_clusters::HistoryClustersInitialState::kIndirectNavigation;
  logger->set_initial_state(initial_state);
}

void HistoryClustersTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  // Will detect when a link was followed from the history clusters page in a
  // new web contents (e.g. new tab or window).
  // And will update this page's associated `HistoryClustersMetricsLogger`.
  if (IsHistoryPage(web_contents()->GetLastCommittedURL(),
                    GURL(chrome::kChromeUIHistoryClustersURL)) &&
      CanAddURLToHistory(url)) {
    auto* logger =
        history_clusters::HistoryClustersMetricsLogger::GetOrCreateForPage(
            web_contents()->GetPrimaryPage());
    logger->IncrementLinksOpenedCount();
  }
}

void HistoryClustersTabHelper::WebContentsDestroyed() {
  // Complete any incomplete visits associated with navigations made in this
  // tab.
  for (auto navigation_id : navigation_ids_)
    RecordPageEndMetricsIfNeeded(navigation_id);
}

void HistoryClustersTabHelper::StartNewNavigationIfNeeded(
    int64_t navigation_id) {
  if (!navigation_ids_.empty() && navigation_id == navigation_ids_.back())
    return;

  // We are starting a new navigation, so record the navigation end metrics for
  // the previous navigation.
  // TODO(tommycli): We don't know the page end reason, but maybe we could guess
  //  and assume it's a same document navigation. Investigate this.
  if (!navigation_ids_.empty())
    RecordPageEndMetricsIfNeeded(navigation_ids_.back());
  navigation_ids_.push_back(navigation_id);
}

void HistoryClustersTabHelper::RecordPageEndMetricsIfNeeded(
    int64_t navigation_id) {
  auto* history_clusters_service = GetHistoryClustersService();
  if (!history_clusters_service)
    return;

  if (!history_clusters_service->HasIncompleteVisitContextAnnotations(
          navigation_id))
    return;
  auto& incomplete_visit_context_annotations =
      history_clusters_service->GetIncompleteVisitContextAnnotations(
          navigation_id);
  if (incomplete_visit_context_annotations.status.navigation_end_signals) {
    DCHECK(incomplete_visit_context_annotations.status.navigation_ended);
    return;
  }
  incomplete_visit_context_annotations.status.navigation_ended = true;
  // Don't record page end metrics if the history rows request hasn't resolved
  // because some of the metrics rely on |url_row.url()|. Setting
  // `navigation_ended` above will ensure `RecordPageEndMetricsIfNeeded()` is
  // re-invoked once the history request resolves.
  if (!incomplete_visit_context_annotations.status.history_rows)
    return;

  incomplete_visit_context_annotations.context_annotations
      .is_placed_in_tab_group =
      !incomplete_visit_context_annotations.context_annotations
           .is_existing_part_of_tab_group &&
      IsPageInTabGroup(web_contents());
  incomplete_visit_context_annotations.context_annotations.is_new_bookmark =
      !incomplete_visit_context_annotations.context_annotations
           .is_existing_bookmark &&
      IsPageBookmarked(web_contents(),
                       incomplete_visit_context_annotations.url_row.url());
  // Android does not have NTP Custom Links.
#if !defined(OS_ANDROID)
  // This queries the prefs directly if the visit URL is stored as an NTP
  // custom link, bypassing the CustomLinksManager.
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  ntp_tiles::CustomLinksStore custom_link_store(pref_service);
  incomplete_visit_context_annotations.context_annotations.is_ntp_custom_link =
      base::Contains(custom_link_store.RetrieveLinks(),
                     incomplete_visit_context_annotations.url_row.url(),
                     [](const auto& link) { return link.url; });
#endif  // !defined(OS_ANDROID)

  incomplete_visit_context_annotations.status.navigation_end_signals = true;
  history_clusters_service->CompleteVisitContextAnnotationsIfReady(
      navigation_id);
}

history_clusters::HistoryClustersService*
HistoryClustersTabHelper::GetHistoryClustersService() {
  if (!web_contents()) {
    NOTREACHED();
    return nullptr;
  }
  return HistoryClustersServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

history::HistoryService* HistoryClustersTabHelper::GetHistoryService() {
  if (!web_contents()) {
    NOTREACHED();
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return HistoryServiceFactory::GetForProfileIfExists(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryClustersTabHelper);
