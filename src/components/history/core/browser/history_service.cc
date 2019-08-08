// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The history system runs on a background thread so that potentially slow
// database operations don't delay the browser. This backend processing is
// represented by HistoryBackend. The HistoryService's job is to dispatch to
// that thread.
//
// Main thread                       History thread
// -----------                       --------------
// HistoryService <----------------> HistoryBackend
//                                   -> HistoryDatabase
//                                      -> SQLite connection to History
//                                   -> ThumbnailDatabase
//                                      -> SQLite connection to Thumbnails
//                                         (and favicons)

#include "components/history/core/browser/history_service.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/sync/delete_directive_handler.h"
#include "components/history/core/browser/visit_database.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/history/core/common/thumbnail_score.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model_impl/proxy_model_type_controller_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_IOS)
#include "base/critical_closure.h"
#endif

using base::Time;

namespace history {
namespace {

const base::Feature kHistoryServiceUsesTaskScheduler{
    "HistoryServiceUsesTaskScheduler", base::FEATURE_DISABLED_BY_DEFAULT};

static const char* kHistoryThreadName = "Chrome_HistoryThread";

void RunWithFaviconResults(
    const favicon_base::FaviconResultsCallback& callback,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  TRACE_EVENT0("browser", "RunWithFaviconResults");
  callback.Run(*bitmap_results);
}

void RunWithFaviconResult(
    const favicon_base::FaviconRawBitmapCallback& callback,
    favicon_base::FaviconRawBitmapResult* bitmap_result) {
  callback.Run(*bitmap_result);
}

void RunWithQueryURLResult(HistoryService::QueryURLCallback callback,
                           const QueryURLResult* result) {
  std::move(callback).Run(result->success, result->row, result->visits);
}

void RunWithVisibleVisitCountToHostResult(
    const HistoryService::GetVisibleVisitCountToHostCallback& callback,
    const VisibleVisitCountToHostResult* result) {
  callback.Run(result->success, result->count, result->first_visit);
}

// Callback from WebHistoryService::ExpireWebHistory().
void ExpireWebHistoryComplete(bool success) {
  // Ignore the result.
  //
  // TODO(davidben): ExpireLocalAndRemoteHistoryBetween callback should not fire
  // until this completes.
}

}  // namespace

// Sends messages from the backend to us on the main thread. This must be a
// separate class from the history service so that it can hold a reference to
// the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class HistoryService::BackendDelegate : public HistoryBackend::Delegate {
 public:
  BackendDelegate(
      const base::WeakPtr<HistoryService>& history_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : history_service_(history_service),
        service_task_runner_(service_task_runner) {}

  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {
    // Send to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyProfileError,
                                  history_service_, init_status, diagnostics));
  }

  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {
    // Send the backend to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::SetInMemoryBackend,
                                  history_service_, std::move(backend)));
  }

  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {
    // Send the notification to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyFaviconsChanged,
                                  history_service_, page_urls, icon_url));
  }

  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::NotifyURLVisited, history_service_,
                       transition, row, redirects, visit_time));
  }

  void NotifyURLsModified(const URLRows& changed_urls) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyURLsModified,
                                  history_service_, changed_urls));
  }

  void NotifyURLsDeleted(DeletionInfo deletion_info) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HistoryService::NotifyURLsDeleted,
                                  history_service_, std::move(deletion_info)));
  }

  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::NotifyKeywordSearchTermUpdated,
                       history_service_, row, keyword_id, term));
  }

  void NotifyKeywordSearchTermDeleted(URLID url_id) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::NotifyKeywordSearchTermDeleted,
                       history_service_, url_id));
  }

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HistoryService::OnDBLoaded, history_service_));
  }

 private:
  const base::WeakPtr<HistoryService> history_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

HistoryService::HistoryService() : HistoryService(nullptr, nullptr) {}

HistoryService::HistoryService(std::unique_ptr<HistoryClient> history_client,
                               std::unique_ptr<VisitDelegate> visit_delegate)
    : thread_(base::FeatureList::IsEnabled(kHistoryServiceUsesTaskScheduler)
                  ? nullptr
                  : new base::Thread(kHistoryThreadName)),
      history_client_(std::move(history_client)),
      visit_delegate_(std::move(visit_delegate)),
      backend_loaded_(false),
      weak_ptr_factory_(this) {}

HistoryService::~HistoryService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

bool HistoryService::BackendLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return backend_loaded_;
}

#if defined(OS_IOS)
void HistoryService::HandleBackgrounding() {
  if (!backend_task_runner_ || !history_backend_.get())
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::MakeCriticalClosure(base::Bind(
                   &HistoryBackend::PersistState, history_backend_.get())));
}
#endif

void HistoryService::ClearCachedDataForContextID(ContextID context_id) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::ClearCachedDataForContextID,
                              history_backend_, context_id));
}

URLDatabase* HistoryService::InMemoryDatabase() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return in_memory_backend_ ? in_memory_backend_->db() : nullptr;
}

void HistoryService::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Cleanup();
}

void HistoryService::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const base::string16& term) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::SetKeywordSearchTermsForURL,
                              history_backend_, url, keyword_id, term));
}

void HistoryService::DeleteAllSearchTermsForKeyword(KeywordID keyword_id) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (in_memory_backend_)
    in_memory_backend_->DeleteAllSearchTermsForKeyword(keyword_id);

  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::DeleteAllSearchTermsForKeyword,
                              history_backend_, keyword_id));
}

void HistoryService::DeleteKeywordSearchTermForURL(const GURL& url) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::DeleteKeywordSearchTermForURL,
                              history_backend_, url));
}

void HistoryService::DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                                  const base::string16& term) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::DeleteMatchingURLsForKeyword,
                              history_backend_, keyword_id, term));
}

void HistoryService::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::URLsNoLongerBookmarked,
                              history_backend_, urls));
}

void HistoryService::AddObserver(HistoryServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void HistoryService::RemoveObserver(HistoryServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

base::CancelableTaskTracker::TaskId HistoryService::ScheduleDBTask(
    const base::Location& from_here,
    std::unique_ptr<HistoryDBTask> task,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId task_id =
      tracker->NewTrackedTaskId(&is_canceled);
  // Use base::ThreadTaskRunnerHandler::Get() to get a task runner for
  // the current message loop so that we can forward the call to the method
  // HistoryDBTask::DoneRunOnMainThread() in the correct thread.
  backend_task_runner_->PostTask(
      from_here,
      base::BindOnce(&HistoryBackend::ProcessDBTask, history_backend_,
                     std::move(task), base::ThreadTaskRunnerHandle::Get(),
                     is_canceled));
  return task_id;
}

void HistoryService::FlushForTest(const base::Closure& flushed) {
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(), flushed);
}

void HistoryService::SetOnBackendDestroyTask(const base::Closure& task) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::SetOnBackendDestroyTask, history_backend_,
                     base::ThreadTaskRunnerHandle::Get(), task));
}

void HistoryService::GetCountsAndLastVisitForOriginsForTesting(
    const std::set<GURL>& origins,
    const GetCountsAndLastVisitForOriginsCallback& callback) const {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetCountsAndLastVisitForOrigins,
                 history_backend_, origins),
      callback);
}

void HistoryService::AddPage(const GURL& url,
                             Time time,
                             ContextID context_id,
                             int nav_entry_id,
                             const GURL& referrer,
                             const RedirectList& redirects,
                             ui::PageTransition transition,
                             VisitSource visit_source,
                             bool did_replace_entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(HistoryAddPageArgs(url, time, context_id, nav_entry_id, referrer,
                             redirects, transition,
                             !ui::PageTransitionIsMainFrame(transition),
                             visit_source, did_replace_entry, true));
}

void HistoryService::AddPage(const GURL& url,
                             base::Time time,
                             VisitSource visit_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(HistoryAddPageArgs(url, time, nullptr, 0, GURL(), RedirectList(),
                             ui::PAGE_TRANSITION_LINK, false, visit_source,
                             false, true));
}

void HistoryService::AddPage(const HistoryAddPageArgs& add_page_args) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (history_client_ && !history_client_->CanAddURL(add_page_args.url))
    return;

  // Inform VisitedDelegate of all links and redirects.
  if (visit_delegate_) {
    if (!add_page_args.redirects.empty()) {
      // We should not be asked to add a page in the middle of a redirect chain,
      // and thus add_page_args.url should be the last element in the array
      // add_page_args.redirects which mean we can use VisitDelegate::AddURLs()
      // with the whole array.
      DCHECK_EQ(add_page_args.url, add_page_args.redirects.back());
      visit_delegate_->AddURLs(add_page_args.redirects);
    } else {
      visit_delegate_->AddURL(add_page_args.url);
    }
  }

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPage, history_backend_,
                              add_page_args));
}

void HistoryService::AddPageNoVisitForBookmark(const GURL& url,
                                               const base::string16& title) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(url))
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPageNoVisitForBookmark,
                              history_backend_, url, title));
}

void HistoryService::SetPageTitle(const GURL& url,
                                  const base::string16& title) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::SetPageTitle,
                                               history_backend_, url, title));
}

void HistoryService::UpdateWithPageEndTime(ContextID context_id,
                                           int nav_entry_id,
                                           const GURL& url,
                                           Time end_ts) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::UpdateWithPageEndTime, history_backend_,
                     context_id, nav_entry_id, url, end_ts));
}

void HistoryService::AddPageWithDetails(const GURL& url,
                                        const base::string16& title,
                                        int visit_count,
                                        int typed_count,
                                        Time last_visit,
                                        bool hidden,
                                        VisitSource visit_source) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // Filter out unwanted URLs.
  if (history_client_ && !history_client_->CanAddURL(url))
    return;

  // Inform VisitDelegate of the URL.
  if (visit_delegate_)
    visit_delegate_->AddURL(url);

  URLRow row(url);
  row.set_title(title);
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(last_visit);
  row.set_hidden(hidden);

  URLRows rows;
  rows.push_back(row);

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPagesWithDetails,
                              history_backend_, rows, visit_source));
}

void HistoryService::AddPagesWithDetails(const URLRows& info,
                                         VisitSource visit_source) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Inform the VisitDelegate of the URLs
  if (!info.empty() && visit_delegate_) {
    std::vector<GURL> urls;
    urls.reserve(info.size());
    for (const auto& row : info)
      urls.push_back(row.url());
    visit_delegate_->AddURLs(urls);
  }

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::AddPagesWithDetails,
                              history_backend_, info, visit_source));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::GetFavicons");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetFavicon, history_backend_, icon_url,
                     icon_type, desired_sizes, results),
      base::BindOnce(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconsForURL(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    const std::vector<int>& desired_sizes,
    bool fallback_to_host,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::GetFaviconsForURL");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetFaviconsForURL, history_backend_,
                     page_url, icon_types, desired_sizes, fallback_to_host,
                     results),
      base::BindOnce(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types,
    int minimum_size_in_pixels,
    const favicon_base::FaviconRawBitmapCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  favicon_base::FaviconRawBitmapResult* result =
      new favicon_base::FaviconRawBitmapResult();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetLargestFaviconForURL, history_backend_,
                     page_url, icon_types, minimum_size_in_pixels, result),
      base::BindOnce(&RunWithFaviconResult, callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconForID(
    favicon_base::FaviconID favicon_id,
    int desired_size,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::GetFaviconForID");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetFaviconForID, history_backend_,
                     favicon_id, desired_size, results),
      base::BindOnce(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId
HistoryService::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "HistoryService::UpdateFaviconMappingsAndFetch");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::UpdateFaviconMappingsAndFetch,
                     history_backend_, page_urls, icon_url, icon_type,
                     desired_sizes, results),
      base::BindOnce(&RunWithFaviconResults, callback, base::Owned(results)));
}

void HistoryService::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  TRACE_EVENT0("browser", "HistoryService::DeleteFaviconMappings");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::DeleteFaviconMappings,
                              history_backend_, page_urls, icon_type));
}

void HistoryService::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  TRACE_EVENT0("browser", "HistoryService::MergeFavicon");
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(page_url))
    return;

  ScheduleTask(
      PRIORITY_NORMAL,
      base::BindOnce(&HistoryBackend::MergeFavicon, history_backend_, page_url,
                     icon_url, icon_type, bitmap_data, pixel_size));
}

void HistoryService::SetFavicons(const base::flat_set<GURL>& page_urls,
                                 favicon_base::IconType icon_type,
                                 const GURL& icon_url,
                                 const std::vector<SkBitmap>& bitmaps) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  base::flat_set<GURL> page_urls_to_save;
  page_urls_to_save.reserve(page_urls.capacity());
  for (const GURL& page_url : page_urls) {
    if (!history_client_ || history_client_->CanAddURL(page_url))
      page_urls_to_save.insert(page_url);
  }

  if (page_urls_to_save.empty())
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetFavicons, history_backend_,
                              page_urls_to_save, icon_type, icon_url, bitmaps));
}

void HistoryService::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::CloneFaviconMappingsForPages,
                              history_backend_, page_url_to_read, icon_types,
                              page_urls_to_write));
}

void HistoryService::CanSetOnDemandFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(page_url)) {
    std::move(callback).Run(false);
    return;
  }

  PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::CanSetOnDemandFavicons, history_backend_,
                     page_url, icon_type),
      std::move(callback));
}

void HistoryService::SetOnDemandFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const GURL& icon_url,
    const std::vector<SkBitmap>& bitmaps,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(page_url)) {
    std::move(callback).Run(false);
    return;
  }

  PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::SetOnDemandFavicons, history_backend_,
                     page_url, icon_type, icon_url, bitmaps),
      std::move(callback));
}

void HistoryService::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetFaviconsOutOfDateForPage,
                              history_backend_, page_url));
}

void HistoryService::TouchOnDemandFavicon(const GURL& icon_url) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::TouchOnDemandFavicon,
                              history_backend_, icon_url));
}

void HistoryService::SetImportedFavicons(
    const favicon_base::FaviconUsageDataList& favicon_usage) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::SetImportedFavicons,
                              history_backend_, favicon_usage));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryURL(
    const GURL& url,
    bool want_visits,
    QueryURLCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  QueryURLResult* query_url_result = new QueryURLResult();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryURL, history_backend_, url,
                     want_visits, base::Unretained(query_url_result)),
      base::BindOnce(&RunWithQueryURLResult, std::move(callback),
                     base::Owned(query_url_result)));
}

// Statistics ------------------------------------------------------------------

base::CancelableTaskTracker::TaskId HistoryService::GetHistoryCount(
    const Time& begin_time,
    const Time& end_time,
    const GetHistoryCountCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  return tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetHistoryCount, history_backend_, begin_time,
                 end_time),
      callback);
}

void HistoryService::CountUniqueHostsVisitedLastMonth(
    const GetHistoryCountCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  tracker->PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindRepeating(&HistoryBackend::CountUniqueHostsVisitedLastMonth,
                          history_backend_),
      callback);
}

// Downloads -------------------------------------------------------------------

// Handle creation of a download by creating an entry in the history service's
// 'downloads' table.
void HistoryService::CreateDownload(
    const DownloadRow& create_info,
    const HistoryService::DownloadCreateCallback& callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                             base::Bind(&HistoryBackend::CreateDownload,
                                        history_backend_, create_info),
                             callback);
}

void HistoryService::GetNextDownloadId(const DownloadIdCallback& callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetNextDownloadId, history_backend_),
      callback);
}

// Handle queries for a list of all downloads in the history database's
// 'downloads' table.
void HistoryService::QueryDownloads(const DownloadQueryCallback& callback) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<DownloadRow>* rows = new std::vector<DownloadRow>();
  std::unique_ptr<std::vector<DownloadRow>> scoped_rows(rows);
  // Beware! The first Bind() does not simply |scoped_rows.get()| because
  // std::move(scoped_rows) nullifies |scoped_rows|, and compilers do not
  // guarantee that the first Bind's arguments are evaluated before the second
  // Bind's arguments.
  backend_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryDownloads, history_backend_, rows),
      base::BindOnce(callback, std::move(scoped_rows)));
}

// Handle updates for a particular download. This is a 'fire and forget'
// operation, so we don't need to be called back.
void HistoryService::UpdateDownload(
    const DownloadRow& data,
    bool should_commit_immediately) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::BindOnce(&HistoryBackend::UpdateDownload, history_backend_,
                              data, should_commit_immediately));
}

void HistoryService::RemoveDownloads(const std::set<uint32_t>& ids) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::RemoveDownloads,
                                               history_backend_, ids));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryHistory(
    const base::string16& text_query,
    const QueryOptions& options,
    const QueryHistoryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  QueryResults* query_results = new QueryResults();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryHistory, history_backend_,
                     text_query, options, base::Unretained(query_results)),
      base::BindOnce(callback, base::Owned(query_results)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsFrom(
    const GURL& from_url,
    const QueryRedirectsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  RedirectList* result = new RedirectList();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryRedirectsFrom, history_backend_,
                     from_url, base::Unretained(result)),
      base::BindOnce(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsTo(
    const GURL& to_url,
    const QueryRedirectsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  RedirectList* result = new RedirectList();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryRedirectsTo, history_backend_,
                     to_url, base::Unretained(result)),
      base::BindOnce(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetVisibleVisitCountToHost(
    const GURL& url,
    const GetVisibleVisitCountToHostCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  VisibleVisitCountToHostResult* result = new VisibleVisitCountToHostResult();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::GetVisibleVisitCountToHost,
                     history_backend_, url, base::Unretained(result)),
      base::BindOnce(&RunWithVisibleVisitCountToHostResult, callback,
                     base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryMostVisitedURLs(
    int result_count,
    int days_back,
    const QueryMostVisitedURLsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  MostVisitedURLList* result = new MostVisitedURLList();
  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::QueryMostVisitedURLs, history_backend_,
                     result_count, days_back, base::Unretained(result)),
      base::BindOnce(callback, base::Owned(result)));
}

void HistoryService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyHistoryServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Inform the HistoryClient that we are shuting down.
  if (history_client_)
    history_client_->Shutdown();

  // Unload the backend.
  if (history_backend_) {
    // Get rid of the in-memory backend.
    in_memory_backend_.reset();

    ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::Closing,
                                                 std::move(history_backend_)));
  }

  // Clear |backend_task_runner_| to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;

  // Join the background thread, if any.
  thread_.reset();
}

bool HistoryService::Init(
    bool no_db,
    const HistoryDatabaseParams& history_database_params) {
  TRACE_EVENT0("browser,startup", "HistoryService::Init")
  SCOPED_UMA_HISTOGRAM_TIMER("History.HistoryServiceInitTime");
  DCHECK(thread_checker_.CalledOnValidThread());

  // Unit tests can inject |backend_task_runner_| before this is called.
  if (!backend_task_runner_) {
    if (thread_) {
      base::Thread::Options options;
      options.timer_slack = base::TIMER_SLACK_MAXIMUM;
      if (!thread_->StartWithOptions(options)) {
        Cleanup();
        return false;
      }
      backend_task_runner_ = thread_->task_runner();
    } else {
      backend_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    }
  }

  // Create the history backend.
  scoped_refptr<HistoryBackend> backend(base::MakeRefCounted<HistoryBackend>(
      std::make_unique<BackendDelegate>(weak_ptr_factory_.GetWeakPtr(),
                                        base::ThreadTaskRunnerHandle::Get()),
      history_client_ ? history_client_->CreateBackendClient() : nullptr,
      backend_task_runner_));
  history_backend_.swap(backend);

  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::Init, history_backend_, no_db,
                              history_database_params));

  delete_directive_handler_ = std::make_unique<DeleteDirectiveHandler>(
      base::BindRepeating(base::IgnoreResult(&HistoryService::ScheduleDBTask),
                          base::Unretained(this)));

  if (visit_delegate_ && !visit_delegate_->Init(this))
    return false;

  if (history_client_)
    history_client_->OnHistoryServiceCreated(this);

  return true;
}

void HistoryService::ScheduleAutocomplete(
    const base::Callback<void(HistoryBackend*, URLDatabase*)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::BindOnce(&HistoryBackend::ScheduleAutocomplete,
                              history_backend_, callback));
}

void HistoryService::ScheduleTask(SchedulePriority priority,
                                  base::OnceClosure task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(backend_task_runner_);
  // TODO(brettw): Do prioritization.
  // NOTE(mastiz): If this implementation changes, be cautious with implications
  // for sync, because a) the sync engine (sync thread) post tasks directly to
  // the task runner via ModelTypeProcessorProxy (which is subtle); and b)
  // ProfileSyncService (UI thread) does the same via
  // ProxyModelTypeControllerDelegate.
  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

base::WeakPtr<HistoryService> HistoryService::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<syncer::SyncableService>
HistoryService::GetDeleteDirectivesSyncableService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(delete_directive_handler_);
  return delete_directive_handler_->AsWeakPtr();
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
HistoryService::GetTypedURLSyncControllerDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Note that a callback is bound for GetTypedURLSyncControllerDelegate()
  // because this getter itself must also run in the backend sequence, and the
  // proxy object below will take care of that.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      backend_task_runner_,
      base::BindRepeating(&HistoryBackend::GetTypedURLSyncControllerDelegate,
                          base::Unretained(history_backend_.get())));
}

void HistoryService::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete_directive_handler_->ProcessLocalDeleteDirective(delete_directive);
}

void HistoryService::SetInMemoryBackend(
    std::unique_ptr<InMemoryHistoryBackend> mem_backend) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!in_memory_backend_) << "Setting mem DB twice";
  in_memory_backend_ = std::move(mem_backend);

  // The database requires additional initialization once we own it.
  in_memory_backend_->AttachToHistoryService(this);
}

void HistoryService::NotifyProfileError(sql::InitStatus init_status,
                                        const std::string& diagnostics) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_)
    history_client_->NotifyProfileError(init_status, diagnostics);
}

void HistoryService::DeleteURL(const GURL& url) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete notifications.
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::DeleteURL,
                                               history_backend_, url));
}

void HistoryService::DeleteURLsForTest(const std::vector<GURL>& urls) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete
  // notifications.
  ScheduleTask(PRIORITY_NORMAL, base::BindOnce(&HistoryBackend::DeleteURLs,
                                               history_backend_, urls));
}

void HistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time,
    bool user_initiated,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::ExpireHistoryBetween, history_backend_,
                     restrict_urls, begin_time, end_time, user_initiated),
      std::move(callback));
}

void HistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(backend_task_runner_.get(), FROM_HERE,
                            base::BindOnce(&HistoryBackend::ExpireHistory,
                                           history_backend_, expire_list),
                            std::move(callback));
}

void HistoryService::ExpireHistoryBeforeForTesting(
    base::Time end_time,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&HistoryBackend::ExpireHistoryBeforeForTesting,
                     history_backend_, end_time),
      std::move(callback));
}

void HistoryService::DeleteLocalAndRemoteHistoryBetween(
    WebHistoryService* web_history,
    Time begin_time,
    Time end_time,
    base::OnceClosure callback,
    base::CancelableTaskTracker* tracker) {
  // TODO(crbug.com/929111): This should be factored out into a separate class
  // that dispatches deletions to the proper places.
  if (web_history) {
    delete_directive_handler_->CreateDeleteDirectives(std::set<int64_t>(),
                                                      begin_time, end_time);

    // Attempt online deletion from the history server, but ignore the result.
    // Deletion directives ensure that the results will eventually be deleted.
    //
    // TODO(davidben): |callback| should not run until this operation completes
    // too.
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
        net::DefinePartialNetworkTrafficAnnotation(
            "web_history_expire_between_dates", "web_history_service", R"(
          semantics {
            description:
              "If a user who syncs their browsing history deletes history "
              "items for a time range, Chrome sends a request to a google.com "
              "host to execute the corresponding deletion serverside."
            trigger:
              "Deleting browsing history for a given time range, e.g. from the "
              "Clear Browsing Data dialog, by an extension, or the "
              "Clear-Site-Data header."
            data:
              "The begin and end timestamps of the selected time range, a "
              "version info token to resolve transaction conflicts, and an "
              "OAuth2 token authenticating the user."
          }
          policy {
            chrome_policy {
              AllowDeletingBrowserHistory {
                AllowDeletingBrowserHistory: false
              }
            }
          })");
    web_history->ExpireHistoryBetween(
        /*restrict_urls=*/{}, begin_time, end_time,
        base::Bind(&ExpireWebHistoryComplete), partial_traffic_annotation);
  }
  ExpireHistoryBetween(/*restrict_urls=*/{}, begin_time, end_time,
                       /*user_initiated=*/true, std::move(callback), tracker);
}

void HistoryService::DeleteLocalAndRemoteUrl(WebHistoryService* web_history,
                                             const GURL& url) {
  DCHECK(url.is_valid());
  // TODO(crbug.com/929111): This should be factored out into a separate class
  // that dispatches deletions to the proper places.
  if (web_history) {
    delete_directive_handler_->CreateUrlDeleteDirective(url);

    // Attempt online deletion from the history server, but ignore the result.
    // Deletion directives ensure that the results will eventually be deleted.
    net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
        net::DefinePartialNetworkTrafficAnnotation("web_history_delete_url",
                                                   "web_history_service", R"(
          semantics {
            description:
              "If a user who syncs their browsing history deletes urls from  "
              "history, Chrome sends a request to a google.com "
              "host to execute the corresponding deletion serverside."
            trigger:
              "Deleting urls from browsing history, e.g. by an extension."
            data:
              "The selected urls, a version info token to resolve transaction "
              "conflicts, and an OAuth2 token authenticating the user."
          }
          policy {
            chrome_policy {
              AllowDeletingBrowserHistory {
                AllowDeletingBrowserHistory: false
              }
            }
          })");
    web_history->ExpireHistoryBetween(
        /*restrict_urls=*/{url}, base::Time(), base::Time::Max(),
        base::Bind(&ExpireWebHistoryComplete), partial_traffic_annotation);
  }
  DeleteURL(url);
}

void HistoryService::OnDBLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_loaded_ = true;
  delete_directive_handler_->OnBackendLoaded();
  NotifyHistoryServiceLoaded();
}

void HistoryService::NotifyURLVisited(ui::PageTransition transition,
                                      const URLRow& row,
                                      const RedirectList& redirects,
                                      base::Time visit_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HistoryServiceObserver& observer : observers_)
    observer.OnURLVisited(this, transition, row, redirects, visit_time);
}

void HistoryService::NotifyURLsModified(const URLRows& changed_urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HistoryServiceObserver& observer : observers_)
    observer.OnURLsModified(this, changed_urls);
}

void HistoryService::NotifyURLsDeleted(const DeletionInfo& deletion_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!backend_task_runner_)
    return;

  // Inform the VisitDelegate of the deleted URLs. We will inform the delegate
  // of added URLs as soon as we get the add notification (we don't have to wait
  // for the backend, which allows us to be faster to update the state).
  //
  // For deleted URLs, we don't typically know what will be deleted since
  // delete notifications are by time. We would also like to be more
  // respectful of privacy and never tell the user something is gone when it
  // isn't. Therefore, we update the delete URLs after the fact.
  if (visit_delegate_) {
    if (deletion_info.IsAllHistory()) {
      visit_delegate_->DeleteAllURLs();
    } else {
      std::vector<GURL> urls;
      urls.reserve(deletion_info.deleted_rows().size());
      for (const auto& row : deletion_info.deleted_rows())
        urls.push_back(row.url());
      visit_delegate_->DeleteURLs(urls);
    }
  }

  for (HistoryServiceObserver& observer : observers_)
    observer.OnURLsDeleted(this, deletion_info);
}

void HistoryService::NotifyHistoryServiceLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HistoryServiceObserver& observer : observers_)
    observer.OnHistoryServiceLoaded(this);
}

void HistoryService::NotifyHistoryServiceBeingDeleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HistoryServiceObserver& observer : observers_)
    observer.HistoryServiceBeingDeleted(this);
}

void HistoryService::NotifyKeywordSearchTermUpdated(
    const URLRow& row,
    KeywordID keyword_id,
    const base::string16& term) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HistoryServiceObserver& observer : observers_)
    observer.OnKeywordSearchTermUpdated(this, row, keyword_id, term);
}

void HistoryService::NotifyKeywordSearchTermDeleted(URLID url_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (HistoryServiceObserver& observer : observers_)
    observer.OnKeywordSearchTermDeleted(this, url_id);
}

std::unique_ptr<
    base::CallbackList<void(const std::set<GURL>&, const GURL&)>::Subscription>
HistoryService::AddFaviconsChangedCallback(
    const HistoryService::OnFaviconsChangedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return favicon_changed_callback_list_.Add(callback);
}

void HistoryService::NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                           const GURL& icon_url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  favicon_changed_callback_list_.Notify(page_urls, icon_url);
}

}  // namespace history
