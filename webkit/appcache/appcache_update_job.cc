// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_update_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "webkit/appcache/appcache_group.h"

namespace appcache {

static const int kBufferSize = 32768;
static const size_t kMaxConcurrentUrlFetches = 2;
static const int kMax503Retries = 3;

// Helper class for collecting hosts per frontend when sending notifications
// so that only one notification is sent for all hosts using the same frontend.
class HostNotifier {
 public:
  typedef std::vector<int> HostIds;
  typedef std::map<AppCacheFrontend*, HostIds> NotifyHostMap;

  // Caller is responsible for ensuring there will be no duplicate hosts.
  void AddHost(AppCacheHost* host) {
    std::pair<NotifyHostMap::iterator , bool> ret = hosts_to_notify.insert(
        NotifyHostMap::value_type(host->frontend(), HostIds()));
    ret.first->second.push_back(host->host_id());
  }

  void AddHosts(const std::set<AppCacheHost*>& hosts) {
    for (std::set<AppCacheHost*>::const_iterator it = hosts.begin();
         it != hosts.end(); ++it) {
      AddHost(*it);
    }
  }

  void SendNotifications(EventID event_id) {
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnEventRaised(it->second, event_id);
    }
  }

  void SendProgressNotifications(
      const GURL& url, int num_total, int num_complete) {
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnProgressEventRaised(it->second, url,
                                      num_total, num_complete);
    }
  }

  void SendErrorNotifications(const std::string& error_message) {
    DCHECK(!error_message.empty());
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnErrorEventRaised(it->second, error_message);
    }
  }

 private:
  NotifyHostMap hosts_to_notify;
};

AppCacheUpdateJob::UrlToFetch::UrlToFetch(const GURL& url,
                                          bool checked,
                                          AppCacheResponseInfo* info)
    : url(url),
      storage_checked(checked),
      existing_response_info(info) {
}

AppCacheUpdateJob::UrlToFetch::~UrlToFetch() {
}

// Helper class to fetch resources. Depending on the fetch type,
// can either fetch to an in-memory string or write the response
// data out to the disk cache.
AppCacheUpdateJob::URLFetcher::URLFetcher(
    const GURL& url, FetchType fetch_type, AppCacheUpdateJob* job)
    : url_(url), job_(job), fetch_type_(fetch_type), retry_503_attempts_(0),
      buffer_(new net::IOBuffer(kBufferSize)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          request_(new net::URLRequest(url, this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &URLFetcher::OnWriteComplete)) {
}

AppCacheUpdateJob::URLFetcher::~URLFetcher() {
}

void AppCacheUpdateJob::URLFetcher::Start() {
  request_->set_context(job_->service_->request_context());
  request_->set_load_flags(request_->load_flags() |
                           net::LOAD_DISABLE_INTERCEPT);
  if (existing_response_headers_)
    AddConditionalHeaders(existing_response_headers_);
  request_->Start();
}

void AppCacheUpdateJob::URLFetcher::OnReceivedRedirect(
    net::URLRequest* request, const GURL& new_url,  bool* defer_redirect) {
  DCHECK(request_ == request);
  // Redirect is not allowed by the update process.
  request->Cancel();
  OnResponseCompleted();
}

void AppCacheUpdateJob::URLFetcher::OnResponseStarted(
    net::URLRequest *request) {
  DCHECK(request == request_);
  if (request->status().is_success() &&
      (request->GetResponseCode() / 100) == 2) {

    // See http://code.google.com/p/chromium/issues/detail?id=69594
    // We willfully violate the HTML5 spec at this point in order
    // to support the appcaching of cross-origin HTTPS resources.
    // We've opted for a milder constraint and allow caching unless
    // the resource has a "no-store" header. A spec change has been
    // requested on the whatwg list.
    // TODO(michaeln): Consider doing this for cross-origin HTTP resources too.
    if (url_.SchemeIsSecure() &&
        url_.GetOrigin() != job_->manifest_url_.GetOrigin()) {
      if (request->response_headers()->
              HasHeaderValue("cache-control", "no-store")) {
        request->Cancel();
        OnResponseCompleted();
        return;
      }
    }

    // Write response info to storage for URL fetches. Wait for async write
    // completion before reading any response data.
    if (fetch_type_ == URL_FETCH || fetch_type_ == MASTER_ENTRY_FETCH) {
      response_writer_.reset(job_->CreateResponseWriter());
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
          new HttpResponseInfoIOBuffer(
              new net::HttpResponseInfo(request->response_info())));
      response_writer_->WriteInfo(io_buffer, &write_callback_);
    } else {
      ReadResponseData();
    }
  } else {
    OnResponseCompleted();
  }
}

void AppCacheUpdateJob::URLFetcher::OnReadCompleted(
    net::URLRequest* request, int bytes_read) {
  DCHECK(request_ == request);
  bool data_consumed = true;
  if (request->status().is_success() && bytes_read > 0) {
    data_consumed = ConsumeResponseData(bytes_read);
    if (data_consumed) {
      bytes_read = 0;
      while (request->Read(buffer_, kBufferSize, &bytes_read)) {
        if (bytes_read > 0) {
          data_consumed = ConsumeResponseData(bytes_read);
          if (!data_consumed)
            break;  // wait for async data processing, then read more
        } else {
          break;
        }
      }
    }
  }
  if (data_consumed && !request->status().is_io_pending())
    OnResponseCompleted();
}

void AppCacheUpdateJob::URLFetcher::AddConditionalHeaders(
    const net::HttpResponseHeaders* headers) {
  DCHECK(request_.get() && headers);
  net::HttpRequestHeaders extra_headers;

  // Add If-Modified-Since header if response info has Last-Modified header.
  const std::string last_modified = "Last-Modified";
  std::string last_modified_value;
  headers->EnumerateHeader(NULL, last_modified, &last_modified_value);
  if (!last_modified_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfModifiedSince,
                            last_modified_value);
  }

  // Add If-None-Match header if response info has ETag header.
  const std::string etag = "ETag";
  std::string etag_value;
  headers->EnumerateHeader(NULL, etag, &etag_value);
  if (!etag_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfNoneMatch,
                            etag_value);
  }
  if (!extra_headers.IsEmpty())
    request_->SetExtraRequestHeaders(extra_headers);
}

void  AppCacheUpdateJob::URLFetcher::OnWriteComplete(int result) {
  if (result < 0) {
    request_->Cancel();
    OnResponseCompleted();
    return;
  }
  ReadResponseData();
}

void AppCacheUpdateJob::URLFetcher::ReadResponseData() {
  InternalUpdateState state = job_->internal_state_;
  if (state == CACHE_FAILURE || state == CANCELLED || state == COMPLETED)
    return;
  int bytes_read = 0;
  request_->Read(buffer_, kBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

// Returns false if response data is processed asynchronously, in which
// case ReadResponseData will be invoked when it is safe to continue
// reading more response data from the request.
bool AppCacheUpdateJob::URLFetcher::ConsumeResponseData(int bytes_read) {
  DCHECK_GT(bytes_read, 0);
  switch (fetch_type_) {
    case MANIFEST_FETCH:
    case MANIFEST_REFETCH:
      manifest_data_.append(buffer_->data(), bytes_read);
      break;
    case URL_FETCH:
    case MASTER_ENTRY_FETCH:
      DCHECK(response_writer_.get());
      response_writer_->WriteData(buffer_, bytes_read,  &write_callback_);
      return false;  // wait for async write completion to continue reading
    default:
      NOTREACHED();
  }
  return true;
}

void AppCacheUpdateJob::URLFetcher::OnResponseCompleted() {
  // Retry for 503s where retry-after is 0.
  if (request_->status().is_success() &&
      request_->GetResponseCode() == 503 &&
      MaybeRetryRequest()) {
    return;
  }

  switch (fetch_type_) {
    case MANIFEST_FETCH:
      job_->HandleManifestFetchCompleted(this);
      break;
    case URL_FETCH:
      job_->HandleUrlFetchCompleted(this);
      break;
    case MASTER_ENTRY_FETCH:
      job_->HandleMasterEntryFetchCompleted(this);
      break;
    case MANIFEST_REFETCH:
      job_->HandleManifestRefetchCompleted(this);
      break;
    default:
      NOTREACHED();
  }

  delete this;
}

bool AppCacheUpdateJob::URLFetcher::MaybeRetryRequest() {
  if (retry_503_attempts_ >= kMax503Retries ||
      !request_->response_headers()->HasHeaderValue("retry-after", "0")) {
    return false;
  }
  ++retry_503_attempts_;
  request_.reset(new net::URLRequest(url_, this));
  Start();
  return true;
}

AppCacheUpdateJob::AppCacheUpdateJob(AppCacheService* service,
                                     AppCacheGroup* group)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      service_(service),
      group_(group),
      update_type_(UNKNOWN_TYPE),
      internal_state_(FETCH_MANIFEST),
      master_entries_completed_(0),
      url_fetches_completed_(0),
      manifest_fetcher_(NULL),
      stored_state_(UNSTORED),
      ALLOW_THIS_IN_INITIALIZER_LIST(manifest_info_write_callback_(
          this, &AppCacheUpdateJob::OnManifestInfoWriteComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(manifest_data_write_callback_(
          this, &AppCacheUpdateJob::OnManifestDataWriteComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(manifest_data_read_callback_(
          this, &AppCacheUpdateJob::OnManifestDataReadComplete)) {
  DCHECK(group_);
  manifest_url_ = group_->manifest_url();
}

AppCacheUpdateJob::~AppCacheUpdateJob() {
  if (internal_state_ != COMPLETED)
    Cancel();

  DCHECK(!manifest_fetcher_);
  DCHECK(pending_url_fetches_.empty());
  DCHECK(!inprogress_cache_);
  DCHECK(pending_master_entries_.empty());
  DCHECK(master_entry_fetches_.empty());

  if (group_)
    group_->SetUpdateStatus(AppCacheGroup::IDLE);
}

void AppCacheUpdateJob::StartUpdate(AppCacheHost* host,
                                    const GURL& new_master_resource) {
  DCHECK(group_->update_job() == this);
  DCHECK(!group_->is_obsolete());

  bool is_new_pending_master_entry = false;
  if (!new_master_resource.is_empty()) {
    DCHECK(new_master_resource == host->pending_master_entry_url());
    DCHECK(!new_master_resource.has_ref());
    DCHECK(new_master_resource.GetOrigin() == manifest_url_.GetOrigin());

    // Cannot add more to this update if already terminating.
    if (IsTerminating()) {
      group_->QueueUpdate(host, new_master_resource);
      return;
    }

    std::pair<PendingMasters::iterator, bool> ret =
        pending_master_entries_.insert(
            PendingMasters::value_type(new_master_resource, PendingHosts()));
    is_new_pending_master_entry = ret.second;
    ret.first->second.push_back(host);
    host->AddObserver(this);
  }

  // Notify host (if any) if already checking or downloading.
  AppCacheGroup::UpdateStatus update_status = group_->update_status();
  if (update_status == AppCacheGroup::CHECKING ||
      update_status == AppCacheGroup::DOWNLOADING) {
    if (host) {
      NotifySingleHost(host, CHECKING_EVENT);
      if (update_status == AppCacheGroup::DOWNLOADING)
        NotifySingleHost(host, DOWNLOADING_EVENT);

      // Add to fetch list or an existing entry if already fetched.
      if (!new_master_resource.is_empty()) {
        AddMasterEntryToFetchList(host, new_master_resource,
                                  is_new_pending_master_entry);
      }
    }
    return;
  }

  // Begin update process for the group.
  group_->SetUpdateStatus(AppCacheGroup::CHECKING);
  if (group_->HasCache()) {
    update_type_ = UPGRADE_ATTEMPT;
    NotifyAllAssociatedHosts(CHECKING_EVENT);
  } else {
    update_type_ = CACHE_ATTEMPT;
    DCHECK(host);
    NotifySingleHost(host, CHECKING_EVENT);
  }

  if (!new_master_resource.is_empty()) {
    AddMasterEntryToFetchList(host, new_master_resource,
                              is_new_pending_master_entry);
  }

  FetchManifest(true);
}

AppCacheResponseWriter* AppCacheUpdateJob::CreateResponseWriter() {
  AppCacheResponseWriter* writer =
      service_->storage()->CreateResponseWriter(manifest_url_,
                                                group_->group_id());
  stored_response_ids_.push_back(writer->response_id());
  return writer;
}

void AppCacheUpdateJob::HandleCacheFailure(const std::string& error_message) {
  // 6.9.4 cache failure steps 2-8.
  DCHECK(internal_state_ != CACHE_FAILURE);
  DCHECK(!error_message.empty());
  internal_state_ = CACHE_FAILURE;
  CancelAllUrlFetches();
  CancelAllMasterEntryFetches(error_message);
  NotifyAllError(error_message);
  DiscardInprogressCache();
  internal_state_ = COMPLETED;
  DeleteSoon();  // To unwind the stack prior to deletion.
}

void AppCacheUpdateJob::FetchManifest(bool is_first_fetch) {
  DCHECK(!manifest_fetcher_);
  manifest_fetcher_ = new URLFetcher(
     manifest_url_,
     is_first_fetch ? URLFetcher::MANIFEST_FETCH :
                      URLFetcher::MANIFEST_REFETCH,
     this);

  // Add any necessary Http headers before sending fetch request.
  if (is_first_fetch) {
    AppCacheEntry* entry = (update_type_ == UPGRADE_ATTEMPT) ?
        group_->newest_complete_cache()->GetEntry(manifest_url_) : NULL;
    if (entry) {
      // Asynchronously load response info for manifest from newest cache.
      service_->storage()->LoadResponseInfo(manifest_url_, group_->group_id(),
                                            entry->response_id(), this);
    } else {
      manifest_fetcher_->Start();
    }
  } else {
    DCHECK(internal_state_ == REFETCH_MANIFEST);
    DCHECK(manifest_response_info_.get());
    manifest_fetcher_->set_existing_response_headers(
        manifest_response_info_->headers);
    manifest_fetcher_->Start();
  }
}


void AppCacheUpdateJob::HandleManifestFetchCompleted(
    URLFetcher* fetcher) {
  DCHECK_EQ(internal_state_, FETCH_MANIFEST);
  DCHECK_EQ(manifest_fetcher_, fetcher);
  manifest_fetcher_ = NULL;

  net::URLRequest* request = fetcher->request();
  int response_code = -1;
  bool is_valid_response_code = false;
  if (request->status().is_success()) {
    response_code = request->GetResponseCode();
    is_valid_response_code = (response_code / 100 == 2);
  }

  if (is_valid_response_code) {
    manifest_data_ = fetcher->manifest_data();
    manifest_response_info_.reset(
        new net::HttpResponseInfo(request->response_info()));
    if (update_type_ == UPGRADE_ATTEMPT)
      CheckIfManifestChanged();  // continues asynchronously
    else
      ContinueHandleManifestFetchCompleted(true);
  } else if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    ContinueHandleManifestFetchCompleted(false);
  } else if ((response_code == 404 || response_code == 410) &&
             update_type_ == UPGRADE_ATTEMPT) {
    service_->storage()->MakeGroupObsolete(group_, this);  // async
  } else {
    const char* kFormatString = "Manifest fetch failed (%d) %s";
    std::string message = base::StringPrintf(kFormatString, response_code,
                                             manifest_url_.spec().c_str());
    HandleCacheFailure(message);
  }
}

void AppCacheUpdateJob::OnGroupMadeObsolete(AppCacheGroup* group,
                                            bool success) {
  DCHECK(master_entry_fetches_.empty());
  CancelAllMasterEntryFetches("The cache has been made obsolete, "
                              "the manifest file returned 404 or 410");
  if (success) {
    DCHECK(group->is_obsolete());
    NotifyAllAssociatedHosts(OBSOLETE_EVENT);
    internal_state_ = COMPLETED;
    MaybeCompleteUpdate();
  } else {
    // Treat failure to mark group obsolete as a cache failure.
    HandleCacheFailure("Failed to mark the cache as obsolete");
  }
}

void AppCacheUpdateJob::ContinueHandleManifestFetchCompleted(bool changed) {
  DCHECK(internal_state_ == FETCH_MANIFEST);

  if (!changed) {
    DCHECK(update_type_ == UPGRADE_ATTEMPT);
    internal_state_ = NO_UPDATE;

    // Wait for pending master entries to download.
    FetchMasterEntries();
    MaybeCompleteUpdate();  // if not done, run async 6.9.4 step 7 substeps
    return;
  }

  Manifest manifest;
  if (!ParseManifest(manifest_url_, manifest_data_.data(),
                     manifest_data_.length(), manifest)) {
    const char* kFormatString = "Failed to parse manifest %s";
    const std::string message = base::StringPrintf(kFormatString,
        manifest_url_.spec().c_str());
    HandleCacheFailure(message);
    VLOG(1) << message;
    return;
  }

  // Proceed with update process. Section 6.9.4 steps 8-20.
  internal_state_ = DOWNLOADING;
  inprogress_cache_ = new AppCache(service_,
                                   service_->storage()->NewCacheId());
  BuildUrlFileList(manifest);
  inprogress_cache_->InitializeWithManifest(&manifest);

  // Associate all pending master hosts with the newly created cache.
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts& hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      (*host_it)->AssociateIncompleteCache(inprogress_cache_, manifest_url_);
    }
  }

  group_->SetUpdateStatus(AppCacheGroup::DOWNLOADING);
  NotifyAllAssociatedHosts(DOWNLOADING_EVENT);
  FetchUrls();
  FetchMasterEntries();
  MaybeCompleteUpdate();  // if not done, continues when async fetches complete
}

void AppCacheUpdateJob::HandleUrlFetchCompleted(URLFetcher* fetcher) {
  DCHECK(internal_state_ == DOWNLOADING);

  net::URLRequest* request = fetcher->request();
  const GURL& url = request->original_url();
  pending_url_fetches_.erase(url);
  NotifyAllProgress(url);
  ++url_fetches_completed_;

  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;
  AppCacheEntry& entry = url_file_list_.find(url)->second;

  if (response_code / 100 == 2) {
    // Associate storage with the new entry.
    DCHECK(fetcher->response_writer());
    entry.set_response_id(fetcher->response_writer()->response_id());
    entry.set_response_size(fetcher->response_writer()->amount_written());
    if (!inprogress_cache_->AddOrModifyEntry(url, entry))
      duplicate_response_ids_.push_back(entry.response_id());

    // TODO(michaeln): Check for <html manifest=xxx>
    // See http://code.google.com/p/chromium/issues/detail?id=97930
    // if (entry.IsMaster() && !entry.IsExplicit())
    //   if (!manifestAttribute) skip it

    // Foreign entries will be detected during cache selection.
    // Note: 6.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else {
    VLOG(1) << "Request status: " << request->status().status()
            << " error: " << request->status().error()
            << " response code: " << response_code;
    if (entry.IsExplicit() || entry.IsFallback()) {
      if (response_code == 304 && fetcher->existing_entry().has_response_id()) {
        // Keep the existing response.
        entry.set_response_id(fetcher->existing_entry().response_id());
        entry.set_response_size(fetcher->existing_entry().response_size());
        inprogress_cache_->AddOrModifyEntry(url, entry);
      } else {
        const char* kFormatString = "Resource fetch failed (%d) %s";
        const std::string message = base::StringPrintf(kFormatString,
            response_code, url.spec().c_str());
        HandleCacheFailure(message);
        return;
      }
    } else if (response_code == 404 || response_code == 410) {
      // Entry is skipped.  They are dropped from the cache.
    } else if (update_type_ == UPGRADE_ATTEMPT &&
               fetcher->existing_entry().has_response_id()) {
      // Keep the existing response.
      // TODO(michaeln): Not sure this is a good idea. This is spec compliant
      // but the old resource may or may not be compatible with the new contents
      // of the cache. Impossible to know one way or the other.
      entry.set_response_id(fetcher->existing_entry().response_id());
      entry.set_response_size(fetcher->existing_entry().response_size());
      inprogress_cache_->AddOrModifyEntry(url, entry);
    }
  }

  // Fetch another URL now that one request has completed.
  DCHECK(internal_state_ != CACHE_FAILURE);
  FetchUrls();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleMasterEntryFetchCompleted(
    URLFetcher* fetcher) {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // TODO(jennb): Handle downloads completing during cache failure when update
  // no longer fetches master entries directly. For now, we cancel all pending
  // master entry fetches when entering cache failure state so this will never
  // be called in CACHE_FAILURE state.

  net::URLRequest* request = fetcher->request();
  const GURL& url = request->original_url();
  master_entry_fetches_.erase(url);
  ++master_entries_completed_;

  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;

  PendingMasters::iterator found = pending_master_entries_.find(url);
  DCHECK(found != pending_master_entries_.end());
  PendingHosts& hosts = found->second;

  // Section 6.9.4. No update case: step 7.3, else step 22.
  if (response_code / 100 == 2) {
    // Add fetched master entry to the appropriate cache.
    AppCache* cache = inprogress_cache_ ? inprogress_cache_.get() :
        group_->newest_complete_cache();
    DCHECK(fetcher->response_writer());
    AppCacheEntry master_entry(AppCacheEntry::MASTER,
                               fetcher->response_writer()->response_id(),
                               fetcher->response_writer()->amount_written());
    if (cache->AddOrModifyEntry(url, master_entry))
      added_master_entries_.push_back(url);
    else
      duplicate_response_ids_.push_back(master_entry.response_id());

    // In no-update case, associate host with the newest cache.
    if (!inprogress_cache_) {
      // TODO(michaeln): defer until the updated cache has been stored
      DCHECK(cache == group_->newest_complete_cache());
      for (PendingHosts::iterator host_it = hosts.begin();
           host_it != hosts.end(); ++host_it) {
        (*host_it)->AssociateCompleteCache(cache);
      }
    }
  } else {
    HostNotifier host_notifier;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host_notifier.AddHost(host);

      // In downloading case, disassociate host from inprogress cache.
      if (inprogress_cache_)
        host->AssociateNoCache(GURL());

      host->RemoveObserver(this);
    }
    hosts.clear();

    const char* kFormatString = "Master entry fetch failed (%d) %s";
    const std::string message = base::StringPrintf(kFormatString,
        response_code, request->url().spec().c_str());
    host_notifier.SendErrorNotifications(message);

    // In downloading case, update result is different if all master entries
    // failed vs. only some failing.
    if (inprogress_cache_) {
      // Only count successful downloads to know if all master entries failed.
      pending_master_entries_.erase(found);
      --master_entries_completed_;

      // Section 6.9.4, step 22.3.
      if (update_type_ == CACHE_ATTEMPT && pending_master_entries_.empty()) {
        HandleCacheFailure(message);
        return;
      }
    }
  }

  DCHECK(internal_state_ != CACHE_FAILURE);
  FetchMasterEntries();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleManifestRefetchCompleted(
    URLFetcher* fetcher) {
  DCHECK(internal_state_ == REFETCH_MANIFEST);
  DCHECK(manifest_fetcher_ == fetcher);
  manifest_fetcher_ = NULL;

  net::URLRequest* request = fetcher->request();
  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;
  if (response_code == 304 || manifest_data_ == fetcher->manifest_data()) {
    // Only need to store response in storage if manifest is not already
    // an entry in the cache.
    AppCacheEntry* entry = inprogress_cache_->GetEntry(manifest_url_);
    if (entry) {
      entry->add_types(AppCacheEntry::MANIFEST);
      StoreGroupAndCache();
    } else {
      manifest_response_writer_.reset(CreateResponseWriter());
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
          new HttpResponseInfoIOBuffer(manifest_response_info_.release()));
      manifest_response_writer_->WriteInfo(io_buffer,
                                           &manifest_info_write_callback_);
    }
  } else {
    VLOG(1) << "Request status: " << request->status().status()
            << " error: " << request->status().error()
            << " response code: " << response_code;
    ScheduleUpdateRetry(kRerunDelayMs);
    HandleCacheFailure("Manifest changed during update, scheduling retry");
  }
}

void AppCacheUpdateJob::OnManifestInfoWriteComplete(int result) {
  if (result > 0) {
    scoped_refptr<net::StringIOBuffer> io_buffer(
        new net::StringIOBuffer(manifest_data_));
    manifest_response_writer_->WriteData(io_buffer, manifest_data_.length(),
                                         &manifest_data_write_callback_);
  } else {
    HandleCacheFailure("Failed to write the manifest headers to storage");
  }
}

void AppCacheUpdateJob::OnManifestDataWriteComplete(int result) {
  if (result > 0) {
    AppCacheEntry entry(AppCacheEntry::MANIFEST,
        manifest_response_writer_->response_id(),
        manifest_response_writer_->amount_written());
    if (!inprogress_cache_->AddOrModifyEntry(manifest_url_, entry))
      duplicate_response_ids_.push_back(entry.response_id());
    StoreGroupAndCache();
  } else {
    HandleCacheFailure("Failed to write the manifest data to storage");
  }
}

void AppCacheUpdateJob::StoreGroupAndCache() {
  DCHECK(stored_state_ == UNSTORED);
  stored_state_ = STORING;
  scoped_refptr<AppCache> newest_cache;
  if (inprogress_cache_)
    newest_cache.swap(inprogress_cache_);
  else
    newest_cache = group_->newest_complete_cache();
  newest_cache->set_update_time(base::Time::Now());
  service_->storage()->StoreGroupAndNewestCache(group_, newest_cache,
                                                this);  // async
}

void AppCacheUpdateJob::OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                                    AppCache* newest_cache,
                                                    bool success,
                                                    bool would_exceed_quota) {
  DCHECK(stored_state_ == STORING);
  if (success) {
    stored_state_ = STORED;
    MaybeCompleteUpdate();  // will definitely complete
  } else {
    // Restore inprogress_cache_ to get the proper events delivered
    // and the proper cleanup to occur.
    if (newest_cache != group->newest_complete_cache())
      inprogress_cache_ = newest_cache;

    std::string message("Failed to commit new cache to storage");
    if (would_exceed_quota)
      message.append(", would exceed quota");
    HandleCacheFailure(message);
  }
}

void AppCacheUpdateJob::NotifySingleHost(AppCacheHost* host,
                                         EventID event_id) {
  std::vector<int> ids(1, host->host_id());
  host->frontend()->OnEventRaised(ids, event_id);
}

void AppCacheUpdateJob::NotifyAllAssociatedHosts(EventID event_id) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendNotifications(event_id);
}

void AppCacheUpdateJob::NotifyAllProgress(const GURL& url) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendProgressNotifications(
      url, url_file_list_.size(), url_fetches_completed_);
}

void AppCacheUpdateJob::NotifyAllFinalProgress() {
  DCHECK(url_file_list_.size() == url_fetches_completed_);
  NotifyAllProgress(GURL());
}

void AppCacheUpdateJob::NotifyAllError(const std::string& error_message) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendErrorNotifications(error_message);
}

void AppCacheUpdateJob::AddAllAssociatedHostsToNotifier(
    HostNotifier* host_notifier) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single cache so no need to worry
  // about duplicate hosts being added to the notifier.
  if (inprogress_cache_) {
    DCHECK(internal_state_ == DOWNLOADING || internal_state_ == CACHE_FAILURE);
    host_notifier->AddHosts(inprogress_cache_->associated_hosts());
  }

  AppCacheGroup::Caches old_caches = group_->old_caches();
  for (AppCacheGroup::Caches::const_iterator it = old_caches.begin();
       it != old_caches.end(); ++it) {
    host_notifier->AddHosts((*it)->associated_hosts());
  }

  AppCache* newest_cache = group_->newest_complete_cache();
  if (newest_cache)
    host_notifier->AddHosts(newest_cache->associated_hosts());
}

void AppCacheUpdateJob::OnDestructionImminent(AppCacheHost* host) {
  // The host is about to be deleted; remove from our collection.
  PendingMasters::iterator found =
      pending_master_entries_.find(host->pending_master_entry_url());
  DCHECK(found != pending_master_entries_.end());
  PendingHosts& hosts = found->second;
  PendingHosts::iterator it = std::find(hosts.begin(), hosts.end(), host);
  DCHECK(it != hosts.end());
  hosts.erase(it);
}

void AppCacheUpdateJob::CheckIfManifestChanged() {
  DCHECK(update_type_ == UPGRADE_ATTEMPT);
  AppCacheEntry* entry =
      group_->newest_complete_cache()->GetEntry(manifest_url_);
  DCHECK(entry);

  // Load manifest data from storage to compare against fetched manifest.
  manifest_response_reader_.reset(
      service_->storage()->CreateResponseReader(manifest_url_,
                                                group_->group_id(),
                                                entry->response_id()));
  read_manifest_buffer_ = new net::IOBuffer(kBufferSize);
  manifest_response_reader_->ReadData(read_manifest_buffer_, kBufferSize,
      &manifest_data_read_callback_);  // async read
}

void AppCacheUpdateJob::OnManifestDataReadComplete(int result) {
  if (result > 0) {
    loaded_manifest_data_.append(read_manifest_buffer_->data(), result);
    manifest_response_reader_->ReadData(read_manifest_buffer_, kBufferSize,
        &manifest_data_read_callback_);  // read more
  } else {
    read_manifest_buffer_ = NULL;
    manifest_response_reader_.reset();
    ContinueHandleManifestFetchCompleted(
        result < 0 || manifest_data_ != loaded_manifest_data_);
  }
}

void AppCacheUpdateJob::BuildUrlFileList(const Manifest& manifest) {
  for (base::hash_set<std::string>::const_iterator it =
           manifest.explicit_urls.begin();
       it != manifest.explicit_urls.end(); ++it) {
    AddUrlToFileList(GURL(*it), AppCacheEntry::EXPLICIT);
  }

  // TODO(michaeln): Add resources from intercept namepsaces too.
  // http://code.google.com/p/chromium/issues/detail?id=101565

  const std::vector<FallbackNamespace>& fallbacks =
      manifest.fallback_namespaces;
  for (std::vector<FallbackNamespace>::const_iterator it = fallbacks.begin();
       it != fallbacks.end(); ++it) {
     AddUrlToFileList(it->second, AppCacheEntry::FALLBACK);
  }

  // Add all master entries from newest complete cache.
  if (update_type_ == UPGRADE_ATTEMPT) {
    const AppCache::EntryMap& entries =
        group_->newest_complete_cache()->entries();
    for (AppCache::EntryMap::const_iterator it = entries.begin();
         it != entries.end(); ++it) {
      const AppCacheEntry& entry = it->second;
      if (entry.IsMaster())
        AddUrlToFileList(it->first, AppCacheEntry::MASTER);
    }
  }
}

void AppCacheUpdateJob::AddUrlToFileList(const GURL& url, int type) {
  std::pair<AppCache::EntryMap::iterator, bool> ret = url_file_list_.insert(
      AppCache::EntryMap::value_type(url, AppCacheEntry(type)));

  if (ret.second)
    urls_to_fetch_.push_back(UrlToFetch(url, false, NULL));
  else
    ret.first->second.add_types(type);  // URL already exists. Merge types.
}

void AppCacheUpdateJob::FetchUrls() {
  DCHECK(internal_state_ == DOWNLOADING);

  // Fetch each URL in the list according to section 6.9.4 step 17.1-17.3.
  // Fetch up to the concurrent limit. Other fetches will be triggered as each
  // each fetch completes.
  while (pending_url_fetches_.size() < kMaxConcurrentUrlFetches &&
         !urls_to_fetch_.empty()) {
    UrlToFetch url_to_fetch = urls_to_fetch_.front();
    urls_to_fetch_.pop_front();

    AppCache::EntryMap::iterator it = url_file_list_.find(url_to_fetch.url);
    DCHECK(it != url_file_list_.end());
    AppCacheEntry& entry = it->second;
    if (ShouldSkipUrlFetch(entry)) {
      NotifyAllProgress(url_to_fetch.url);
      ++url_fetches_completed_;
    } else if (AlreadyFetchedEntry(url_to_fetch.url, entry.types())) {
      NotifyAllProgress(url_to_fetch.url);
      ++url_fetches_completed_;  // saved a URL request
    } else if (!url_to_fetch.storage_checked &&
               MaybeLoadFromNewestCache(url_to_fetch.url, entry)) {
      // Continues asynchronously after data is loaded from newest cache.
    } else {
      URLFetcher* fetcher = new URLFetcher(
          url_to_fetch.url, URLFetcher::URL_FETCH, this);
      if (url_to_fetch.existing_response_info.get()) {
        DCHECK(group_->newest_complete_cache());
        AppCacheEntry* existing_entry =
            group_->newest_complete_cache()->GetEntry(url_to_fetch.url);
        DCHECK(existing_entry);
        DCHECK(existing_entry->response_id() ==
               url_to_fetch.existing_response_info->response_id());
        fetcher->set_existing_response_headers(
            url_to_fetch.existing_response_info->http_response_info()->headers);
        fetcher->set_existing_entry(*existing_entry);
      }
      fetcher->Start();
      pending_url_fetches_.insert(
          PendingUrlFetches::value_type(url_to_fetch.url, fetcher));
    }
  }
}

void AppCacheUpdateJob::CancelAllUrlFetches() {
  // Cancel any pending URL requests.
  for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
       it != pending_url_fetches_.end(); ++it) {
    delete it->second;
  }

  url_fetches_completed_ +=
      pending_url_fetches_.size() + urls_to_fetch_.size();
  pending_url_fetches_.clear();
  urls_to_fetch_.clear();
}

bool AppCacheUpdateJob::ShouldSkipUrlFetch(const AppCacheEntry& entry) {
  // 6.6.4 Step 17
  // If the resource URL being processed was flagged as neither an
  // "explicit entry" nor or a "fallback entry", then the user agent
  // may skip this URL.
  if (entry.IsExplicit() || entry.IsFallback()) {
    return false;
  }

  // TODO(jennb): decide if entry should be skipped to expire it from cache
  return false;
}

bool AppCacheUpdateJob::AlreadyFetchedEntry(const GURL& url,
                                            int entry_type) {
  DCHECK(internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE);
  AppCacheEntry* existing = inprogress_cache_ ?
      inprogress_cache_->GetEntry(url) :
      group_->newest_complete_cache()->GetEntry(url);
  if (existing) {
    existing->add_types(entry_type);
    return true;
  }
  return false;
}

void AppCacheUpdateJob::AddMasterEntryToFetchList(AppCacheHost* host,
                                                  const GURL& url,
                                                  bool is_new) {
  DCHECK(!IsTerminating());

  if (internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE) {
    AppCache* cache;
    if (inprogress_cache_) {
      // always associate
      host->AssociateIncompleteCache(inprogress_cache_, manifest_url_);
      cache = inprogress_cache_.get();
    } else {
      cache = group_->newest_complete_cache();
    }

    // Update existing entry if it has already been fetched.
    AppCacheEntry* entry = cache->GetEntry(url);
    if (entry) {
      entry->add_types(AppCacheEntry::MASTER);
      if (internal_state_ == NO_UPDATE && !inprogress_cache_) {
        // only associate if have entry
        host->AssociateCompleteCache(cache);
      }
      if (is_new)
        ++master_entries_completed_;  // pretend fetching completed
      return;
    }
  }

  // Add to fetch list if not already fetching.
  if (master_entry_fetches_.find(url) == master_entry_fetches_.end()) {
    master_entries_to_fetch_.insert(url);
    if (internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE)
      FetchMasterEntries();
  }
}

void AppCacheUpdateJob::FetchMasterEntries() {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // Fetch each master entry in the list, up to the concurrent limit.
  // Additional fetches will be triggered as each fetch completes.
  while (master_entry_fetches_.size() < kMaxConcurrentUrlFetches &&
         !master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();

    if (AlreadyFetchedEntry(url, AppCacheEntry::MASTER)) {
      ++master_entries_completed_;  // saved a URL request

      // In no update case, associate hosts to newest cache in group
      // now that master entry has been "successfully downloaded".
      if (internal_state_ == NO_UPDATE) {
        // TODO(michaeln): defer until the updated cache has been stored.
        DCHECK(!inprogress_cache_.get());
        AppCache* cache = group_->newest_complete_cache();
        PendingMasters::iterator found = pending_master_entries_.find(url);
        DCHECK(found != pending_master_entries_.end());
        PendingHosts& hosts = found->second;
        for (PendingHosts::iterator host_it = hosts.begin();
             host_it != hosts.end(); ++host_it) {
          (*host_it)->AssociateCompleteCache(cache);
        }
      }
    } else {
      URLFetcher* fetcher = new URLFetcher(
          url, URLFetcher::MASTER_ENTRY_FETCH, this);
      fetcher->Start();
      master_entry_fetches_.insert(PendingUrlFetches::value_type(url, fetcher));
    }

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
}

void AppCacheUpdateJob::CancelAllMasterEntryFetches(
    const std::string& error_message) {
  // For now, cancel all in-progress fetches for master entries and pretend
  // all master entries fetches have completed.
  // TODO(jennb): Delete this when update no longer fetches master entries
  // directly.

  // Cancel all in-progress fetches.
  for (PendingUrlFetches::iterator it = master_entry_fetches_.begin();
       it != master_entry_fetches_.end(); ++it) {
    delete it->second;
    master_entries_to_fetch_.insert(it->first);  // back in unfetched list
  }
  master_entry_fetches_.clear();

  master_entries_completed_ += master_entries_to_fetch_.size();

  // Cache failure steps, step 2.
  // Pretend all master entries that have not yet been fetched have completed
  // downloading. Unassociate hosts from any appcache and send ERROR event.
  HostNotifier host_notifier;
  while (!master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();
    PendingMasters::iterator found = pending_master_entries_.find(url);
    DCHECK(found != pending_master_entries_.end());
    PendingHosts& hosts = found->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host->AssociateNoCache(GURL());
      host_notifier.AddHost(host);
      host->RemoveObserver(this);
    }
    hosts.clear();

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
  host_notifier.SendErrorNotifications(error_message);
}

bool AppCacheUpdateJob::MaybeLoadFromNewestCache(const GURL& url,
                                                 AppCacheEntry& entry) {
  if (update_type_ != UPGRADE_ATTEMPT)
    return false;

  AppCache* newest = group_->newest_complete_cache();
  AppCacheEntry* copy_me = newest->GetEntry(url);
  if (!copy_me || !copy_me->has_response_id())
    return false;

  // Load HTTP headers for entry from newest cache.
  loading_responses_.insert(
      LoadingResponses::value_type(copy_me->response_id(), url));
  service_->storage()->LoadResponseInfo(manifest_url_, group_->group_id(),
                                        copy_me->response_id(),
                                        this);
  // Async: wait for OnResponseInfoLoaded to complete.
  return true;
}

void AppCacheUpdateJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info, int64 response_id) {
  const net::HttpResponseInfo* http_info = response_info ?
      response_info->http_response_info() : NULL;

  // Needed response info for a manifest fetch request.
  if (internal_state_ == FETCH_MANIFEST) {
    if (http_info)
      manifest_fetcher_->set_existing_response_headers(http_info->headers);
    manifest_fetcher_->Start();
    return;
  }

  LoadingResponses::iterator found = loading_responses_.find(response_id);
  DCHECK(found != loading_responses_.end());
  const GURL& url = found->second;

  if (!http_info) {
    LoadFromNewestCacheFailed(url, NULL);  // no response found
  } else {
    // Check if response can be re-used according to HTTP caching semantics.
    // Responses with a "vary" header get treated as expired.
    const std::string name = "vary";
    std::string value;
    void* iter = NULL;
    if (!http_info->headers ||
        http_info->headers->RequiresValidation(http_info->request_time,
                                               http_info->response_time,
                                               base::Time::Now()) ||
        http_info->headers->EnumerateHeader(&iter, name, &value)) {
      LoadFromNewestCacheFailed(url, response_info);
    } else {
      DCHECK(group_->newest_complete_cache());
      AppCacheEntry* copy_me = group_->newest_complete_cache()->GetEntry(url);
      DCHECK(copy_me);
      DCHECK(copy_me->response_id() == response_id);

      AppCache::EntryMap::iterator it = url_file_list_.find(url);
      DCHECK(it != url_file_list_.end());
      AppCacheEntry& entry = it->second;
      entry.set_response_id(response_id);
      entry.set_response_size(copy_me->response_size());
      inprogress_cache_->AddOrModifyEntry(url, entry);
      NotifyAllProgress(url);
      ++url_fetches_completed_;
    }
  }
  loading_responses_.erase(found);

  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::LoadFromNewestCacheFailed(
    const GURL& url, AppCacheResponseInfo* response_info) {
  if (internal_state_ == CACHE_FAILURE)
    return;

  // Re-insert url at front of fetch list. Indicate storage has been checked.
  urls_to_fetch_.push_front(UrlToFetch(url, true, response_info));
  FetchUrls();
}

void AppCacheUpdateJob::MaybeCompleteUpdate() {
  DCHECK(internal_state_ != CACHE_FAILURE);

  // Must wait for any pending master entries or url fetches to complete.
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size()) {
    DCHECK(internal_state_ != COMPLETED);
    return;
  }

  switch (internal_state_) {
    case NO_UPDATE:
      if (master_entries_completed_ > 0) {
        switch (stored_state_) {
          case UNSTORED:
            StoreGroupAndCache();
            return;
          case STORING:
            return;
          case STORED:
            break;
        }
      }
      // 6.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(NO_UPDATE_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = COMPLETED;
      break;
    case DOWNLOADING:
      internal_state_ = REFETCH_MANIFEST;
      FetchManifest(false);
      break;
    case REFETCH_MANIFEST:
      DCHECK(stored_state_ == STORED);
      NotifyAllFinalProgress();
      if (update_type_ == CACHE_ATTEMPT)
        NotifyAllAssociatedHosts(CACHED_EVENT);
      else
        NotifyAllAssociatedHosts(UPDATE_READY_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = COMPLETED;
      break;
    case CACHE_FAILURE:
      NOTREACHED();  // See HandleCacheFailure
      break;
    default:
      break;
  }

  // Let the stack unwind before deletion to make it less risky as this
  // method is called from multiple places in this file.
  if (internal_state_ == COMPLETED)
    DeleteSoon();
}

void AppCacheUpdateJob::ScheduleUpdateRetry(int delay_ms) {
  // TODO(jennb): post a delayed task with the "same parameters" as this job
  // to retry the update at a later time. Need group, URLs of pending master
  // entries and their hosts.
}

void AppCacheUpdateJob::Cancel() {
  internal_state_ = CANCELLED;

  if (manifest_fetcher_) {
    delete manifest_fetcher_;
    manifest_fetcher_ = NULL;
  }

  for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
       it != pending_url_fetches_.end(); ++it) {
    delete it->second;
  }
  pending_url_fetches_.clear();

  for (PendingUrlFetches::iterator it = master_entry_fetches_.begin();
       it != master_entry_fetches_.end(); ++it) {
    delete it->second;
  }
  master_entry_fetches_.clear();

  ClearPendingMasterEntries();
  DiscardInprogressCache();

  // Delete response writer to avoid any callbacks.
  if (manifest_response_writer_.get())
    manifest_response_writer_.reset();

  service_->storage()->CancelDelegateCallbacks(this);
}

void AppCacheUpdateJob::ClearPendingMasterEntries() {
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts& hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      (*host_it)->RemoveObserver(this);
    }
  }

  pending_master_entries_.clear();
}

void AppCacheUpdateJob::DiscardInprogressCache() {
  service_->storage()->DoomResponses(manifest_url_, stored_response_ids_);

  if (!inprogress_cache_) {
    // We have to undo the changes we made, if any, to the existing cache.
    for (std::vector<GURL>::iterator iter = added_master_entries_.begin();
         iter != added_master_entries_.end(); ++iter) {
      DCHECK(group_->newest_complete_cache());
      group_->newest_complete_cache()->RemoveEntry(*iter);
    }
    return;
  }

  AppCache::AppCacheHosts& hosts = inprogress_cache_->associated_hosts();
  while (!hosts.empty())
    (*hosts.begin())->AssociateNoCache(GURL());

  inprogress_cache_ = NULL;
}

void AppCacheUpdateJob::DiscardDuplicateResponses() {
  service_->storage()->DoomResponses(manifest_url_, duplicate_response_ids_);
}

void AppCacheUpdateJob::DeleteSoon() {
  ClearPendingMasterEntries();
  manifest_response_writer_.reset();
  service_->storage()->CancelDelegateCallbacks(this);

  // Break the connection with the group so the group cannot call delete
  // on this object after we've posted a task to delete ourselves.
  group_->SetUpdateStatus(AppCacheGroup::IDLE);
  group_ = NULL;

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace appcache
