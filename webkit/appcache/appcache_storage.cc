// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_storage.h"

#include "base/stl_util-inl.h"

namespace appcache {

AppCacheStorage::AppCacheStorage(AppCacheService* service)
    : last_cache_id_(kUnitializedId), last_group_id_(kUnitializedId),
      last_entry_id_(kUnitializedId), last_response_id_(kUnitializedId),
      service_(service)  {
}

AppCacheStorage::~AppCacheStorage() {
  STLDeleteValues(&pending_info_loads_);
  DCHECK(delegate_references_.empty());
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

