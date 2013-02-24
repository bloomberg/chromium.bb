// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DOM_STORAGE_DOM_STORAGE_CONTEXT_H_
#define WEBKIT_DOM_STORAGE_DOM_STORAGE_CONTEXT_H_

#include <map>
#include <set>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/storage/webkit_storage_export.h"

class NullableString16;

namespace base {
class FilePath;
class Time;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace dom_storage {

class DomStorageArea;
class DomStorageNamespace;
class DomStorageSession;
class DomStorageTaskRunner;
class SessionStorageDatabase;
struct LocalStorageUsageInfo;
struct SessionStorageUsageInfo;

// The Context is the root of an object containment hierachy for
// Namespaces and Areas related to the owning profile.
// One instance is allocated in the main process for each profile,
// instance methods should be called serially in the background as
// determined by the task_runner. Specifcally not on chrome's non-blocking
// IO thread since these methods can result in blocking file io.
//
// In general terms, the DomStorage object relationships are...
//   Contexts (per-profile) own Namespaces which own Areas which share Maps.
//   Hosts(per-renderer) refer to Namespaces and Areas open in its renderer.
//   Sessions (per-tab) cause the creation and deletion of session Namespaces.
//
// Session Namespaces are cloned by initially making a shallow copy of
// all contained Areas, the shallow copies refer to the same refcounted Map,
// and does a deep copy-on-write if needed.
//
// Classes intended to be used by an embedder are DomStorageContext,
// DomStorageHost, and DomStorageSession. The other classes are for
// internal consumption.
class WEBKIT_STORAGE_EXPORT DomStorageContext
    : public base::RefCountedThreadSafe<DomStorageContext> {
 public:
  // An interface for observing Local and Session Storage events on the
  // background thread.
  class EventObserver {
   public:
    virtual void OnDomStorageItemSet(
        const DomStorageArea* area,
        const string16& key,
        const string16& new_value,
        const NullableString16& old_value,  // may be null on initial insert
        const GURL& page_url) = 0;
    virtual void OnDomStorageItemRemoved(
        const DomStorageArea* area,
        const string16& key,
        const string16& old_value,
        const GURL& page_url) = 0;
    virtual void OnDomStorageAreaCleared(
        const DomStorageArea* area,
        const GURL& page_url) = 0;

   protected:
    virtual ~EventObserver() {}
  };

  DomStorageContext(
      const base::FilePath& localstorage_directory,  // empty for incognito profiles
      const base::FilePath& sessionstorage_directory,  // empty for incognito profiles
      quota::SpecialStoragePolicy* special_storage_policy,
      DomStorageTaskRunner* task_runner);

  // Returns the directory path for localStorage, or an empty directory, if
  // there is no backing on disk.
  const base::FilePath& localstorage_directory() { return localstorage_directory_; }

  // Returns the directory path for sessionStorage, or an empty directory, if
  // there is no backing on disk.
  const base::FilePath& sessionstorage_directory() {
    return sessionstorage_directory_;
  }

  DomStorageTaskRunner* task_runner() const { return task_runner_; }
  DomStorageNamespace* GetStorageNamespace(int64 namespace_id);

  void GetLocalStorageUsage(std::vector<LocalStorageUsageInfo>* infos,
                            bool include_file_info);
  void GetSessionStorageUsage(std::vector<SessionStorageUsageInfo>* infos);
  void DeleteLocalStorage(const GURL& origin);
  void DeleteSessionStorage(const SessionStorageUsageInfo& usage_info);
  void PurgeMemory();

  // Used by content settings to alter the behavior around
  // what data to keep and what data to discard at shutdown.
  // The policy is not so straight forward to describe, see
  // the implementation for details.
  void SetForceKeepSessionState() {
    force_keep_session_state_ = true;
  }

  // Called when the owning BrowserContext is ending.
  // Schedules the commit of any unsaved changes and will delete
  // and keep data on disk per the content settings and special storage
  // policies. Contained areas and namespaces will stop functioning after
  // this method has been called.
  void Shutdown();

  // Methods to add, remove, and notify EventObservers.
  void AddEventObserver(EventObserver* observer);
  void RemoveEventObserver(EventObserver* observer);
  void NotifyItemSet(
      const DomStorageArea* area,
      const string16& key,
      const string16& new_value,
      const NullableString16& old_value,
      const GURL& page_url);
  void NotifyItemRemoved(
      const DomStorageArea* area,
      const string16& key,
      const string16& old_value,
      const GURL& page_url);
  void NotifyAreaCleared(
      const DomStorageArea* area,
      const GURL& page_url);

  // May be called on any thread.
  int64 AllocateSessionId() {
    return session_id_sequence_.GetNext();
  }

  std::string AllocatePersistentSessionId();

  // Must be called on the background thread.
  void CreateSessionNamespace(int64 namespace_id,
                              const std::string& persistent_namespace_id);
  void DeleteSessionNamespace(int64 namespace_id, bool should_persist_data);
  void CloneSessionNamespace(int64 existing_id, int64 new_id,
                             const std::string& new_persistent_id);

  // Starts backing sessionStorage on disk. This function must be called right
  // after DomStorageContext is created, before it's used.
  void SetSaveSessionStorageOnDisk();

  // Deletes all namespaces which don't have an associated DomStorageNamespace
  // alive. This function is used for deleting possible leftover data after an
  // unclean exit.
  void StartScavengingUnusedSessionStorage();

 private:
  friend class DomStorageContextTest;
  FRIEND_TEST_ALL_PREFIXES(DomStorageContextTest, Basics);
  friend class base::RefCountedThreadSafe<DomStorageContext>;
  typedef std::map<int64, scoped_refptr<DomStorageNamespace> >
      StorageNamespaceMap;

  ~DomStorageContext();

  void ClearSessionOnlyOrigins();

  // For scavenging unused sessionStorages.
  void FindUnusedNamespaces();
  void FindUnusedNamespacesInCommitSequence(
      const std::set<std::string>& namespace_ids_in_use,
      const std::set<std::string>& protected_persistent_session_ids);
  void DeleteNextUnusedNamespace();
  void DeleteNextUnusedNamespaceInCommitSequence();

  // Collection of namespaces keyed by id.
  StorageNamespaceMap namespaces_;

  // Where localstorage data is stored, maybe empty for the incognito use case.
  base::FilePath localstorage_directory_;

  // Where sessionstorage data is stored, maybe empty for the incognito use
  // case. Always empty until the file-backed session storage feature is
  // implemented.
  base::FilePath sessionstorage_directory_;

  // Used to schedule sequenced background tasks.
  scoped_refptr<DomStorageTaskRunner> task_runner_;

  // List of objects observing local storage events.
  ObserverList<EventObserver> event_observers_;

  // We use a 32 bit identifier for per tab storage sessions.
  // At a tab per second, this range is large enough for 68 years.
  base::AtomicSequenceNumber session_id_sequence_;

  bool is_shutdown_;
  bool force_keep_session_state_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<SessionStorageDatabase> session_storage_database_;

  // For cleaning up unused namespaces gradually.
  bool scavenging_started_;
  std::vector<std::string> deletable_persistent_namespace_ids_;

  // Persistent namespace IDs to protect from gradual deletion (they will
  // be needed for session restore).
  std::set<std::string> protected_persistent_session_ids_;

  // Mapping between persistent namespace IDs and namespace IDs for
  // sessionStorage.
  std::map<std::string, int64> persistent_namespace_id_to_namespace_id_;
};

}  // namespace dom_storage

#endif  // WEBKIT_DOM_STORAGE_DOM_STORAGE_CONTEXT_H_
