// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/directory.h"

#include "base/debug/trace_event.h"
#include "base/perftimer.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/syncable/base_transaction.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/on_disk_directory_backing_store.h"
#include "sync/syncable/read_transaction.h"
#include "sync/syncable/scoped_index_updater.h"
#include "sync/syncable/syncable-inl.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/write_transaction.h"

using std::string;

namespace syncer {
namespace syncable {

namespace {
// Helper function to add an item to the index, if it ought to be added.
template<typename Indexer>
void InitializeIndexEntry(EntryKernel* entry,
                          typename Index<Indexer>::Set* index) {
  if (Indexer::ShouldInclude(entry)) {
    index->insert(entry);
  }
}

}

// static
bool ClientTagIndexer::ShouldInclude(const EntryKernel* a) {
  return !a->ref(UNIQUE_CLIENT_TAG).empty();
}

bool ParentIdAndHandleIndexer::Comparator::operator() (
    const syncable::EntryKernel* a,
    const syncable::EntryKernel* b) const {
  int cmp = a->ref(PARENT_ID).compare(b->ref(PARENT_ID));
  if (cmp != 0)
    return cmp < 0;

  int64 a_position = a->ref(SERVER_POSITION_IN_PARENT);
  int64 b_position = b->ref(SERVER_POSITION_IN_PARENT);
  if (a_position != b_position)
    return a_position < b_position;

  cmp = a->ref(ID).compare(b->ref(ID));
  return cmp < 0;
}

// static
bool ParentIdAndHandleIndexer::ShouldInclude(const EntryKernel* a) {
  // This index excludes deleted items and the root item.  The root
  // item is excluded so that it doesn't show up as a child of itself.
  return !a->ref(IS_DEL) && !a->ref(ID).IsRoot();
}

// static
const FilePath::CharType Directory::kSyncDatabaseFilename[] =
    FILE_PATH_LITERAL("SyncData.sqlite3");

void Directory::InitKernelForTest(
    const std::string& name,
    DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer) {
  DCHECK(!kernel_);
  kernel_ = new Kernel(name, KernelLoadInfo(), delegate, transaction_observer);
}

Directory::PersistedKernelInfo::PersistedKernelInfo()
    : next_id(0) {
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    reset_download_progress(ModelTypeFromInt(i));
  }
}

Directory::PersistedKernelInfo::~PersistedKernelInfo() {}

void Directory::PersistedKernelInfo::reset_download_progress(
    ModelType model_type) {
  download_progress[model_type].set_data_type_id(
      GetSpecificsFieldNumberFromModelType(model_type));
  // An empty-string token indicates no prior knowledge.
  download_progress[model_type].set_token(std::string());
}

Directory::SaveChangesSnapshot::SaveChangesSnapshot()
    : kernel_info_status(KERNEL_SHARE_INFO_INVALID) {
}

Directory::SaveChangesSnapshot::~SaveChangesSnapshot() {}

Directory::Kernel::Kernel(
    const std::string& name,
    const KernelLoadInfo& info, DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer)
    : next_write_transaction_id(0),
      name(name),
      metahandles_index(new Directory::MetahandlesIndex),
      ids_index(new Directory::IdsIndex),
      parent_id_child_index(new Directory::ParentIdChildIndex),
      client_tag_index(new Directory::ClientTagIndex),
      unsynced_metahandles(new MetahandleSet),
      dirty_metahandles(new MetahandleSet),
      metahandles_to_purge(new MetahandleSet),
      info_status(Directory::KERNEL_SHARE_INFO_VALID),
      persisted_info(info.kernel_info),
      cache_guid(info.cache_guid),
      next_metahandle(info.max_metahandle + 1),
      delegate(delegate),
      transaction_observer(transaction_observer) {
  DCHECK(delegate);
  DCHECK(transaction_observer.IsInitialized());
}

Directory::Kernel::~Kernel() {
  delete unsynced_metahandles;
  delete dirty_metahandles;
  delete metahandles_to_purge;
  delete parent_id_child_index;
  delete client_tag_index;
  delete ids_index;
  STLDeleteElements(metahandles_index);
  delete metahandles_index;
}

Directory::Directory(
    DirectoryBackingStore* store,
    UnrecoverableErrorHandler* unrecoverable_error_handler,
    ReportUnrecoverableErrorFunction report_unrecoverable_error_function,
    NigoriHandler* nigori_handler,
    Cryptographer* cryptographer)
    : kernel_(NULL),
      store_(store),
      unrecoverable_error_handler_(unrecoverable_error_handler),
      report_unrecoverable_error_function_(
          report_unrecoverable_error_function),
      unrecoverable_error_set_(false),
      nigori_handler_(nigori_handler),
      cryptographer_(cryptographer),
      invariant_check_level_(VERIFY_CHANGES) {
}

Directory::~Directory() {
  Close();
}

DirOpenResult Directory::Open(
    const string& name,
    DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer) {
  TRACE_EVENT0("sync", "SyncDatabaseOpen");

  const DirOpenResult result =
      OpenImpl(name, delegate, transaction_observer);

  if (OPENED != result)
    Close();
  return result;
}

void Directory::InitializeIndices() {
  MetahandlesIndex::iterator it = kernel_->metahandles_index->begin();
  for (; it != kernel_->metahandles_index->end(); ++it) {
    EntryKernel* entry = *it;
    InitializeIndexEntry<ParentIdAndHandleIndexer>(entry,
        kernel_->parent_id_child_index);
    InitializeIndexEntry<IdIndexer>(entry, kernel_->ids_index);
    InitializeIndexEntry<ClientTagIndexer>(entry, kernel_->client_tag_index);
    const int64 metahandle = entry->ref(META_HANDLE);
    if (entry->ref(IS_UNSYNCED))
      kernel_->unsynced_metahandles->insert(metahandle);
    if (entry->ref(IS_UNAPPLIED_UPDATE)) {
      const ModelType type = entry->GetServerModelType();
      kernel_->unapplied_update_metahandles[type].insert(metahandle);
    }
    DCHECK(!entry->is_dirty());
  }
}

DirOpenResult Directory::OpenImpl(
    const string& name,
    DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>&
        transaction_observer) {

  KernelLoadInfo info;
  // Temporary indices before kernel_ initialized in case Load fails. We 0(1)
  // swap these later.
  MetahandlesIndex metas_bucket;
  DirOpenResult result = store_->Load(&metas_bucket, &info);
  if (OPENED != result)
    return result;

  kernel_ = new Kernel(name, info, delegate, transaction_observer);
  kernel_->metahandles_index->swap(metas_bucket);
  InitializeIndices();

  // Write back the share info to reserve some space in 'next_id'.  This will
  // prevent local ID reuse in the case of an early crash.  See the comments in
  // TakeSnapshotForSaveChanges() or crbug.com/142987 for more information.
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  if (!SaveChanges())
    return FAILED_INITIAL_WRITE;

  return OPENED;
}

void Directory::Close() {
  store_.reset();
  if (kernel_) {
    delete kernel_;
    kernel_ = NULL;
  }
}

void Directory::OnUnrecoverableError(const BaseTransaction* trans,
                                     const tracked_objects::Location& location,
                                     const std::string & message) {
  DCHECK(trans != NULL);
  unrecoverable_error_set_ = true;
  unrecoverable_error_handler_->OnUnrecoverableError(location,
                                                     message);
}


EntryKernel* Directory::GetEntryById(const Id& id) {
  ScopedKernelLock lock(this);
  return GetEntryById(id, &lock);
}

EntryKernel* Directory::GetEntryById(const Id& id,
                                     ScopedKernelLock* const lock) {
  DCHECK(kernel_);
  // Find it in the in memory ID index.
  kernel_->needle.put(ID, id);
  IdsIndex::iterator id_found = kernel_->ids_index->find(&kernel_->needle);
  if (id_found != kernel_->ids_index->end()) {
    return *id_found;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByClientTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  // Find it in the ClientTagIndex.
  kernel_->needle.put(UNIQUE_CLIENT_TAG, tag);
  ClientTagIndex::iterator found = kernel_->client_tag_index->find(
      &kernel_->needle);
  if (found != kernel_->client_tag_index->end()) {
    return *found;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByServerTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  // We don't currently keep a separate index for the tags.  Since tags
  // only exist for server created items that are the first items
  // to be created in a store, they should have small metahandles.
  // So, we just iterate over the items in sorted metahandle order,
  // looking for a match.
  MetahandlesIndex& set = *kernel_->metahandles_index;
  for (MetahandlesIndex::iterator i = set.begin(); i != set.end(); ++i) {
    if ((*i)->ref(UNIQUE_SERVER_TAG) == tag) {
      return *i;
    }
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByHandle(int64 metahandle) {
  ScopedKernelLock lock(this);
  return GetEntryByHandle(metahandle, &lock);
}

EntryKernel* Directory::GetEntryByHandle(int64 metahandle,
                                         ScopedKernelLock* lock) {
  // Look up in memory
  kernel_->needle.put(META_HANDLE, metahandle);
  MetahandlesIndex::iterator found =
      kernel_->metahandles_index->find(&kernel_->needle);
  if (found != kernel_->metahandles_index->end()) {
    // Found it in memory.  Easy.
    return *found;
  }
  return NULL;
}

bool Directory::GetChildHandlesById(
    BaseTransaction* trans, const Id& parent_id,
    Directory::ChildHandles* result) {
  if (!SyncAssert(this == trans->directory(), FROM_HERE,
                  "Directories don't match", trans))
    return false;
  result->clear();

  ScopedKernelLock lock(this);
  AppendChildHandles(lock, parent_id, result);
  return true;
}

bool Directory::GetChildHandlesByHandle(
    BaseTransaction* trans, int64 handle,
    Directory::ChildHandles* result) {
  if (!SyncAssert(this == trans->directory(), FROM_HERE,
                  "Directories don't match", trans))
    return false;

  result->clear();

  ScopedKernelLock lock(this);
  EntryKernel* kernel = GetEntryByHandle(handle, &lock);
  if (!kernel)
    return true;

  AppendChildHandles(lock, kernel->ref(ID), result);
  return true;
}

EntryKernel* Directory::GetRootEntry() {
  return GetEntryById(Id());
}

bool Directory::InsertEntry(WriteTransaction* trans, EntryKernel* entry) {
  ScopedKernelLock lock(this);
  return InsertEntry(trans, entry, &lock);
}

bool Directory::InsertEntry(WriteTransaction* trans,
                            EntryKernel* entry,
                            ScopedKernelLock* lock) {
  DCHECK(NULL != lock);
  if (!SyncAssert(NULL != entry, FROM_HERE, "Entry is null", trans))
    return false;

  static const char error[] = "Entry already in memory index.";
  if (!SyncAssert(kernel_->metahandles_index->insert(entry).second,
                  FROM_HERE,
                  error,
                  trans))
    return false;

  if (!entry->ref(IS_DEL)) {
    if (!SyncAssert(kernel_->parent_id_child_index->insert(entry).second,
                    FROM_HERE,
                    error,
                    trans)) {
      return false;
    }
  }
  if (!SyncAssert(kernel_->ids_index->insert(entry).second,
                  FROM_HERE,
                  error,
                  trans))
    return false;

  // Should NEVER be created with a client tag.
  if (!SyncAssert(entry->ref(UNIQUE_CLIENT_TAG).empty(), FROM_HERE,
                  "Client should be empty", trans))
    return false;

  return true;
}

bool Directory::ReindexId(WriteTransaction* trans,
                         EntryKernel* const entry,
                         const Id& new_id) {
  ScopedKernelLock lock(this);
  if (NULL != GetEntryById(new_id, &lock))
    return false;

  {
    // Update the indices that depend on the ID field.
    ScopedIndexUpdater<IdIndexer> updater_a(lock, entry, kernel_->ids_index);
    ScopedIndexUpdater<ParentIdAndHandleIndexer> updater_b(lock, entry,
        kernel_->parent_id_child_index);
    entry->put(ID, new_id);
  }
  return true;
}

bool Directory::ReindexParentId(WriteTransaction* trans,
                                EntryKernel* const entry,
                                const Id& new_parent_id) {
  ScopedKernelLock lock(this);

  {
    // Update the indices that depend on the PARENT_ID field.
    ScopedIndexUpdater<ParentIdAndHandleIndexer> index_updater(lock, entry,
        kernel_->parent_id_child_index);
    entry->put(PARENT_ID, new_parent_id);
  }
  return true;
}

bool Directory::unrecoverable_error_set(const BaseTransaction* trans) const {
  DCHECK(trans != NULL);
  return unrecoverable_error_set_;
}

void Directory::ClearDirtyMetahandles() {
  kernel_->transaction_mutex.AssertAcquired();
  kernel_->dirty_metahandles->clear();
}

bool Directory::SafeToPurgeFromMemory(WriteTransaction* trans,
                                      const EntryKernel* const entry) const {
  bool safe = entry->ref(IS_DEL) && !entry->is_dirty() &&
      !entry->ref(SYNCING) && !entry->ref(IS_UNAPPLIED_UPDATE) &&
      !entry->ref(IS_UNSYNCED);

  if (safe) {
    int64 handle = entry->ref(META_HANDLE);
    const ModelType type = entry->GetServerModelType();
    if (!SyncAssert(kernel_->dirty_metahandles->count(handle) == 0U,
                    FROM_HERE,
                    "Dirty metahandles should be empty", trans))
      return false;
    // TODO(tim): Bug 49278.
    if (!SyncAssert(!kernel_->unsynced_metahandles->count(handle),
                    FROM_HERE,
                    "Unsynced handles should be empty",
                    trans))
      return false;
    if (!SyncAssert(!kernel_->unapplied_update_metahandles[type].count(handle),
                    FROM_HERE,
                    "Unapplied metahandles should be empty",
                    trans))
      return false;
  }

  return safe;
}

void Directory::TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot) {
  ReadTransaction trans(FROM_HERE, this);
  ScopedKernelLock lock(this);

  // If there is an unrecoverable error then just bail out.
  if (unrecoverable_error_set(&trans))
    return;

  // Deep copy dirty entries from kernel_->metahandles_index into snapshot and
  // clear dirty flags.
  for (MetahandleSet::const_iterator i = kernel_->dirty_metahandles->begin();
       i != kernel_->dirty_metahandles->end(); ++i) {
    EntryKernel* entry = GetEntryByHandle(*i, &lock);
    if (!entry)
      continue;
    // Skip over false positives; it happens relatively infrequently.
    if (!entry->is_dirty())
      continue;
    snapshot->dirty_metas.insert(snapshot->dirty_metas.end(), *entry);
    DCHECK_EQ(1U, kernel_->dirty_metahandles->count(*i));
    // We don't bother removing from the index here as we blow the entire thing
    // in a moment, and it unnecessarily complicates iteration.
    entry->clear_dirty(NULL);
  }
  ClearDirtyMetahandles();

  // Set purged handles.
  DCHECK(snapshot->metahandles_to_purge.empty());
  snapshot->metahandles_to_purge.swap(*(kernel_->metahandles_to_purge));

  // Fill kernel_info_status and kernel_info.
  snapshot->kernel_info = kernel_->persisted_info;
  // To avoid duplicates when the process crashes, we record the next_id to be
  // greater magnitude than could possibly be reached before the next save
  // changes.  In other words, it's effectively impossible for the user to
  // generate 65536 new bookmarks in 3 seconds.
  snapshot->kernel_info.next_id -= 65536;
  snapshot->kernel_info_status = kernel_->info_status;
  // This one we reset on failure.
  kernel_->info_status = KERNEL_SHARE_INFO_VALID;
}

bool Directory::SaveChanges() {
  bool success = false;

  base::AutoLock scoped_lock(kernel_->save_changes_mutex);

  // Snapshot and save.
  SaveChangesSnapshot snapshot;
  TakeSnapshotForSaveChanges(&snapshot);
  success = store_->SaveChanges(snapshot);

  // Handle success or failure.
  if (success)
    success = VacuumAfterSaveChanges(snapshot);
  else
    HandleSaveChangesFailure(snapshot);
  return success;
}

bool Directory::VacuumAfterSaveChanges(const SaveChangesSnapshot& snapshot) {
  if (snapshot.dirty_metas.empty())
    return true;

  // Need a write transaction as we are about to permanently purge entries.
  WriteTransaction trans(FROM_HERE, VACUUM_AFTER_SAVE, this);
  ScopedKernelLock lock(this);
  // Now drop everything we can out of memory.
  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    kernel_->needle.put(META_HANDLE, i->ref(META_HANDLE));
    MetahandlesIndex::iterator found =
        kernel_->metahandles_index->find(&kernel_->needle);
    EntryKernel* entry = (found == kernel_->metahandles_index->end() ?
                          NULL : *found);
    if (entry && SafeToPurgeFromMemory(&trans, entry)) {
      // We now drop deleted metahandles that are up to date on both the client
      // and the server.
      size_t num_erased = 0;
      num_erased = kernel_->ids_index->erase(entry);
      DCHECK_EQ(1u, num_erased);
      num_erased = kernel_->metahandles_index->erase(entry);
      DCHECK_EQ(1u, num_erased);

      // Might not be in it
      num_erased = kernel_->client_tag_index->erase(entry);
      DCHECK_EQ(entry->ref(UNIQUE_CLIENT_TAG).empty(), !num_erased);
      if (!SyncAssert(!kernel_->parent_id_child_index->count(entry),
                      FROM_HERE,
                      "Deleted entry still present",
                      (&trans)))
        return false;
      delete entry;
    }
    if (trans.unrecoverable_error_set())
      return false;
  }
  return true;
}

bool Directory::PurgeEntriesWithTypeIn(ModelTypeSet types) {
  if (types.Empty())
    return true;

  {
    WriteTransaction trans(FROM_HERE, PURGE_ENTRIES, this);
    {
      ScopedKernelLock lock(this);
      MetahandlesIndex::iterator it = kernel_->metahandles_index->begin();
      while (it != kernel_->metahandles_index->end()) {
        const sync_pb::EntitySpecifics& local_specifics = (*it)->ref(SPECIFICS);
        const sync_pb::EntitySpecifics& server_specifics =
            (*it)->ref(SERVER_SPECIFICS);
        ModelType local_type = GetModelTypeFromSpecifics(local_specifics);
        ModelType server_type = GetModelTypeFromSpecifics(server_specifics);

        // Note the dance around incrementing |it|, since we sometimes erase().
        if ((IsRealDataType(local_type) && types.Has(local_type)) ||
            (IsRealDataType(server_type) && types.Has(server_type))) {
          if (!UnlinkEntryFromOrder(*it, &trans, &lock, DATA_TYPE_PURGE))
            return false;

          int64 handle = (*it)->ref(META_HANDLE);
          kernel_->metahandles_to_purge->insert(handle);

          size_t num_erased = 0;
          EntryKernel* entry = *it;
          num_erased = kernel_->ids_index->erase(entry);
          DCHECK_EQ(1u, num_erased);
          num_erased = kernel_->client_tag_index->erase(entry);
          DCHECK_EQ(entry->ref(UNIQUE_CLIENT_TAG).empty(), !num_erased);
          num_erased = kernel_->unsynced_metahandles->erase(handle);
          DCHECK_EQ(entry->ref(IS_UNSYNCED), num_erased > 0);
          num_erased =
              kernel_->unapplied_update_metahandles[server_type].erase(handle);
          DCHECK_EQ(entry->ref(IS_UNAPPLIED_UPDATE), num_erased > 0);
          num_erased = kernel_->parent_id_child_index->erase(entry);
          DCHECK_EQ(entry->ref(IS_DEL), !num_erased);
          kernel_->metahandles_index->erase(it++);
          delete entry;
        } else {
          ++it;
        }
      }

      // Ensure meta tracking for these data types reflects the deleted state.
      for (ModelTypeSet::Iterator it = types.First();
           it.Good(); it.Inc()) {
        set_initial_sync_ended_for_type_unsafe(it.Get(), false);
        kernel_->persisted_info.reset_download_progress(it.Get());
      }
    }
  }
  return true;
}

void Directory::HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot) {
  ScopedKernelLock lock(this);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;

  // Because we optimistically cleared the dirty bit on the real entries when
  // taking the snapshot, we must restore it on failure.  Not doing this could
  // cause lost data, if no other changes are made to the in-memory entries
  // that would cause the dirty bit to get set again. Setting the bit ensures
  // that SaveChanges will at least try again later.
  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    kernel_->needle.put(META_HANDLE, i->ref(META_HANDLE));
    MetahandlesIndex::iterator found =
        kernel_->metahandles_index->find(&kernel_->needle);
    if (found != kernel_->metahandles_index->end()) {
      (*found)->mark_dirty(kernel_->dirty_metahandles);
    }
  }

  kernel_->metahandles_to_purge->insert(snapshot.metahandles_to_purge.begin(),
                                        snapshot.metahandles_to_purge.end());
}

void Directory::GetDownloadProgress(
    ModelType model_type,
    sync_pb::DataTypeProgressMarker* value_out) const {
  ScopedKernelLock lock(this);
  return value_out->CopyFrom(
      kernel_->persisted_info.download_progress[model_type]);
}

void Directory::GetDownloadProgressAsString(
    ModelType model_type,
    std::string* value_out) const {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].SerializeToString(
      value_out);
}

size_t Directory::GetEntriesCount() const {
  ScopedKernelLock lock(this);
  return kernel_->metahandles_index ? kernel_->metahandles_index->size() : 0;
}

void Directory::SetDownloadProgress(
    ModelType model_type,
    const sync_pb::DataTypeProgressMarker& new_progress) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].CopyFrom(new_progress);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

ModelTypeSet Directory::initial_sync_ended_types() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.initial_sync_ended;
}

bool Directory::initial_sync_ended_for_type(ModelType type) const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.initial_sync_ended.Has(type);
}

template <class T> void Directory::TestAndSet(
    T* kernel_data, const T* data_to_set) {
  if (*kernel_data != *data_to_set) {
    *kernel_data = *data_to_set;
    kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  }
}

void Directory::set_initial_sync_ended_for_type(ModelType type, bool x) {
  ScopedKernelLock lock(this);
  set_initial_sync_ended_for_type_unsafe(type, x);
}

void Directory::set_initial_sync_ended_for_type_unsafe(ModelType type,
                                                       bool x) {
  if (kernel_->persisted_info.initial_sync_ended.Has(type) == x)
    return;
  if (x) {
    kernel_->persisted_info.initial_sync_ended.Put(type);
  } else {
    kernel_->persisted_info.initial_sync_ended.Remove(type);
  }
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

void Directory::SetNotificationStateUnsafe(
    const std::string& notification_state) {
  if (notification_state == kernel_->persisted_info.notification_state)
    return;
  kernel_->persisted_info.notification_state = notification_state;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

string Directory::store_birthday() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.store_birthday;
}

void Directory::set_store_birthday(const string& store_birthday) {
  ScopedKernelLock lock(this);
  if (kernel_->persisted_info.store_birthday == store_birthday)
    return;
  kernel_->persisted_info.store_birthday = store_birthday;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

string Directory::bag_of_chips() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.bag_of_chips;
}

void Directory::set_bag_of_chips(const string& bag_of_chips) {
  ScopedKernelLock lock(this);
  if (kernel_->persisted_info.bag_of_chips == bag_of_chips)
    return;
  kernel_->persisted_info.bag_of_chips = bag_of_chips;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

std::string Directory::GetNotificationState() const {
  ScopedKernelLock lock(this);
  std::string notification_state = kernel_->persisted_info.notification_state;
  return notification_state;
}

void Directory::SetNotificationState(const std::string& notification_state) {
  ScopedKernelLock lock(this);
  SetNotificationStateUnsafe(notification_state);
}

string Directory::cache_guid() const {
  // No need to lock since nothing ever writes to it after load.
  return kernel_->cache_guid;
}

NigoriHandler* Directory::GetNigoriHandler() {
  return nigori_handler_;
}

Cryptographer* Directory::GetCryptographer(const BaseTransaction* trans) {
  DCHECK_EQ(this, trans->directory());
  return cryptographer_;
}

void Directory::GetAllMetaHandles(BaseTransaction* trans,
                                  MetahandleSet* result) {
  result->clear();
  ScopedKernelLock lock(this);
  MetahandlesIndex::iterator i;
  for (i = kernel_->metahandles_index->begin();
       i != kernel_->metahandles_index->end();
       ++i) {
    result->insert((*i)->ref(META_HANDLE));
  }
}

void Directory::GetAllEntryKernels(BaseTransaction* trans,
                                   std::vector<const EntryKernel*>* result) {
  result->clear();
  ScopedKernelLock lock(this);
  result->insert(result->end(),
                 kernel_->metahandles_index->begin(),
                 kernel_->metahandles_index->end());
}

void Directory::GetUnsyncedMetaHandles(BaseTransaction* trans,
                                       UnsyncedMetaHandles* result) {
  result->clear();
  ScopedKernelLock lock(this);
  copy(kernel_->unsynced_metahandles->begin(),
       kernel_->unsynced_metahandles->end(), back_inserter(*result));
}

int64 Directory::unsynced_entity_count() const {
  ScopedKernelLock lock(this);
  return kernel_->unsynced_metahandles->size();
}

FullModelTypeSet Directory::GetServerTypesWithUnappliedUpdates(
    BaseTransaction* trans) const {
  FullModelTypeSet server_types;
  ScopedKernelLock lock(this);
  for (int i = UNSPECIFIED; i < MODEL_TYPE_COUNT; ++i) {
    const ModelType type = ModelTypeFromInt(i);
    if (!kernel_->unapplied_update_metahandles[type].empty()) {
      server_types.Put(type);
    }
  }
  return server_types;
}

void Directory::GetUnappliedUpdateMetaHandles(
    BaseTransaction* trans,
    FullModelTypeSet server_types,
    std::vector<int64>* result) {
  result->clear();
  ScopedKernelLock lock(this);
  for (int i = UNSPECIFIED; i < MODEL_TYPE_COUNT; ++i) {
    const ModelType type = ModelTypeFromInt(i);
    if (server_types.Has(type)) {
      std::copy(kernel_->unapplied_update_metahandles[type].begin(),
                kernel_->unapplied_update_metahandles[type].end(),
                back_inserter(*result));
    }
  }
}

bool Directory::CheckInvariantsOnTransactionClose(
    syncable::BaseTransaction* trans,
    const EntryKernelMutationMap& mutations) {
  // NOTE: The trans may be in the process of being destructed.  Be careful if
  // you wish to call any of its virtual methods.
  MetahandleSet handles;

  switch (invariant_check_level_) {
  case FULL_DB_VERIFICATION:
    GetAllMetaHandles(trans, &handles);
    break;
  case VERIFY_CHANGES:
    for (EntryKernelMutationMap::const_iterator i = mutations.begin();
         i != mutations.end(); ++i) {
      handles.insert(i->first);
    }
    break;
  case OFF:
    break;
  }

  return CheckTreeInvariants(trans, handles);
}

bool Directory::FullyCheckTreeInvariants(syncable::BaseTransaction* trans) {
  MetahandleSet handles;
  GetAllMetaHandles(trans, &handles);
  return CheckTreeInvariants(trans, handles);
}

bool Directory::CheckTreeInvariants(syncable::BaseTransaction* trans,
                                    const MetahandleSet& handles) {
  MetahandleSet::const_iterator i;
  for (i = handles.begin() ; i != handles.end() ; ++i) {
    int64 metahandle = *i;
    Entry e(trans, GET_BY_HANDLE, metahandle);
    if (!SyncAssert(e.good(), FROM_HERE, "Entry is bad", trans))
      return false;
    syncable::Id id = e.Get(ID);
    syncable::Id parentid = e.Get(PARENT_ID);

    if (id.IsRoot()) {
      if (!SyncAssert(e.Get(IS_DIR), FROM_HERE,
                      "Entry should be a directory",
                      trans))
        return false;
      if (!SyncAssert(parentid.IsRoot(), FROM_HERE,
                      "Entry should be root",
                      trans))
         return false;
      if (!SyncAssert(!e.Get(IS_UNSYNCED), FROM_HERE,
                      "Entry should be sycned",
                      trans))
         return false;
      continue;
    }

    if (!e.Get(IS_DEL)) {
      if (!SyncAssert(id != parentid, FROM_HERE,
                      "Id should be different from parent id.",
                      trans))
         return false;
      if (!SyncAssert(!e.Get(NON_UNIQUE_NAME).empty(), FROM_HERE,
                      "Non unique name should not be empty.",
                      trans))
        return false;
      int safety_count = handles.size() + 1;
      while (!parentid.IsRoot()) {
        Entry parent(trans, GET_BY_ID, parentid);
        if (!SyncAssert(parent.good(), FROM_HERE,
                        "Parent entry is not valid.",
                        trans))
          return false;
        if (handles.end() == handles.find(parent.Get(META_HANDLE)))
            break; // Skip further checking if parent was unmodified.
        if (!SyncAssert(parent.Get(IS_DIR), FROM_HERE,
                        "Parent should be a directory",
                        trans))
          return false;
        if (!SyncAssert(!parent.Get(IS_DEL), FROM_HERE,
                        "Parent should not have been marked for deletion.",
                        trans))
          return false;
        if (!SyncAssert(handles.end() != handles.find(parent.Get(META_HANDLE)),
                        FROM_HERE,
                        "Parent should be in the index.",
                        trans))
          return false;
        parentid = parent.Get(PARENT_ID);
        if (!SyncAssert(--safety_count > 0, FROM_HERE,
                        "Count should be greater than zero.",
                        trans))
          return false;
      }
    }
    int64 base_version = e.Get(BASE_VERSION);
    int64 server_version = e.Get(SERVER_VERSION);
    bool using_unique_client_tag = !e.Get(UNIQUE_CLIENT_TAG).empty();
    if (CHANGES_VERSION == base_version || 0 == base_version) {
      if (e.Get(IS_UNAPPLIED_UPDATE)) {
        // Must be a new item, or a de-duplicated unique client tag
        // that was created both locally and remotely.
        if (!using_unique_client_tag) {
          if (!SyncAssert(e.Get(IS_DEL), FROM_HERE,
                          "The entry should not have been deleted.",
                          trans))
            return false;
        }
        // It came from the server, so it must have a server ID.
        if (!SyncAssert(id.ServerKnows(), FROM_HERE,
                        "The id should be from a server.",
                        trans))
          return false;
      } else {
        if (e.Get(IS_DIR)) {
          // TODO(chron): Implement this mode if clients ever need it.
          // For now, you can't combine a client tag and a directory.
          if (!SyncAssert(!using_unique_client_tag, FROM_HERE,
                          "Directory cannot have a client tag.",
                          trans))
            return false;
        }
        // Should be an uncomitted item, or a successfully deleted one.
        if (!e.Get(IS_DEL)) {
          if (!SyncAssert(e.Get(IS_UNSYNCED), FROM_HERE,
                          "The item should be unsynced.",
                          trans))
            return false;
        }
        // If the next check failed, it would imply that an item exists
        // on the server, isn't waiting for application locally, but either
        // is an unsynced create or a sucessful delete in the local copy.
        // Either way, that's a mismatch.
        if (!SyncAssert(0 == server_version, FROM_HERE,
                        "Server version should be zero.",
                        trans))
          return false;
        // Items that aren't using the unique client tag should have a zero
        // base version only if they have a local ID.  Items with unique client
        // tags are allowed to use the zero base version for undeletion and
        // de-duplication; the unique client tag trumps the server ID.
        if (!using_unique_client_tag) {
          if (!SyncAssert(!id.ServerKnows(), FROM_HERE,
                          "Should be a client only id.",
                          trans))
            return false;
        }
      }
    } else {
      if (!SyncAssert(id.ServerKnows(),
                      FROM_HERE,
                      "Should be a server id.",
                      trans))
        return false;
    }
    // Server-unknown items that are locally deleted should not be sent up to
    // the server.  They must be !IS_UNSYNCED.
    if (!SyncAssert(!(!id.ServerKnows() &&
                      e.Get(IS_DEL) &&
                      e.Get(IS_UNSYNCED)), FROM_HERE,
                    "Locally deleted item must not be unsynced.",
                    trans)) {
      return false;
    }
  }
  return true;
}

void Directory::SetInvariantCheckLevel(InvariantCheckLevel check_level) {
  invariant_check_level_ = check_level;
}

bool Directory::UnlinkEntryFromOrder(EntryKernel* entry,
                                     WriteTransaction* trans,
                                     ScopedKernelLock* lock,
                                     UnlinkReason unlink_reason) {
  if (!SyncAssert(!trans || this == trans->directory(),
                  FROM_HERE,
                  "Transaction not pointing to the right directory",
                  trans))
    return false;
  Id old_previous = entry->ref(PREV_ID);
  Id old_next = entry->ref(NEXT_ID);

  entry->put(NEXT_ID, entry->ref(ID));
  entry->put(PREV_ID, entry->ref(ID));
  entry->mark_dirty(kernel_->dirty_metahandles);

  if (!old_previous.IsRoot()) {
    if (old_previous == old_next) {
      // Note previous == next doesn't imply previous == next == Get(ID). We
      // could have prev==next=="c-XX" and Get(ID)=="sX..." if an item was added
      // and deleted before receiving the server ID in the commit response.
      if (!SyncAssert(
               (old_next == entry->ref(ID)) || !old_next.ServerKnows(),
               FROM_HERE,
               "Encounteered inconsistent entry while deleting",
               trans)) {
        return false;
      }
      return true;  // Done if we were already self-looped (hence unlinked).
    }
    EntryKernel* previous_entry = GetEntryById(old_previous, lock);
    ModelType type = GetModelTypeFromSpecifics(entry->ref(SPECIFICS));
    // TODO(tim): Multiple asserts here for bug 101039 investigation.
    if (type == AUTOFILL) {
      if (!SyncAssert(previous_entry != NULL,
                      FROM_HERE,
                      "Could not find previous autofill entry",
                      trans)) {
        return false;
      }
    } else {
      if (!SyncAssert(previous_entry != NULL,
                      FROM_HERE,
                      "Could not find previous entry",
                      trans)) {
        return false;
      }
    }
    if (unlink_reason == NODE_MANIPULATION)
      trans->SaveOriginal(previous_entry);
    previous_entry->put(NEXT_ID, old_next);
    previous_entry->mark_dirty(kernel_->dirty_metahandles);
  }

  if (!old_next.IsRoot()) {
    EntryKernel* next_entry = GetEntryById(old_next, lock);
    if (!SyncAssert(next_entry != NULL,
                    FROM_HERE,
                    "Could not find next entry",
                    trans)) {
      return false;
    }
    if (unlink_reason == NODE_MANIPULATION)
      trans->SaveOriginal(next_entry);
    next_entry->put(PREV_ID, old_previous);
    next_entry->mark_dirty(kernel_->dirty_metahandles);
  }
  return true;
}

int64 Directory::NextMetahandle() {
  ScopedKernelLock lock(this);
  int64 metahandle = (kernel_->next_metahandle)++;
  return metahandle;
}

// Always returns a client ID that is the string representation of a negative
// number.
Id Directory::NextId() {
  int64 result;
  {
    ScopedKernelLock lock(this);
    result = (kernel_->persisted_info.next_id)--;
    kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  }
  DCHECK_LT(result, 0);
  return Id::CreateFromClientString(base::Int64ToString(result));
}

bool Directory::HasChildren(BaseTransaction* trans, const Id& id) {
  ScopedKernelLock lock(this);
  return (GetPossibleFirstChild(lock, id) != NULL);
}

bool Directory::GetFirstChildId(BaseTransaction* trans,
                                const Id& parent_id,
                                Id* first_child_id) {
  ScopedKernelLock lock(this);
  EntryKernel* entry = GetPossibleFirstChild(lock, parent_id);
  if (!entry) {
    *first_child_id = Id();
    return true;
  }

  // Walk to the front of the list; the server position ordering
  // is commonly identical to the linked-list ordering, but pending
  // unsynced or unapplied items may diverge.
  while (!entry->ref(PREV_ID).IsRoot()) {
    entry = GetEntryById(entry->ref(PREV_ID), &lock);
    if (!entry) {
      *first_child_id = Id();
      return false;
    }
  }
  *first_child_id = entry->ref(ID);
  return true;
}

bool Directory::GetLastChildIdForTest(
    BaseTransaction* trans, const Id& parent_id, Id* last_child_id) {
  ScopedKernelLock lock(this);
  EntryKernel* entry = GetPossibleFirstChild(lock, parent_id);
  if (!entry) {
    *last_child_id = Id();
    return true;
  }

  // Walk to the back of the list; the server position ordering
  // is commonly identical to the linked-list ordering, but pending
  // unsynced or unapplied items may diverge.
  while (!entry->ref(NEXT_ID).IsRoot()) {
    entry = GetEntryById(entry->ref(NEXT_ID), &lock);
    if (!entry) {
      *last_child_id = Id();
      return false;
    }
  }

  *last_child_id = entry->ref(ID);
  return true;
}

Id Directory::ComputePrevIdFromServerPosition(
    const EntryKernel* entry,
    const syncable::Id& parent_id) {
  ScopedKernelLock lock(this);

  // Find the natural insertion point in the parent_id_child_index, and
  // work back from there, filtering out ineligible candidates.
  ParentIdChildIndex::iterator sibling = LocateInParentChildIndex(lock,
      parent_id, entry->ref(SERVER_POSITION_IN_PARENT), entry->ref(ID));
  ParentIdChildIndex::iterator first_sibling =
      GetParentChildIndexLowerBound(lock, parent_id);

  while (sibling != first_sibling) {
    --sibling;
    EntryKernel* candidate = *sibling;

    // The item itself should never be in the range under consideration.
    DCHECK_NE(candidate->ref(META_HANDLE), entry->ref(META_HANDLE));

    // Ignore unapplied updates -- they might not even be server-siblings.
    if (candidate->ref(IS_UNAPPLIED_UPDATE))
      continue;

    // We can't trust the SERVER_ fields of unsynced items, but they are
    // potentially legitimate local predecessors.  In the case where
    // |update_item| and an unsynced item wind up in the same insertion
    // position, we need to choose how to order them.  The following check puts
    // the unapplied update first; removing it would put the unsynced item(s)
    // first.
    if (candidate->ref(IS_UNSYNCED))
      continue;

    // Skip over self-looped items, which are not valid predecessors.  This
    // shouldn't happen in practice, but is worth defending against.
    if (candidate->ref(PREV_ID) == candidate->ref(NEXT_ID) &&
        !candidate->ref(PREV_ID).IsRoot()) {
      NOTREACHED();
      continue;
    }
    return candidate->ref(ID);
  }
  // This item will be the first in the sibling order.
  return Id();
}

Directory::ParentIdChildIndex::iterator Directory::LocateInParentChildIndex(
    const ScopedKernelLock& lock,
    const Id& parent_id,
    int64 position_in_parent,
    const Id& item_id_for_tiebreaking) {
  kernel_->needle.put(PARENT_ID, parent_id);
  kernel_->needle.put(SERVER_POSITION_IN_PARENT, position_in_parent);
  kernel_->needle.put(ID, item_id_for_tiebreaking);
  return kernel_->parent_id_child_index->lower_bound(&kernel_->needle);
}

Directory::ParentIdChildIndex::iterator
Directory::GetParentChildIndexLowerBound(const ScopedKernelLock& lock,
                                         const Id& parent_id) {
  // Peg the parent ID, and use the least values for the remaining
  // index variables.
  return LocateInParentChildIndex(lock, parent_id,
      std::numeric_limits<int64>::min(),
      Id::GetLeastIdForLexicographicComparison());
}

Directory::ParentIdChildIndex::iterator
Directory::GetParentChildIndexUpperBound(const ScopedKernelLock& lock,
                                         const Id& parent_id) {
  // The upper bound of |parent_id|'s range is the lower
  // bound of |++parent_id|'s range.
  return GetParentChildIndexLowerBound(lock,
      parent_id.GetLexicographicSuccessor());
}

void Directory::AppendChildHandles(const ScopedKernelLock& lock,
                                   const Id& parent_id,
                                   Directory::ChildHandles* result) {
  typedef ParentIdChildIndex::iterator iterator;
  CHECK(result);
  for (iterator i = GetParentChildIndexLowerBound(lock, parent_id),
           end = GetParentChildIndexUpperBound(lock, parent_id);
       i != end; ++i) {
    DCHECK_EQ(parent_id, (*i)->ref(PARENT_ID));
    result->push_back((*i)->ref(META_HANDLE));
  }
}

EntryKernel* Directory::GetPossibleFirstChild(
    const ScopedKernelLock& lock, const Id& parent_id) {
  // We can use the server positional ordering as a hint because it's generally
  // in sync with the local (linked-list) positional ordering, and we have an
  // index on it.
  ParentIdChildIndex::iterator candidate =
      GetParentChildIndexLowerBound(lock, parent_id);
  ParentIdChildIndex::iterator end_range =
      GetParentChildIndexUpperBound(lock, parent_id);
  for (; candidate != end_range; ++candidate) {
    EntryKernel* entry = *candidate;
    // Filter out self-looped items, which are temporarily not in the child
    // ordering.
    if (entry->ref(PREV_ID).IsRoot() ||
        entry->ref(PREV_ID) != entry->ref(NEXT_ID)) {
      return entry;
    }
  }
  // There were no children in the linked list.
  return NULL;
}

ScopedKernelLock::ScopedKernelLock(const Directory* dir)
  :  scoped_lock_(dir->kernel_->mutex), dir_(const_cast<Directory*>(dir)) {
}

}  // namespace syncable
}  // namespace syncer
