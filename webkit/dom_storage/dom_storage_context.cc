// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/dom_storage_context.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/time.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_database.h"
#include "webkit/dom_storage/dom_storage_namespace.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/quota/special_storage_policy.h"

using file_util::FileEnumerator;

namespace dom_storage {

DomStorageContext::UsageInfo::UsageInfo() : data_size(0) {}
DomStorageContext::UsageInfo::~UsageInfo() {}

DomStorageContext::DomStorageContext(
    const FilePath& localstorage_directory,
    const FilePath& sessionstorage_directory,
    quota::SpecialStoragePolicy* special_storage_policy,
    DomStorageTaskRunner* task_runner)
    : localstorage_directory_(localstorage_directory),
      sessionstorage_directory_(sessionstorage_directory),
      task_runner_(task_runner),
      is_shutdown_(false),
      force_keep_session_state_(false),
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
  if (is_shutdown_)
    return NULL;
  StorageNamespaceMap::iterator found = namespaces_.find(namespace_id);
  if (found == namespaces_.end()) {
    if (namespace_id == kLocalStorageNamespaceId) {
      if (!localstorage_directory_.empty()) {
        if (!file_util::CreateDirectory(localstorage_directory_)) {
          LOG(ERROR) << "Failed to create 'Local Storage' directory,"
                        " falling back to in-memory only.";
          localstorage_directory_ = FilePath();
        }
      }
      DomStorageNamespace* local =
          new DomStorageNamespace(localstorage_directory_, task_runner_);
      namespaces_[kLocalStorageNamespaceId] = local;
      return local;
    }
    return NULL;
  }
  return found->second;
}

void DomStorageContext::GetUsageInfo(std::vector<UsageInfo>* infos,
                                     bool include_file_info) {
  if (localstorage_directory_.empty())
    return;
  FileEnumerator enumerator(localstorage_directory_, false,
                            FileEnumerator::FILES);
  for (FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.MatchesExtension(DomStorageArea::kDatabaseFileExtension)) {
      UsageInfo info;
      info.origin = DomStorageArea::OriginFromDatabaseFileName(path);
      if (include_file_info) {
        FileEnumerator::FindInfo find_info;
        enumerator.GetFindInfo(&find_info);
        info.data_size = FileEnumerator::GetFilesize(find_info);
        info.last_modified = FileEnumerator::GetLastModifiedTime(find_info);
      }
      infos->push_back(info);
    }
  }
}

void DomStorageContext::DeleteOrigin(const GURL& origin) {
  DCHECK(!is_shutdown_);
  DomStorageNamespace* local = GetStorageNamespace(kLocalStorageNamespaceId);
  local->DeleteOrigin(origin);
}

void DomStorageContext::PurgeMemory() {
  // We can only purge memory from the local storage namespace
  // which is backed by disk.
  StorageNamespaceMap::iterator found =
      namespaces_.find(kLocalStorageNamespaceId);
  if (found != namespaces_.end())
    found->second->PurgeMemory();
}

void DomStorageContext::Shutdown() {
  is_shutdown_ = true;
  StorageNamespaceMap::const_iterator it = namespaces_.begin();
  for (; it != namespaces_.end(); ++it)
    it->second->Shutdown();

  if (localstorage_directory_.empty())
    return;

  // Respect the content policy settings about what to
  // keep and what to discard.
  if (force_keep_session_state_)
    return;  // Keep everything.

  bool has_session_only_origins =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  if (has_session_only_origins) {
    // We may have to delete something. We continue on the
    // commit sequence after area shutdown tasks have cycled
    // thru that sequence (and closed their database files).
    bool success = task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DomStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(&DomStorageContext::ClearSessionOnlyOrigins, this));
    DCHECK(success);
  }
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

std::string DomStorageContext::AllocatePersistentSessionId() {
  std::string guid = base::GenerateGUID();
  std::replace(guid.begin(), guid.end(), '-', '_');
  return guid;
}

void DomStorageContext::CreateSessionNamespace(
    int64 namespace_id,
    const std::string& persistent_namespace_id) {
  if (is_shutdown_)
    return;
  DCHECK(namespace_id != kLocalStorageNamespaceId);
  DCHECK(namespaces_.find(namespace_id) == namespaces_.end());
  namespaces_[namespace_id] = new DomStorageNamespace(
      namespace_id, persistent_namespace_id, task_runner_);
}

void DomStorageContext::DeleteSessionNamespace(
    int64 namespace_id, bool should_persist_data) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  // TODO(marja): Protect the sessionStorage data (once it's written on disk).
  namespaces_.erase(namespace_id);
}

void DomStorageContext::CloneSessionNamespace(
    int64 existing_id, int64 new_id,
    const std::string& new_persistent_id) {
  if (is_shutdown_)
    return;
  DCHECK_NE(kLocalStorageNamespaceId, existing_id);
  DCHECK_NE(kLocalStorageNamespaceId, new_id);
  StorageNamespaceMap::iterator found = namespaces_.find(existing_id);
  if (found != namespaces_.end())
    namespaces_[new_id] = found->second->Clone(new_id, new_persistent_id);
  else
    CreateSessionNamespace(new_id, new_persistent_id);
}

void DomStorageContext::ClearSessionOnlyOrigins() {
  std::vector<UsageInfo> infos;
  const bool kDontIncludeFileInfo = false;
  GetUsageInfo(&infos, kDontIncludeFileInfo);
  for (size_t i = 0; i < infos.size(); ++i) {
    const GURL& origin = infos[i].origin;
    if (special_storage_policy_->IsStorageProtected(origin))
      continue;
    if (!special_storage_policy_->IsStorageSessionOnly(origin))
      continue;

    const bool kNotRecursive = false;
    FilePath database_file_path = localstorage_directory_.Append(
        DomStorageArea::DatabaseFileNameFromOrigin(origin));
    file_util::Delete(database_file_path, kNotRecursive);
    file_util::Delete(
        DomStorageDatabase::GetJournalFilePath(database_file_path),
        kNotRecursive);
  }
}

}  // namespace dom_storage
