// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_context.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/time.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/special_storage_policy.h"

using file_util::FileEnumerator;

namespace dom_storage {

DomStorageContext::UsageInfo::UsageInfo() {}
DomStorageContext::UsageInfo::~UsageInfo() {}

DomStorageContext::DomStorageContext(
    const FilePath& directory,
    quota::SpecialStoragePolicy* special_storage_policy,
    DomStorageTaskRunner* task_runner)
    : directory_(directory),
      task_runner_(task_runner),
      clear_local_state_(false),
      save_session_state_(false),
      special_storage_policy_(special_storage_policy) {
  // AtomicSequenceNum starts at 0 but we want to start session
  // namespace ids at one since zero is reserved for the
  // kLocalStorageNamespaceId.
  session_id_sequence_.GetNext();
}

DomStorageContext::~DomStorageContext() {
}

DomStorageNamespace* DomStorageContext::GetStorageNamespace(
    int64 namespace_id) {
  StorageNamespaceMap::iterator found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    if (namespace_id == kLocalStorageNamespaceId) {
      DomStorageNamespace* local =
          new DomStorageNamespace(directory_, task_runner_);
      namespaces_[kLocalStorageNamespaceId] = local;
      return local;
    }
    return NULL;
  }
  return found->second;
}

void DomStorageContext::GetUsageInfo(std::vector<UsageInfo>* infos) {
  FileEnumerator enumerator(directory_, false, FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.MatchesExtension(DomStorageArea::kDatabaseFileExtension)) {
      UsageInfo info;

      // Extract origin id from the path and construct a GURL.
      WebKit::WebString origin_id = webkit_glue::FilePathToWebString(
          path.BaseName().RemoveExtension());
      info.origin = GURL(WebKit::WebSecurityOrigin::
          createFromDatabaseIdentifier(origin_id).toString());

      FileEnumerator::FindInfo find_info;
      enumerator.GetFindInfo(&find_info);
      info.data_size = FileEnumerator::GetFilesize(find_info);
      info.last_modified = FileEnumerator::GetLastModifiedTime(find_info);

      infos->push_back(info);
    }
  }
}

void DomStorageContext::DeleteOrigin(const GURL& origin) {
  // TODO(michaeln): write me
}

void DomStorageContext::DeleteDataModifiedSince(const base::Time& cutoff) {
  // TODO(michaeln): write me
}

void DomStorageContext::PurgeMemory() {
  // TODO(michaeln): write me
}

void DomStorageContext::Shutdown() {
  // TODO(michaeln): write me
}

void DomStorageContext::AddEventObserver(EventObserver* observer) {
  event_observers_.AddObserver(observer);
}

void DomStorageContext::RemoveEventObserver(EventObserver* observer) {
  event_observers_.RemoveObserver(observer);
}

void DomStorageContext::NotifyItemSet(
    const DomStorageArea* area,
    const string16& key,
    const string16& new_value,
    const NullableString16& old_value,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDomStorageItemSet(area, key, new_value, old_value, page_url));
}

void DomStorageContext::NotifyItemRemoved(
    const DomStorageArea* area,
    const string16& key,
    const string16& old_value,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDomStorageItemRemoved(area, key, old_value, page_url));
}

void DomStorageContext::NotifyAreaCleared(
    const DomStorageArea* area,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDomStorageAreaCleared(area, page_url));
}

void DomStorageContext::CreateSessionNamespace(
    int64 namespace_id) {
  DCHECK(namespace_id != kLocalStorageNamespaceId);
  DCHECK(namespaces_.find(namespace_id) == namespaces_.end());
  namespaces_[namespace_id] = new DomStorageNamespace(
      namespace_id, task_runner_);
}

void DomStorageContext::DeleteSessionNamespace(
    int64 namespace_id) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  namespaces_.erase(namespace_id);
}

void DomStorageContext::CloneSessionNamespace(
    int64 existing_id, int64 new_id) {
  DCHECK_NE(kLocalStorageNamespaceId, existing_id);
  DCHECK_NE(kLocalStorageNamespaceId, new_id);
  StorageNamespaceMap::iterator found = namespaces_.find(existing_id);
  if (found != namespaces_.end())
    namespaces_[new_id] = found->second->Clone(new_id);
  else
    CreateSessionNamespace(new_id);
}

}  // namespace dom_storage
