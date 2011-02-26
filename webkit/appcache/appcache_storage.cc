// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_storage.h"

#include "base/stl_util-inl.h"
#include "webkit/appcache/appcache_response.h"

namespace appcache {

// static
const int64 AppCacheStorage::kUnitializedId = -1;

AppCacheStorage::AppCacheStorage(AppCacheService* service)
    : last_cache_id_(kUnitializedId), last_group_id_(kUnitializedId),
      last_response_id_(kUnitializedId), service_(service)  {
}

AppCacheStorage::~AppCacheStorage() {
  STLDeleteValues(&pending_info_loads_);
  DCHECK(delegate_references_.empty());
}

AppCacheStorage::DelegateReference::DelegateReference(
    Delegate* delegate, AppCacheStorage* storage)
    : delegate(delegate), storage(storage) {
  storage->delegate_references_.insert(
      DelegateReferenceMap::value_type(delegate, this));
}

AppCacheStorage::DelegateReference::~DelegateReference() {
  if (delegate)
    storage->delegate_references_.erase(delegate);
}

AppCacheStorage::ResponseInfoLoadTask::ResponseInfoLoadTask(
    const GURL& manifest_url,
    int64 response_id,
    AppCacheStorage* storage)
    : storage_(storage),
      manifest_url_(manifest_url),
      response_id_(response_id),
      info_buffer_(new HttpResponseInfoIOBuffer),
      ALLOW_THIS_IN_INITIALIZER_LIST(read_callback_(
          this, &ResponseInfoLoadTask::OnReadComplete)) {
  storage_->pending_info_loads_.insert(
      PendingResponseInfoLoads::value_type(response_id, this));
}

AppCacheStorage::ResponseInfoLoadTask::~ResponseInfoLoadTask() {
}

void AppCacheStorage::ResponseInfoLoadTask::StartIfNeeded() {
  if (reader_.get())
    return;
  reader_.reset(
      storage_->CreateResponseReader(manifest_url_, response_id_));
  reader_->ReadInfo(info_buffer_, &read_callback_);
}

void AppCacheStorage::ResponseInfoLoadTask::OnReadComplete(int result) {
  storage_->pending_info_loads_.erase(response_id_);
  scoped_refptr<AppCacheResponseInfo> info;
  if (result >= 0) {
    info = new AppCacheResponseInfo(storage_->service(), manifest_url_,
                                    response_id_,
                                    info_buffer_->http_info.release(),
                                    info_buffer_->response_data_size);
  }
  FOR_EACH_DELEGATE(delegates_, OnResponseInfoLoaded(info.get(), response_id_));
  delete this;
}

void AppCacheStorage::LoadResponseInfo(
    const GURL& manifest_url, int64 id, Delegate* delegate) {
  AppCacheResponseInfo* info = working_set_.GetResponseInfo(id);
  if (info) {
    delegate->OnResponseInfoLoaded(info, id);
    return;
  }
  ResponseInfoLoadTask* info_load =
      GetOrCreateResponseInfoLoadTask(manifest_url, id);
  DCHECK(manifest_url == info_load->manifest_url());
  DCHECK(id == info_load->response_id());
  info_load->AddDelegate(GetOrCreateDelegateReference(delegate));
  info_load->StartIfNeeded();
}

}  // namespace appcache

