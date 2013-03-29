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

DomStorageContext::DomStorageContext(
    const base::FilePath& localstorage_directory,
    const base::FilePath& sessionstorage_directory,
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
    // shouldn't happen on this thread.
    SessionStorageDatabase* to_release = session_storage_database_.get();
    to_release->AddRef();
    session_storage_database_ = NULL;
    task_runner_->PostShutdownBlockingTask(
        FROM_HERE,
        DomStorageTaskRunner::COMMIT_SEQUENCE,
        base::Bind(&SessionStorageDatabase::Release,
                   base::Unretained(to_release)));
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
          localstorage_directory_ = base::FilePath();
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

void DomStorageContext::GetLocalStorageUsage(
    std::vector<LocalStorageUsageInfo>* infos,
    bool include_file_info) {
  if (localstorage_directory_.empty())
    return;
  FileEnumerator enumerator(localstorage_directory_, false,
                            FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.MatchesExtension(DomStorageArea::kDatabaseFileExtension)) {
      LocalStorageUsageInfo info;
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

void DomStorageContext::GetSessionStorageUsage(
    std::vector<SessionStorageUsageInfo>* infos) {
  if (!session_storage_database_.get())
    return;
  std::map<std::string, std::vector<GURL> > namespaces_and_origins;
  session_storage_database_->ReadNamespacesAndOrigins(
      &namespaces_and_origins);
  for (std::map<std::string, std::vector<GURL> >::const_iterator it =
           namespaces_and_origins.begin();
       it != namespaces_and_origins.end(); ++it) {
    for (std::vector<GURL>::const_iterator origin_it = it->second.begin();
         origin_it != it->second.end(); ++origin_it) {
      SessionStorageUsageInfo info;
      info.persistent_namespace_id = it->first;
      info.origin = *origin_it;
      infos->push_back(info);
    }
  }
}

void DomStorageContext::DeleteLocalStorage(const GURL& origin) {
  DCHECK(!is_shutdown_);
  DomStorageNamespace* local = GetStorageNamespace(kLocalStorageNamespaceId);
  local->DeleteLocalStorageOrigin(origin);
  // Synthesize a 'cleared' event if the area is open so CachedAreas in
  // renderers get emptied out too.
  DomStorageArea* area = local->GetOpenStorageArea(origin);
  if (area)
    NotifyAreaCleared(area, origin);
}

void DomStorageContext::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info) {
  DCHECK(!is_shutdown_);
  DomStorageNamespace* dom_storage_namespace = NULL;
  std::map<std::string, int64>::const_iterator it =
      persistent_namespace_id_to_namespace_id_.find(
          usage_info.persistent_namespace_id);
  if (it != persistent_namespace_id_to_namespace_id_.end()) {
    dom_storage_namespace = GetStorageNamespace(it->second);
  } else {
    int64 namespace_id = AllocateSessionId();
    CreateSessionNamespace(namespace_id, usage_info.persistent_namespace_id);
    dom_storage_namespace = GetStorageNamespace(namespace_id);
  }
  dom_storage_namespace->DeleteSessionStorageOrigin(usage_info.origin);
  // Synthesize a 'cleared' event if the area is open so CachedAreas in
  // renderers get emptied out too.
  DomStorageArea* area =
      dom_storage_namespace->GetOpenStorageArea(usage_info.origin);
  if (area)
    NotifyAreaCleared(area, usage_info.origin);
}

void DomStorageContext::PurgeMemory() {
  // We can only purge memory from the local storage namespace
  // which is backed by disk.
  // TODO(marja): Purge sessionStorage, too. (Requires changes to the FastClear
  // functionality.)
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
    const base::string16& key,
    const base::string16& new_value,
    const NullableString16& old_value,
    const GURL& page_url) {
  FOR_EACH_OBSERVER(
      EventObserver, event_observers_,
      OnDomStorageItemSet(area, key, new_value, old_value, page_url));
}

void DomStorageContext::NotifyItemRemoved(
    const DomStorageArea* area,
    const base::string16& key,
    const base::string16& old_value,
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
  persistent_namespace_id_to_namespace_id_[persistent_namespace_id] =
      namespace_id;
}

void DomStorageContext::DeleteSessionNamespace(
    int64 namespace_id, bool should_persist_data) {
  DCHECK_NE(kLocalStorageNamespaceId, namespace_id);
  StorageNamespaceMap::const_iterator it = namespaces_.find(namespace_id);
  if (it == namespaces_.end())
    return;
  std::string persistent_namespace_id = it->second->persistent_namespace_id();
  if (session_storage_database_.get()) {
    if (!should_persist_data) {
      task_runner_->PostShutdownBlockingTask(
          FROM_HERE,
          DomStorageTaskRunner::COMMIT_SEQUENCE,
          base::Bind(
              base::IgnoreResult(&SessionStorageDatabase::DeleteNamespace),
              session_storage_database_,
              persistent_namespace_id));
    } else {
      // Ensure that the data gets committed before we shut down.
      it->second->Shutdown();
      if (!scavenging_started_) {
        // Protect the persistent namespace ID from scavenging.
        protected_persistent_session_ids_.insert(persistent_namespace_id);
      }
    }
  }
  persistent_namespace_id_to_namespace_id_.erase(persistent_namespace_id);
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
    std::vector<LocalStorageUsageInfo> infos;
    const bool kDontIncludeFileInfo = false;
    GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
    for (size_t i = 0; i < infos.size(); ++i) {
      const GURL& origin = infos[i].origin;
      if (special_storage_policy_->IsStorageProtected(origin))
        continue;
      if (!special_storage_policy_->IsStorageSessionOnly(origin))
        continue;

      const bool kNotRecursive = false;
      base::FilePath database_file_path = localstorage_directory_.Append(
          DomStorageArea::DatabaseFileNameFromOrigin(origin));
      file_util::Delete(database_file_path, kNotRecursive);
      file_util::Delete(
          DomStorageDatabase::GetJournalFilePath(database_file_path),
          kNotRecursive);
    }
  }
  if (session_storage_database_.get()) {
    std::vector<SessionStorageUsageInfo> infos;
    GetSessionStorageUsage(&infos);
    for (size_t i = 0; i < infos.size(); ++i) {
      const GURL& origin = infos[i].origin;
      if (special_storage_policy_->IsStorageProtected(origin))
        continue;
      if (!special_storage_policy_->IsStorageSessionOnly(origin))
        continue;
      session_storage_database_->DeleteArea(infos[i].persistent_namespace_id,
                                            origin);
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
  std::map<std::string, std::vector<GURL> > namespaces_and_origins;
  session_storage_database_->ReadNamespacesAndOrigins(&namespaces_and_origins);
  for (std::map<std::string, std::vector<GURL> >::const_iterator it =
           namespaces_and_origins.begin();
       it != namespaces_and_origins.end(); ++it) {
    if (namespace_ids_in_use.find(it->first) == namespace_ids_in_use.end() &&
        protected_persistent_session_ids.find(it->first) ==
        protected_persistent_session_ids.end()) {
      deletable_persistent_namespace_ids_.push_back(it->first);
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
