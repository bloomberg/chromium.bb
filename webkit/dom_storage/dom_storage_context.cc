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
#include "webkit/dom_storage/session_storage_database.h"
#include "webkit/quota/special_storage_policy.h"

using file_util::FileEnumerator;

namespace dom_storage {

static const int kSessionStoraceScavengingSeconds = 60;

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
      special_storage_policy_(special_storage_policy),
      scavenging_started_(false) {
  // AtomicSequenceNum starts at 0 but we want to start session
  // namespace ids at one since zero is reserved for the
  // kLocalStorageNamespaceId.
  session_id_sequence_.GetNext();
}

DomStorageContext::~DomStorageContext() {
  if (session_storage_database_.get()) {
    // SessionStorageDatabase shouldn't be deleted right away: deleting it will
    // potentially involve waiting in leveldb::DBImpl::~DBImpl, and waiting
    // shouldn't happen on this thread. This will unassign the ref counted
    // pointer without decreasing the ref count, and is balanced by the Release
    // that happens later.
    task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DomStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(&SessionStorageDatabase::Release,
                   base::Unretained(session_storage_database_.release())));
  }
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
  // TODO(marja): Get usage infos for sessionStorage (crbug.com/123599).
}

void DomStorageContext::DeleteOrigin(const GURL& origin) {
  DCHECK(!is_shutdown_);
  DomStorageNamespace* local = GetStorageNamespace(kLocalStorageNamespaceId);
  local->DeleteOrigin(origin);
  // Synthesize a 'cleared' event if the area is open so CachedAreas in
  // renderers get emptied out too.
  DomStorageArea* area = local->GetOpenStorageArea(origin);
  if (area)
    NotifyAreaCleared(area, origin);
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

  if (localstorage_directory_.empty() && !session_storage_database_.get())
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
      namespace_id, persistent_namespace_id, session_storage_database_.get(),
      task_runner_);
}

void DomStorageContext::DeleteSessionNamespace(
    int64 namespace_id, bool should_persist_data) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  if (session_storage_database_.get()) {
    std::string persistent_namespace_id = it->second->persistent_namespace_id();
    if (!should_persist_data) {
      task_runner_->PostShutdownBlockingTask(
          FROM_HERE,
          DomStorageTaskRunner::COMMIT_SEQUENCE,
          base::Bind(
              base::IgnoreResult(&SessionStorageDatabase::DeleteNamespace),
              session_storage_database_,
              persistent_namespace_id));
    } else if (!scavenging_started_) {
      // Protect the persistent namespace ID from scavenging.
      protected_persistent_session_ids_.insert(persistent_namespace_id);
    }
  }
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
  if (!localstorage_directory_.empty()) {
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
  if (session_storage_database_.get()) {
    std::vector<std::string> namespace_ids;
    session_storage_database_->ReadNamespaceIds(&namespace_ids);
    for (std::vector<std::string>::const_iterator it = namespace_ids.begin();
         it != namespace_ids.end(); ++it) {
      std::vector<GURL> origins;
      session_storage_database_->ReadOriginsInNamespace(*it, &origins);

      for (std::vector<GURL>::const_iterator origin_it = origins.begin();
           origin_it != origins.end(); ++origin_it) {
        if (special_storage_policy_->IsStorageProtected(*origin_it))
          continue;
        if (!special_storage_policy_->IsStorageSessionOnly(*origin_it))
          continue;
        session_storage_database_->DeleteArea(*it, *origin_it);
      }
    }
  }
}

void DomStorageContext::SetSaveSessionStorageOnDisk() {
  DCHECK(namespaces_.empty());
  if (!sessionstorage_directory_.empty()) {
    session_storage_database_ = new SessionStorageDatabase(
        sessionstorage_directory_);
  }
}

void DomStorageContext::StartScavengingUnusedSessionStorage() {
  if (session_storage_database_.get()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&DomStorageContext::FindUnusedNamespaces, this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DomStorageContext::FindUnusedNamespaces() {
  DCHECK(session_storage_database_.get());
  if (scavenging_started_)
    return;
  scavenging_started_ = true;
  std::set<std::string> namespace_ids_in_use;
  for (StorageNamespaceMap::const_iterator it = namespaces_.begin();
       it != namespaces_.end(); ++it)
    namespace_ids_in_use.insert(it->second->persistent_namespace_id());
  std::set<std::string> protected_persistent_session_ids;
  protected_persistent_session_ids.swap(protected_persistent_session_ids_);
  task_runner_->PostShutdownBlockingTask(
      FROM_HERE, DomStorageTaskRunner::COMMIT_SEQUENCE,
      base::Bind(
          &DomStorageContext::FindUnusedNamespacesInCommitSequence,
          this, namespace_ids_in_use, protected_persistent_session_ids));
}

void DomStorageContext::FindUnusedNamespacesInCommitSequence(
    const std::set<std::string>& namespace_ids_in_use,
    const std::set<std::string>& protected_persistent_session_ids) {
  DCHECK(session_storage_database_.get());
  // Delete all namespaces which don't have an associated DomStorageNamespace
  // alive.
  std::vector<std::string> namespace_ids;
  session_storage_database_->ReadNamespaceIds(&namespace_ids);
  for (std::vector<std::string>::const_iterator it = namespace_ids.begin();
       it != namespace_ids.end(); ++it) {
    if (namespace_ids_in_use.find(*it) == namespace_ids_in_use.end() &&
        protected_persistent_session_ids.find(*it) ==
        protected_persistent_session_ids.end()) {
      deletable_persistent_namespace_ids_.push_back(*it);
    }
  }
  if (!deletable_persistent_namespace_ids_.empty()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(
            &DomStorageContext::DeleteNextUnusedNamespace,
            this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

void DomStorageContext::DeleteNextUnusedNamespace() {
  if (is_shutdown_)
    return;
  task_runner_->PostShutdownBlockingTask(
        FROM_HERE, DomStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(
            &DomStorageContext::DeleteNextUnusedNamespaceInCommitSequence,
            this));
}

void DomStorageContext::DeleteNextUnusedNamespaceInCommitSequence() {
  if (deletable_persistent_namespace_ids_.empty())
    return;
  const std::string& persistent_id = deletable_persistent_namespace_ids_.back();
  session_storage_database_->DeleteNamespace(persistent_id);
  deletable_persistent_namespace_ids_.pop_back();
  if (!deletable_persistent_namespace_ids_.empty()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(
            &DomStorageContext::DeleteNextUnusedNamespace,
            this),
        base::TimeDelta::FromSeconds(kSessionStoraceScavengingSeconds));
  }
}

}  // namespace dom_storage
