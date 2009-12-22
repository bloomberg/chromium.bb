// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_update_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "webkit/appcache/appcache_group.h"

namespace appcache {

static const int kBufferSize = 4096;
static const size_t kMaxConcurrentUrlFetches = 2;
static const int kMax503Retries = 3;

// Extra info associated with requests for use during response processing.
// This info is deleted when the URLRequest is deleted.
class UpdateJobInfo :  public URLRequest::UserData {
 public:
  enum RequestType {
    MANIFEST_FETCH,
    URL_FETCH,
    MASTER_ENTRY_FETCH,
    MANIFEST_REFETCH,
  };

  explicit UpdateJobInfo(RequestType request_type)
      : type_(request_type),
        buffer_(new net::IOBuffer(kBufferSize)),
        retry_503_attempts_(0),
        update_job_(NULL),
        request_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(write_callback_(
            this, &UpdateJobInfo::OnWriteComplete)) {
  }

  void SetUpResponseWriter(AppCacheResponseWriter* writer,
                         AppCacheUpdateJob* update,
                         URLRequest* request) {
    DCHECK(!response_writer_.get());
    response_writer_.reset(writer);
    update_job_ = update;
    request_ = request;
  }

  void OnWriteComplete(int result) {
    // A completed write may delete the URL request and this object.
    update_job_->OnWriteResponseComplete(result, request_, this);
  }

  RequestType type_;
  scoped_refptr<net::IOBuffer> buffer_;
  int retry_503_attempts_;

  // Info needed to write responses to storage and process callbacks.
  scoped_ptr<AppCacheResponseWriter> response_writer_;
  AppCacheUpdateJob* update_job_;
  URLRequest* request_;
  net::CompletionCallbackImpl<UpdateJobInfo> write_callback_;
};

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

 private:
  NotifyHostMap hosts_to_notify;
};

AppCacheUpdateJob::AppCacheUpdateJob(AppCacheService* service,
                                     AppCacheGroup* group)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      service_(service),
      group_(group),
      update_type_(UNKNOWN_TYPE),
      internal_state_(FETCH_MANIFEST),
      master_entries_completed_(0),
      url_fetches_completed_(0),
      manifest_url_request_(NULL),
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

  DCHECK(!manifest_url_request_);
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

void AppCacheUpdateJob::FetchManifest(bool is_first_fetch) {
  DCHECK(!manifest_url_request_);
  manifest_url_request_ = new URLRequest(manifest_url_, this);
  UpdateJobInfo::RequestType fetch_type = is_first_fetch ?
      UpdateJobInfo::MANIFEST_FETCH : UpdateJobInfo::MANIFEST_REFETCH;
  manifest_url_request_->SetUserData(this, new UpdateJobInfo(fetch_type));
  manifest_url_request_->set_context(service_->request_context());
  manifest_url_request_->set_load_flags(
      manifest_url_request_->load_flags() | net::LOAD_DISABLE_INTERCEPT);

  // Add any necessary Http headers before sending fetch request.
  if (is_first_fetch) {
    AppCacheEntry* entry = (update_type_ == UPGRADE_ATTEMPT) ?
        group_->newest_complete_cache()->GetEntry(manifest_url_) : NULL;
    if (entry) {
      // Asynchronously load response info for manifest from newest cache.
      service_->storage()->LoadResponseInfo(manifest_url_,
                                            entry->response_id(), this);
    } else {
      AddHttpHeadersAndFetch(manifest_url_request_, NULL);
    }
  } else {
    DCHECK(internal_state_ == REFETCH_MANIFEST);
    DCHECK(manifest_response_info_.get());
    AddHttpHeadersAndFetch(manifest_url_request_,
                           manifest_response_info_.get());
  }
}

void AppCacheUpdateJob::AddHttpHeadersAndFetch(
    URLRequest* request, const net::HttpResponseInfo* info) {
  DCHECK(request);
  if (info) {
    std::string extra_headers;

    // Add If-Modified-Since header if response info has Last-Modified header.
    const std::string last_modified = "Last-Modified";
    std::string last_modified_value;
    info->headers->EnumerateHeader(NULL, last_modified, &last_modified_value);
    if (!last_modified_value.empty()) {
      extra_headers.append("If-Modified-Since: ");
      extra_headers.append(last_modified_value);
    }

    // Add If-None-Match header if resposne info has ETag header.
    const std::string etag = "ETag";
    std::string etag_value;
    info->headers->EnumerateHeader(NULL, etag, &etag_value);
    if (!etag_value.empty()) {
      if (!extra_headers.empty())
        extra_headers.append("\r\n");
      extra_headers.append("If-None-Match: ");
      extra_headers.append(etag_value);
    }

    if (!extra_headers.empty())
      request->SetExtraRequestHeaders(extra_headers);
  }
  request->Start();
}

void AppCacheUpdateJob::OnResponseStarted(URLRequest *request) {
  if (request->status().is_success() &&
      (request->GetResponseCode() / 100) == 2) {
    // Write response info to storage for URL fetches. Wait for async write
    // completion before reading any response data.
    UpdateJobInfo* info =
        static_cast<UpdateJobInfo*>(request->GetUserData(this));
    if (info->type_ == UpdateJobInfo::URL_FETCH ||
        info->type_ == UpdateJobInfo::MASTER_ENTRY_FETCH) {
      info->SetUpResponseWriter(
          service_->storage()->CreateResponseWriter(manifest_url_),
          this, request);
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
          new HttpResponseInfoIOBuffer(
              new net::HttpResponseInfo(request->response_info()));
      info->response_writer_->WriteInfo(io_buffer, &info->write_callback_);
    } else {
      ReadResponseData(request);
    }
  } else {
    OnResponseCompleted(request);
  }
}

void AppCacheUpdateJob::ReadResponseData(URLRequest* request) {
  if (internal_state_ == CACHE_FAILURE || internal_state_ == CANCELLED ||
      internal_state_ == COMPLETED) {
    return;
  }

  int bytes_read = 0;
  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  request->Read(info->buffer_, kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void AppCacheUpdateJob::OnReadCompleted(URLRequest* request, int bytes_read) {
  bool data_consumed = true;
  if (request->status().is_success() && bytes_read > 0) {
    UpdateJobInfo* info =
        static_cast<UpdateJobInfo*>(request->GetUserData(this));

    data_consumed = ConsumeResponseData(request, info, bytes_read);
    if (data_consumed) {
      bytes_read = 0;
      while (request->Read(info->buffer_, kBufferSize, &bytes_read)) {
        if (bytes_read > 0) {
          data_consumed = ConsumeResponseData(request, info, bytes_read);
          if (!data_consumed)
            break;  // wait for async data processing, then read more
        } else {
          break;
        }
      }
    }
  }

  if (data_consumed && !request->status().is_io_pending())
    OnResponseCompleted(request);
}

bool AppCacheUpdateJob::ConsumeResponseData(URLRequest* request,
                                            UpdateJobInfo* info,
                                            int bytes_read) {
  DCHECK_GT(bytes_read, 0);
  switch (info->type_) {
    case UpdateJobInfo::MANIFEST_FETCH:
      manifest_data_.append(info->buffer_->data(), bytes_read);
      break;
    case UpdateJobInfo::URL_FETCH:
    case UpdateJobInfo::MASTER_ENTRY_FETCH:
      DCHECK(info->response_writer_.get());
      info->response_writer_->WriteData(info->buffer_, bytes_read,
                                        &info->write_callback_);
      return false;  // wait for async write completion to continue reading
    case UpdateJobInfo::MANIFEST_REFETCH:
      manifest_refetch_data_.append(info->buffer_->data(), bytes_read);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void AppCacheUpdateJob::OnWriteResponseComplete(int result,
                                                URLRequest* request,
                                                UpdateJobInfo* info) {
  if (result < 0) {
    request->Cancel();
    OnResponseCompleted(request);
    return;
  }

  ReadResponseData(request);
}

void AppCacheUpdateJob::OnReceivedRedirect(URLRequest* request,
                                           const GURL& new_url,
                                           bool* defer_redirect) {
  // Redirect is not allowed by the update process.
  request->Cancel();
  OnResponseCompleted(request);
}

void AppCacheUpdateJob::OnResponseCompleted(URLRequest* request) {
  // Retry for 503s where retry-after is 0.
  if (request->status().is_success() &&
      request->GetResponseCode() == 503 &&
      RetryRequest(request)) {
    return;
  }

  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  switch (info->type_) {
    case UpdateJobInfo::MANIFEST_FETCH:
      HandleManifestFetchCompleted(request);
      break;
    case UpdateJobInfo::URL_FETCH:
      HandleUrlFetchCompleted(request);
      break;
    case UpdateJobInfo::MASTER_ENTRY_FETCH:
      HandleMasterEntryFetchCompleted(request);
      break;
    case UpdateJobInfo::MANIFEST_REFETCH:
      HandleManifestRefetchCompleted(request);
      break;
    default:
      NOTREACHED();
  }

  delete request;
}

bool AppCacheUpdateJob::RetryRequest(URLRequest* request) {
  UpdateJobInfo* info =
      static_cast<UpdateJobInfo*>(request->GetUserData(this));
  if (info->retry_503_attempts_ >= kMax503Retries) {
    return false;
  }

  if (!request->response_headers()->HasHeaderValue("retry-after", "0"))
    return false;

  const GURL& url = request->original_url();
  URLRequest* retry = new URLRequest(url, this);
  UpdateJobInfo* retry_info = new UpdateJobInfo(info->type_);
  retry_info->retry_503_attempts_ = info->retry_503_attempts_ + 1;
  retry->SetUserData(this, retry_info);
  retry->set_context(request->context());
  retry->set_load_flags(request->load_flags());

  switch (info->type_) {
    case UpdateJobInfo::MANIFEST_FETCH:
    case UpdateJobInfo::MANIFEST_REFETCH:
      manifest_url_request_ = retry;
      manifest_data_.clear();
      break;
    case UpdateJobInfo::URL_FETCH:
      pending_url_fetches_.erase(url);
      pending_url_fetches_.insert(PendingUrlFetches::value_type(url, retry));
      break;
    case UpdateJobInfo::MASTER_ENTRY_FETCH:
      master_entry_fetches_.erase(url);
      master_entry_fetches_.insert(PendingUrlFetches::value_type(url, retry));
      break;
    default:
      NOTREACHED();
  }

  retry->Start();

  delete request;
  return true;
}

void AppCacheUpdateJob::HandleManifestFetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == FETCH_MANIFEST);
  manifest_url_request_ = NULL;

  int response_code = -1;
  std::string mime_type;
  if (request->status().is_success()) {
    response_code = request->GetResponseCode();
    request->GetMimeType(&mime_type);
  }

  if ((response_code / 100 == 2) && mime_type == kManifestMimeType) {
    manifest_response_info_.reset(
        new net::HttpResponseInfo(request->response_info()));
    if (update_type_ == UPGRADE_ATTEMPT)
      CheckIfManifestChanged();  // continues asynchronously
    else
      ContinueHandleManifestFetchCompleted(true);
  } else if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    ContinueHandleManifestFetchCompleted(false);
  } else if (response_code == 404 || response_code == 410) {
    service_->storage()->MakeGroupObsolete(group_, this);  // async
  } else {
    internal_state_ = CACHE_FAILURE;
    CancelAllMasterEntryFetches();
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
  }
}

void AppCacheUpdateJob::OnGroupMadeObsolete(AppCacheGroup* group,
                                            bool success) {
  DCHECK(master_entry_fetches_.empty());
  CancelAllMasterEntryFetches();
  if (success) {
    DCHECK(group->is_obsolete());
    NotifyAllAssociatedHosts(OBSOLETE_EVENT);
    internal_state_ = COMPLETED;
  } else {
    // Treat failure to mark group obsolete as a cache failure.
    internal_state_ = CACHE_FAILURE;
  }
  MaybeCompleteUpdate();
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
    LOG(INFO) << "Failed to parse manifest: " << manifest_url_;
    internal_state_ = CACHE_FAILURE;
    CancelAllMasterEntryFetches();
    MaybeCompleteUpdate();  // if not done, run async cache failure steps
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
      (*host_it)->AssociateCache(inprogress_cache_);
    }
  }

  group_->SetUpdateStatus(AppCacheGroup::DOWNLOADING);
  NotifyAllAssociatedHosts(DOWNLOADING_EVENT);
  FetchUrls();
  FetchMasterEntries();
  MaybeCompleteUpdate();  // if not done, continues when async fetches complete
}

void AppCacheUpdateJob::HandleUrlFetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == DOWNLOADING);

  const GURL& url = request->original_url();
  pending_url_fetches_.erase(url);
  ++url_fetches_completed_;

  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;
  AppCacheEntry& entry = url_file_list_.find(url)->second;

  if (request->status().is_success() && (response_code / 100 == 2)) {
    // Associate storage with the new entry.
    UpdateJobInfo* info =
        static_cast<UpdateJobInfo*>(request->GetUserData(this));
    DCHECK(info->response_writer_.get());
    entry.set_response_id(info->response_writer_->response_id());

    inprogress_cache_->AddOrModifyEntry(url, entry);

    // Foreign entries will be detected during cache selection.
    // Note: 6.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else {
    LOG(INFO) << "Request status: " << request->status().status()
        << " os_error: " << request->status().os_error()
        << " response code: " << response_code;

    // TODO(jennb): Discard any stored data for this entry? May be unnecessary
    // if handled automatically by storage layer.

    if (entry.IsExplicit() || entry.IsFallback()) {
      internal_state_ = CACHE_FAILURE;
      CancelAllUrlFetches();
      CancelAllMasterEntryFetches();
    } else if (response_code == 404 || response_code == 410) {
      // Entry is skipped.  They are dropped from the cache.
    } else if (update_type_ == UPGRADE_ATTEMPT) {
      // Copy the response from the newest complete cache.
      AppCache* cache = group_->newest_complete_cache();
      AppCacheEntry* copy = cache->GetEntry(url);
      if (copy) {
        entry.set_response_id(copy->response_id());
        inprogress_cache_->AddOrModifyEntry(url, entry);
      }
    }
  }

  // Fetch another URL now that one request has completed.
  if (internal_state_ != CACHE_FAILURE)
    FetchUrls();

  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleMasterEntryFetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // TODO(jennb): Handle downloads completing during cache failure when update
  // no longer fetches master entries directly. For now, we cancel all pending
  // master entry fetches when entering cache failure state so this will never
  // be called in CACHE_FAILURE state.

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
    UpdateJobInfo* info =
        static_cast<UpdateJobInfo*>(request->GetUserData(this));
    AppCache* cache = inprogress_cache_ ? inprogress_cache_.get() :
        group_->newest_complete_cache();
    DCHECK(info->response_writer_.get());
    AppCacheEntry master_entry(AppCacheEntry::MASTER,
                               info->response_writer_->response_id());
    cache->AddOrModifyEntry(url, master_entry);

    // In no-update case, associate host with the newest cache.
    if (!inprogress_cache_) {
      DCHECK(cache == group_->newest_complete_cache());
      for (PendingHosts::iterator host_it = hosts.begin();
           host_it != hosts.end(); ++host_it) {
        (*host_it)->AssociateCache(cache);
      }
    }
  } else {
    HostNotifier host_notifier;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host_notifier.AddHost(host);

      // In downloading case, disassociate host from inprogress cache.
      if (inprogress_cache_) {
        host->AssociateCache(NULL);
      }

      host->RemoveObserver(this);
    }
    hosts.clear();
    host_notifier.SendNotifications(ERROR_EVENT);

    // In downloading case, update result is different if all master entries
    // failed vs. only some failing.
    if (inprogress_cache_) {
      // Only count successful downloads to know if all master entries failed.
      pending_master_entries_.erase(found);
      --master_entries_completed_;

      // Section 6.9.4, step 22.3.
      if (update_type_ == CACHE_ATTEMPT && pending_master_entries_.empty()) {
        CancelAllUrlFetches();
        internal_state_ = CACHE_FAILURE;
      }
    }
  }

  if (internal_state_ != CACHE_FAILURE)
    FetchMasterEntries();

  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleManifestRefetchCompleted(URLRequest* request) {
  DCHECK(internal_state_ == REFETCH_MANIFEST);
  manifest_url_request_ = NULL;

  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;
  if (response_code == 304 || manifest_data_ == manifest_refetch_data_) {
    // Only need to store response in storage if manifest is not already
    // an entry in the cache.
    AppCacheEntry* entry = inprogress_cache_->GetEntry(manifest_url_);
    if (entry) {
      entry->add_types(AppCacheEntry::MANIFEST);
      CompleteInprogressCache();
    } else {
      manifest_response_writer_.reset(
          service_->storage()->CreateResponseWriter(manifest_url_));
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
          new HttpResponseInfoIOBuffer(manifest_response_info_.release());
      manifest_response_writer_->WriteInfo(io_buffer,
                                           &manifest_info_write_callback_);
    }
  } else {
    LOG(INFO) << "Request status: " << request->status().status()
        << " os_error: " << request->status().os_error()
        << " response code: " << response_code;
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::OnManifestInfoWriteComplete(int result) {
  if (result > 0) {
    scoped_refptr<net::StringIOBuffer> io_buffer =
        new net::StringIOBuffer(manifest_data_);
    manifest_response_writer_->WriteData(io_buffer, manifest_data_.length(),
                                         &manifest_data_write_callback_);
  } else {
    // Treat storage failure as if refetch of manifest failed.
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::OnManifestDataWriteComplete(int result) {
  if (result > 0) {
    AppCacheEntry entry(AppCacheEntry::MANIFEST,
        manifest_response_writer_->response_id());
    inprogress_cache_->AddOrModifyEntry(manifest_url_, entry);
    CompleteInprogressCache();
  } else {
    // Treat storage failure as if refetch of manifest failed.
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::CompleteInprogressCache() {
  inprogress_cache_->set_update_time(base::TimeTicks::Now());
  inprogress_cache_->set_complete(true);
  service_->storage()->StoreGroupAndNewestCache(group_, inprogress_cache_,
                                                this);  // async
  protect_new_cache_.swap(inprogress_cache_);
}

void AppCacheUpdateJob::OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                                    bool success) {
  if (success) {
    DCHECK_EQ(protect_new_cache_, group->newest_complete_cache());
    MaybeCompleteUpdate();  // will definitely complete
  } else {
    protect_new_cache_ = NULL;

    // Treat storage failure as if manifest refetch failed.
    HandleManifestRefetchFailure();
  }
}

void AppCacheUpdateJob::HandleManifestRefetchFailure() {
  ScheduleUpdateRetry(kRerunDelayMs);
  internal_state_ = CACHE_FAILURE;
  MaybeCompleteUpdate();  // will definitely complete
}

void AppCacheUpdateJob::NotifySingleHost(AppCacheHost* host,
                                         EventID event_id) {
  std::vector<int> ids(1, host->host_id());
  host->frontend()->OnEventRaised(ids, event_id);
}

void AppCacheUpdateJob::NotifyAllPendingMasterHosts(EventID event_id) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single pending master entry
  // so no need to worry about duplicate hosts being added to the notifier.
  HostNotifier host_notifier;
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts& hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      host_notifier.AddHost(*host_it);
    }
  }

  host_notifier.SendNotifications(event_id);
}

void AppCacheUpdateJob::NotifyAllAssociatedHosts(EventID event_id) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single cache so no need to worry
  // about duplicate hosts being added to the notifier.
  HostNotifier host_notifier;
  if (inprogress_cache_) {
    DCHECK(internal_state_ == DOWNLOADING || internal_state_ == CACHE_FAILURE);
    host_notifier.AddHosts(inprogress_cache_->associated_hosts());
  }

  AppCacheGroup::Caches old_caches = group_->old_caches();
  for (AppCacheGroup::Caches::const_iterator it = old_caches.begin();
       it != old_caches.end(); ++it) {
    host_notifier.AddHosts((*it)->associated_hosts());
  }

  AppCache* newest_cache = group_->newest_complete_cache();
  if (newest_cache)
    host_notifier.AddHosts(newest_cache->associated_hosts());

  // TODO(jennb): if progress event, also pass params lengthComputable=true,
  // total = url_file_list_.size(), loaded=url_fetches_completed_.
  host_notifier.SendNotifications(event_id);
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
    urls_to_fetch_.push_back(UrlsToFetch(url, false));
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
    // Notify about progress first to ensure it starts from 0% in case any
    // entries are skipped.
    NotifyAllAssociatedHosts(PROGRESS_EVENT);

    const GURL url = urls_to_fetch_.front().first;
    bool storage_checked = urls_to_fetch_.front().second;
    urls_to_fetch_.pop_front();

    AppCache::EntryMap::iterator it = url_file_list_.find(url);
    DCHECK(it != url_file_list_.end());
    AppCacheEntry& entry = it->second;
    if (ShouldSkipUrlFetch(entry)) {
      ++url_fetches_completed_;
    } else if (AlreadyFetchedEntry(url, entry.types())) {
      ++url_fetches_completed_;  // saved a URL request
    } else if (!storage_checked && MaybeLoadFromNewestCache(url, entry)) {
      // Continues asynchronously after data is loaded from newest cache.
    } else {
      // Send URL request for the resource.
      URLRequest* request = new URLRequest(url, this);
      request->SetUserData(this, new UpdateJobInfo(UpdateJobInfo::URL_FETCH));
      request->set_context(service_->request_context());
      request->set_load_flags(
          request->load_flags() | net::LOAD_DISABLE_INTERCEPT);
      request->Start();
      pending_url_fetches_.insert(PendingUrlFetches::value_type(url, request));
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
      host->AssociateCache(inprogress_cache_);  // always associate
      cache = inprogress_cache_.get();
    } else {
      cache = group_->newest_complete_cache();
    }

    // Update existing entry if it has already been fetched.
    AppCacheEntry* entry = cache->GetEntry(url);
    if (entry) {
      entry->add_types(AppCacheEntry::MASTER);
      if (internal_state_ == NO_UPDATE)
        host->AssociateCache(cache);  // only associate if have entry
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
        DCHECK(!inprogress_cache_.get());
        AppCache* cache = group_->newest_complete_cache();
        PendingMasters::iterator found = pending_master_entries_.find(url);
        DCHECK(found != pending_master_entries_.end());
        PendingHosts& hosts = found->second;
        for (PendingHosts::iterator host_it = hosts.begin();
             host_it != hosts.end(); ++host_it) {
          (*host_it)->AssociateCache(cache);
        }
      }
    } else {
      // Send URL request for the master entry.
      URLRequest* request = new URLRequest(url, this);
      request->SetUserData(this,
          new UpdateJobInfo(UpdateJobInfo::MASTER_ENTRY_FETCH));
      request->set_context(service_->request_context());
      request->set_load_flags(
          request->load_flags() | net::LOAD_DISABLE_INTERCEPT);
      request->Start();
      master_entry_fetches_.insert(PendingUrlFetches::value_type(url, request));
    }

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
}

void AppCacheUpdateJob::CancelAllMasterEntryFetches() {
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
      host->AssociateCache(NULL);
      host_notifier.AddHost(host);
      host->RemoveObserver(this);
    }
    hosts.clear();

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
  host_notifier.SendNotifications(ERROR_EVENT);
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
  service_->storage()->LoadResponseInfo(manifest_url_, copy_me->response_id(),
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
    AddHttpHeadersAndFetch(manifest_url_request_, http_info);
    return;
  }

  LoadingResponses::iterator found = loading_responses_.find(response_id);
  DCHECK(found != loading_responses_.end());
  const GURL& url = found->second;

  if (!http_info) {
    LoadFromNewestCacheFailed(url);  // no response found
  } else {
    // Check if response can be re-used according to HTTP caching semantics.
    // Responses with a "vary" header get treated as expired.
    const std::string name = "vary";
    std::string value;
    void* iter = NULL;
    if (http_info->headers->RequiresValidation(http_info->request_time,
                                               http_info->response_time,
                                               base::Time::Now()) ||
        http_info->headers->EnumerateHeader(&iter, name, &value)) {
      LoadFromNewestCacheFailed(url);
    } else {
      AppCache::EntryMap::iterator it = url_file_list_.find(url);
      DCHECK(it != url_file_list_.end());
      AppCacheEntry& entry = it->second;
      entry.set_response_id(response_id);
      inprogress_cache_->AddOrModifyEntry(url, entry);
      ++url_fetches_completed_;
    }
  }
  loading_responses_.erase(found);

  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::LoadFromNewestCacheFailed(const GURL& url) {
  if (internal_state_ == CACHE_FAILURE)
    return;

  // Re-insert url at front of fetch list. Indicate storage has been checked.
  urls_to_fetch_.push_front(AppCacheUpdateJob::UrlsToFetch(url, true));
  FetchUrls();
}

void AppCacheUpdateJob::MaybeCompleteUpdate() {
  // Must wait for any pending master entries or url fetches to complete.
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size()) {
    DCHECK(internal_state_ != COMPLETED);
    return;
  }

  switch (internal_state_) {
    case NO_UPDATE:
      // 6.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(NO_UPDATE_EVENT);
      internal_state_ = COMPLETED;
      break;
    case DOWNLOADING:
      internal_state_ = REFETCH_MANIFEST;
      FetchManifest(false);
      break;
    case REFETCH_MANIFEST:
      if (update_type_ == CACHE_ATTEMPT)
        NotifyAllAssociatedHosts(CACHED_EVENT);
      else
        NotifyAllAssociatedHosts(UPDATE_READY_EVENT);
      internal_state_ = COMPLETED;
      break;
    case CACHE_FAILURE:
      // 6.9.4 cache failure steps 2-8.
      NotifyAllAssociatedHosts(ERROR_EVENT);
      DiscardInprogressCache();
      // For a CACHE_ATTEMPT, group will be discarded when the host(s) that
      // started this update removes its reference to the group. Nothing more
      // to do here.
      internal_state_ = COMPLETED;
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

  if (manifest_url_request_) {
    delete manifest_url_request_;
    manifest_url_request_ = NULL;
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
  if (!inprogress_cache_)
    return;

  AppCache::AppCacheHosts& hosts = inprogress_cache_->associated_hosts();
  while (!hosts.empty())
    (*hosts.begin())->AssociateCache(NULL);

  // TODO(jennb): Cleanup stored responses for entries in the cache?
  // May not be necessary if handled automatically by storage layer.

  inprogress_cache_ = NULL;
}

void AppCacheUpdateJob::DeleteSoon() {
  ClearPendingMasterEntries();
  manifest_response_writer_.reset();
  service_->storage()->CancelDelegateCallbacks(this);

  // Break the connection with the group so the group cannot call delete
  // on this object after we've posted a task to delete ourselves.
  group_->SetUpdateStatus(AppCacheGroup::IDLE);
  protect_new_cache_ = NULL;
  group_ = NULL;

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace appcache
