// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/directory.h"

#include <iterator>

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "sync/internal_api/public/base/attachment_id_proto.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/on_disk_directory_backing_store.h"
#include "sync/syncable/scoped_kernel_lock.h"
#include "sync/syncable/scoped_parent_child_index_updater.h"
#include "sync/syncable/syncable-inl.h"
#include "sync/syncable/syncable_base_transaction.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"

using std::string;

namespace syncer {
namespace syncable {

// static
const base::FilePath::CharType Directory::kSyncDatabaseFilename[] =
    FILE_PATH_LITERAL("SyncData.sqlite3");

Directory::PersistedKernelInfo::PersistedKernelInfo()
    : next_id(0) {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelTypeSet::Iterator iter = protocol_types.First(); iter.Good();
       iter.Inc()) {
    ResetDownloadProgress(iter.Get());
    transaction_version[iter.Get()] = 0;
  }
}

Directory::PersistedKernelInfo::~PersistedKernelInfo() {}

void Directory::PersistedKernelInfo::ResetDownloadProgress(
    ModelType model_type) {
  // Clear everything except the data type id field.
  download_progress[model_type].Clear();
  download_progress[model_type].set_data_type_id(
      GetSpecificsFieldNumberFromModelType(model_type));

  // Explicitly set an empty token field to denote no progress.
  download_progress[model_type].set_token("");
}

bool Directory::PersistedKernelInfo::HasEmptyDownloadProgress(
    ModelType model_type) {
  const sync_pb::DataTypeProgressMarker& progress_marker =
      download_progress[model_type];
  return progress_marker.token().empty();
}

Directory::SaveChangesSnapshot::SaveChangesSnapshot()
    : kernel_info_status(KERNEL_SHARE_INFO_INVALID) {
}

Directory::SaveChangesSnapshot::~SaveChangesSnapshot() {
  STLDeleteElements(&dirty_metas);
  STLDeleteElements(&delete_journals);
}

Directory::Kernel::Kernel(
    const std::string& name,
    const KernelLoadInfo& info, DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer)
    : next_write_transaction_id(0),
      name(name),
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
  STLDeleteContainerPairSecondPointers(metahandles_map.begin(),
                                       metahandles_map.end());
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

void Directory::InitializeIndices(MetahandlesMap* handles_map) {
  ScopedKernelLock lock(this);
  kernel_->metahandles_map.swap(*handles_map);
  for (MetahandlesMap::const_iterator it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    EntryKernel* entry = it->second;
    if (ParentChildIndex::ShouldInclude(entry))
      kernel_->parent_child_index.Insert(entry);
    const int64 metahandle = entry->ref(META_HANDLE);
    if (entry->ref(IS_UNSYNCED))
      kernel_->unsynced_metahandles.insert(metahandle);
    if (entry->ref(IS_UNAPPLIED_UPDATE)) {
      const ModelType type = entry->GetServerModelType();
      kernel_->unapplied_update_metahandles[type].insert(metahandle);
    }
    if (!entry->ref(UNIQUE_SERVER_TAG).empty()) {
      DCHECK(kernel_->server_tags_map.find(entry->ref(UNIQUE_SERVER_TAG)) ==
             kernel_->server_tags_map.end())
          << "Unexpected duplicate use of client tag";
      kernel_->server_tags_map[entry->ref(UNIQUE_SERVER_TAG)] = entry;
    }
    if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
      DCHECK(kernel_->server_tags_map.find(entry->ref(UNIQUE_SERVER_TAG)) ==
             kernel_->server_tags_map.end())
          << "Unexpected duplicate use of server tag";
      kernel_->client_tags_map[entry->ref(UNIQUE_CLIENT_TAG)] = entry;
    }
    DCHECK(kernel_->ids_map.find(entry->ref(ID).value()) ==
           kernel_->ids_map.end()) << "Unexpected duplicate use of ID";
    kernel_->ids_map[entry->ref(ID).value()] = entry;
    DCHECK(!entry->is_dirty());
    AddToAttachmentIndex(metahandle, entry->ref(ATTACHMENT_METADATA), lock);
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
  Directory::MetahandlesMap tmp_handles_map;

  // Avoids mem leaks on failure.  Harmlessly deletes the empty hash map after
  // the swap in the success case.
  STLValueDeleter<Directory::MetahandlesMap> deleter(&tmp_handles_map);

  JournalIndex delete_journals;

  DirOpenResult result =
      store_->Load(&tmp_handles_map, &delete_journals, &info);
  if (OPENED != result)
    return result;

  kernel_ = new Kernel(name, info, delegate, transaction_observer);
  delete_journal_.reset(new DeleteJournal(&delete_journals));
  InitializeIndices(&tmp_handles_map);

  // Write back the share info to reserve some space in 'next_id'.  This will
  // prevent local ID reuse in the case of an early crash.  See the comments in
  // TakeSnapshotForSaveChanges() or crbug.com/142987 for more information.
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  if (!SaveChanges())
    return FAILED_INITIAL_WRITE;

  return OPENED;
}

DeleteJournal* Directory::delete_journal() {
  DCHECK(delete_journal_.get());
  return delete_journal_.get();
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
  IdsMap::iterator id_found = kernel_->ids_map.find(id.value());
  if (id_found != kernel_->ids_map.end()) {
    return id_found->second;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByClientTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);

  TagsMap::iterator it = kernel_->client_tags_map.find(tag);
  if (it != kernel_->client_tags_map.end()) {
    return it->second;
  }
  return NULL;
}

EntryKernel* Directory::GetEntryByServerTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  TagsMap::iterator it = kernel_->server_tags_map.find(tag);
  if (it != kernel_->server_tags_map.end()) {
    return it->second;
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
  MetahandlesMap::iterator found =
      kernel_->metahandles_map.find(metahandle);
  if (found != kernel_->metahandles_map.end()) {
    // Found it in memory.  Easy.
    return found->second;
  }
  return NULL;
}

bool Directory::GetChildHandlesById(
    BaseTransaction* trans, const Id& parent_id,
    Directory::Metahandles* result) {
  if (!SyncAssert(this == trans->directory(), FROM_HERE,
                  "Directories don't match", trans))
    return false;
  result->clear();

  ScopedKernelLock lock(this);
  AppendChildHandles(lock, parent_id, result);
  return true;
}

int Directory::GetTotalNodeCount(
    BaseTransaction* trans,
    EntryKernel* kernel) const {
  if (!SyncAssert(this == trans->directory(), FROM_HERE,
                  "Directories don't match", trans))
    return false;

  int count = 1;
  std::deque<const OrderedChildSet*> child_sets;

  GetChildSetForKernel(trans, kernel, &child_sets);
  while (!child_sets.empty()) {
    const OrderedChildSet* set = child_sets.front();
    child_sets.pop_front();
    for (OrderedChildSet::const_iterator it = set->begin();
         it != set->end(); ++it) {
      count++;
      GetChildSetForKernel(trans, *it, &child_sets);
    }
  }

  return count;
}

void Directory::GetChildSetForKernel(
    BaseTransaction* trans,
    EntryKernel* kernel,
    std::deque<const OrderedChildSet*>* child_sets) const {
  if (!kernel->ref(IS_DIR))
    return;  // Not a directory => no children.

  const OrderedChildSet* descendants =
      kernel_->parent_child_index.GetChildren(kernel->ref(ID));
  if (!descendants)
    return;  // This directory has no children.

  // Add our children to the list of items to be traversed.
  child_sets->push_back(descendants);
}

int Directory::GetPositionIndex(
    BaseTransaction* trans,
    EntryKernel* kernel) const {
  const OrderedChildSet* siblings =
      kernel_->parent_child_index.GetChildren(kernel->ref(PARENT_ID));

  OrderedChildSet::const_iterator it = siblings->find(kernel);
  return std::distance(siblings->begin(), it);
}

bool Directory::InsertEntry(BaseWriteTransaction* trans, EntryKernel* entry) {
  ScopedKernelLock lock(this);
  return InsertEntry(trans, entry, &lock);
}

bool Directory::InsertEntry(BaseWriteTransaction* trans,
                            EntryKernel* entry,
                            ScopedKernelLock* lock) {
  DCHECK(NULL != lock);
  if (!SyncAssert(NULL != entry, FROM_HERE, "Entry is null", trans))
    return false;

  static const char error[] = "Entry already in memory index.";

  if (!SyncAssert(
          kernel_->metahandles_map.insert(
              std::make_pair(entry->ref(META_HANDLE), entry)).second,
          FROM_HERE,
          error,
          trans)) {
    return false;
  }
  if (!SyncAssert(
          kernel_->ids_map.insert(
              std::make_pair(entry->ref(ID).value(), entry)).second,
          FROM_HERE,
          error,
          trans)) {
    return false;
  }
  if (ParentChildIndex::ShouldInclude(entry)) {
    if (!SyncAssert(kernel_->parent_child_index.Insert(entry),
                    FROM_HERE,
                    error,
                    trans)) {
      return false;
    }
  }
  AddToAttachmentIndex(
      entry->ref(META_HANDLE), entry->ref(ATTACHMENT_METADATA), *lock);

  // Should NEVER be created with a client tag or server tag.
  if (!SyncAssert(entry->ref(UNIQUE_SERVER_TAG).empty(), FROM_HERE,
                  "Server tag should be empty", trans)) {
    return false;
  }
  if (!SyncAssert(entry->ref(UNIQUE_CLIENT_TAG).empty(), FROM_HERE,
                  "Client tag should be empty", trans))
    return false;

  return true;
}

bool Directory::ReindexId(BaseWriteTransaction* trans,
                          EntryKernel* const entry,
                          const Id& new_id) {
  ScopedKernelLock lock(this);
  if (NULL != GetEntryById(new_id, &lock))
    return false;

  {
    // Update the indices that depend on the ID field.
    ScopedParentChildIndexUpdater updater_b(lock, entry,
        &kernel_->parent_child_index);
    size_t num_erased = kernel_->ids_map.erase(entry->ref(ID).value());
    DCHECK_EQ(1U, num_erased);
    entry->put(ID, new_id);
    kernel_->ids_map[entry->ref(ID).value()] = entry;
  }
  return true;
}

bool Directory::ReindexParentId(BaseWriteTransaction* trans,
                                EntryKernel* const entry,
                                const Id& new_parent_id) {
  ScopedKernelLock lock(this);

  {
    // Update the indices that depend on the PARENT_ID field.
    ScopedParentChildIndexUpdater index_updater(lock, entry,
        &kernel_->parent_child_index);
    entry->put(PARENT_ID, new_parent_id);
  }
  return true;
}

void Directory::RemoveFromAttachmentIndex(
    const int64 metahandle,
    const sync_pb::AttachmentMetadata& attachment_metadata,
    const ScopedKernelLock& lock) {
  for (int i = 0; i < attachment_metadata.record_size(); ++i) {
    AttachmentIdUniqueId unique_id =
        attachment_metadata.record(i).id().unique_id();
    IndexByAttachmentId::iterator iter =
        kernel_->index_by_attachment_id.find(unique_id);
    if (iter != kernel_->index_by_attachment_id.end()) {
      iter->second.erase(metahandle);
      if (iter->second.empty()) {
        kernel_->index_by_attachment_id.erase(iter);
      }
    }
  }
}

void Directory::AddToAttachmentIndex(
    const int64 metahandle,
    const sync_pb::AttachmentMetadata& attachment_metadata,
    const ScopedKernelLock& lock) {
  for (int i = 0; i < attachment_metadata.record_size(); ++i) {
    AttachmentIdUniqueId unique_id =
        attachment_metadata.record(i).id().unique_id();
    IndexByAttachmentId::iterator iter =
        kernel_->index_by_attachment_id.find(unique_id);
    if (iter == kernel_->index_by_attachment_id.end()) {
      iter = kernel_->index_by_attachment_id.insert(std::make_pair(
                                                        unique_id,
                                                        MetahandleSet())).first;
    }
    iter->second.insert(metahandle);
  }
}

void Directory::UpdateAttachmentIndex(
    const int64 metahandle,
    const sync_pb::AttachmentMetadata& old_metadata,
    const sync_pb::AttachmentMetadata& new_metadata) {
  ScopedKernelLock lock(this);
  RemoveFromAttachmentIndex(metahandle, old_metadata, lock);
  AddToAttachmentIndex(metahandle, new_metadata, lock);
}

void Directory::GetMetahandlesByAttachmentId(
    BaseTransaction* trans,
    const sync_pb::AttachmentIdProto& attachment_id_proto,
    Metahandles* result) {
  DCHECK(result);
  result->clear();
  ScopedKernelLock lock(this);
  IndexByAttachmentId::const_iterator index_iter =
      kernel_->index_by_attachment_id.find(attachment_id_proto.unique_id());
  if (index_iter == kernel_->index_by_attachment_id.end())
    return;
  const MetahandleSet& metahandle_set = index_iter->second;
  std::copy(
      metahandle_set.begin(), metahandle_set.end(), back_inserter(*result));
}

bool Directory::unrecoverable_error_set(const BaseTransaction* trans) const {
  DCHECK(trans != NULL);
  return unrecoverable_error_set_;
}

void Directory::ClearDirtyMetahandles() {
  kernel_->transaction_mutex.AssertAcquired();
  kernel_->dirty_metahandles.clear();
}

bool Directory::SafeToPurgeFromMemory(WriteTransaction* trans,
                                      const EntryKernel* const entry) const {
  bool safe = entry->ref(IS_DEL) && !entry->is_dirty() &&
      !entry->ref(SYNCING) && !entry->ref(IS_UNAPPLIED_UPDATE) &&
      !entry->ref(IS_UNSYNCED);

  if (safe) {
    int64 handle = entry->ref(META_HANDLE);
    const ModelType type = entry->GetServerModelType();
    if (!SyncAssert(kernel_->dirty_metahandles.count(handle) == 0U,
                    FROM_HERE,
                    "Dirty metahandles should be empty", trans))
      return false;
    // TODO(tim): Bug 49278.
    if (!SyncAssert(!kernel_->unsynced_metahandles.count(handle),
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
  for (MetahandleSet::const_iterator i = kernel_->dirty_metahandles.begin();
       i != kernel_->dirty_metahandles.end(); ++i) {
    EntryKernel* entry = GetEntryByHandle(*i, &lock);
    if (!entry)
      continue;
    // Skip over false positives; it happens relatively infrequently.
    if (!entry->is_dirty())
      continue;
    snapshot->dirty_metas.insert(snapshot->dirty_metas.end(),
                                 new EntryKernel(*entry));
    DCHECK_EQ(1U, kernel_->dirty_metahandles.count(*i));
    // We don't bother removing from the index here as we blow the entire thing
    // in a moment, and it unnecessarily complicates iteration.
    entry->clear_dirty(NULL);
  }
  ClearDirtyMetahandles();

  // Set purged handles.
  DCHECK(snapshot->metahandles_to_purge.empty());
  snapshot->metahandles_to_purge.swap(kernel_->metahandles_to_purge);

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

  delete_journal_->TakeSnapshotAndClear(
      &trans, &snapshot->delete_journals, &snapshot->delete_journals_to_purge);
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
    MetahandlesMap::iterator found =
        kernel_->metahandles_map.find((*i)->ref(META_HANDLE));
    EntryKernel* entry = (found == kernel_->metahandles_map.end() ?
                          NULL : found->second);
    if (entry && SafeToPurgeFromMemory(&trans, entry)) {
      // We now drop deleted metahandles that are up to date on both the client
      // and the server.
      size_t num_erased = 0;
      num_erased = kernel_->metahandles_map.erase(entry->ref(META_HANDLE));
      DCHECK_EQ(1u, num_erased);
      num_erased = kernel_->ids_map.erase(entry->ref(ID).value());
      DCHECK_EQ(1u, num_erased);
      if (!entry->ref(UNIQUE_SERVER_TAG).empty()) {
        num_erased =
            kernel_->server_tags_map.erase(entry->ref(UNIQUE_SERVER_TAG));
        DCHECK_EQ(1u, num_erased);
      }
      if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
        num_erased =
            kernel_->client_tags_map.erase(entry->ref(UNIQUE_CLIENT_TAG));
        DCHECK_EQ(1u, num_erased);
      }
      if (!SyncAssert(!kernel_->parent_child_index.Contains(entry),
                      FROM_HERE,
                      "Deleted entry still present",
                      (&trans)))
        return false;
      RemoveFromAttachmentIndex(
          entry->ref(META_HANDLE), entry->ref(ATTACHMENT_METADATA), lock);

      delete entry;
    }
    if (trans.unrecoverable_error_set())
      return false;
  }
  return true;
}

void Directory::UnapplyEntry(EntryKernel* entry) {
  int64 handle = entry->ref(META_HANDLE);
  ModelType server_type = GetModelTypeFromSpecifics(
      entry->ref(SERVER_SPECIFICS));

  // Clear enough so that on the next sync cycle all local data will
  // be overwritten.
  // Note: do not modify the root node in order to preserve the
  // initial sync ended bit for this type (else on the next restart
  // this type will be treated as disabled and therefore fully purged).
  if (IsRealDataType(server_type) &&
      ModelTypeToRootTag(server_type) == entry->ref(UNIQUE_SERVER_TAG)) {
    return;
  }

  // Set the unapplied bit if this item has server data.
  if (IsRealDataType(server_type) && !entry->ref(IS_UNAPPLIED_UPDATE)) {
    entry->put(IS_UNAPPLIED_UPDATE, true);
    kernel_->unapplied_update_metahandles[server_type].insert(handle);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // Unset the unsynced bit.
  if (entry->ref(IS_UNSYNCED)) {
    kernel_->unsynced_metahandles.erase(handle);
    entry->put(IS_UNSYNCED, false);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // Mark the item as locally deleted. No deleted items are allowed in the
  // parent child index.
  if (!entry->ref(IS_DEL)) {
    kernel_->parent_child_index.Remove(entry);
    entry->put(IS_DEL, true);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // Set the version to the "newly created" version.
  if (entry->ref(BASE_VERSION) != CHANGES_VERSION) {
    entry->put(BASE_VERSION, CHANGES_VERSION);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // At this point locally created items that aren't synced will become locally
  // deleted items, and purged on the next snapshot. All other items will match
  // the state they would have had if they were just created via a server
  // update. See MutableEntry::MutableEntry(.., CreateNewUpdateItem, ..).
}

void Directory::DeleteEntry(bool save_to_journal,
                            EntryKernel* entry,
                            EntryKernelSet* entries_to_journal,
                            const ScopedKernelLock& lock) {
  int64 handle = entry->ref(META_HANDLE);
  ModelType server_type = GetModelTypeFromSpecifics(
      entry->ref(SERVER_SPECIFICS));

  kernel_->metahandles_to_purge.insert(handle);

  size_t num_erased = 0;
  num_erased = kernel_->metahandles_map.erase(entry->ref(META_HANDLE));
  DCHECK_EQ(1u, num_erased);
  num_erased = kernel_->ids_map.erase(entry->ref(ID).value());
  DCHECK_EQ(1u, num_erased);
  num_erased = kernel_->unsynced_metahandles.erase(handle);
  DCHECK_EQ(entry->ref(IS_UNSYNCED), num_erased > 0);
  num_erased =
      kernel_->unapplied_update_metahandles[server_type].erase(handle);
  DCHECK_EQ(entry->ref(IS_UNAPPLIED_UPDATE), num_erased > 0);
  if (kernel_->parent_child_index.Contains(entry))
    kernel_->parent_child_index.Remove(entry);

  if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
    num_erased =
        kernel_->client_tags_map.erase(entry->ref(UNIQUE_CLIENT_TAG));
    DCHECK_EQ(1u, num_erased);
  }
  if (!entry->ref(UNIQUE_SERVER_TAG).empty()) {
    num_erased =
        kernel_->server_tags_map.erase(entry->ref(UNIQUE_SERVER_TAG));
    DCHECK_EQ(1u, num_erased);
  }
  RemoveFromAttachmentIndex(handle, entry->ref(ATTACHMENT_METADATA), lock);

  if (save_to_journal) {
    entries_to_journal->insert(entry);
  } else {
    delete entry;
  }
}

bool Directory::PurgeEntriesWithTypeIn(ModelTypeSet disabled_types,
                                       ModelTypeSet types_to_journal,
                                       ModelTypeSet types_to_unapply) {
  disabled_types.RemoveAll(ProxyTypes());

  if (disabled_types.Empty())
    return true;

  {
    WriteTransaction trans(FROM_HERE, PURGE_ENTRIES, this);

    EntryKernelSet entries_to_journal;
    STLElementDeleter<EntryKernelSet> journal_deleter(&entries_to_journal);

    {
      ScopedKernelLock lock(this);

      bool found_progress = false;
      for (ModelTypeSet::Iterator iter = disabled_types.First(); iter.Good();
           iter.Inc()) {
        if (!kernel_->persisted_info.HasEmptyDownloadProgress(iter.Get()))
          found_progress = true;
      }

      // If none of the disabled types have progress markers, there's nothing to
      // purge.
      if (!found_progress)
        return true;

      // We iterate in two passes to avoid a bug in STLport (which is used in
      // the Android build).  There are some versions of that library where a
      // hash_map's iterators can be invalidated when an item is erased from the
      // hash_map.
      // See http://sourceforge.net/p/stlport/bugs/239/.

      std::set<EntryKernel*> to_purge;
      for (MetahandlesMap::iterator it = kernel_->metahandles_map.begin();
           it != kernel_->metahandles_map.end(); ++it) {
        const sync_pb::EntitySpecifics& local_specifics =
            it->second->ref(SPECIFICS);
        const sync_pb::EntitySpecifics& server_specifics =
            it->second->ref(SERVER_SPECIFICS);
        ModelType local_type = GetModelTypeFromSpecifics(local_specifics);
        ModelType server_type = GetModelTypeFromSpecifics(server_specifics);

        if ((IsRealDataType(local_type) && disabled_types.Has(local_type)) ||
            (IsRealDataType(server_type) && disabled_types.Has(server_type))) {
          to_purge.insert(it->second);
        }
      }

      for (std::set<EntryKernel*>::iterator it = to_purge.begin();
           it != to_purge.end(); ++it) {
        EntryKernel* entry = *it;

        const sync_pb::EntitySpecifics& local_specifics =
            (*it)->ref(SPECIFICS);
        const sync_pb::EntitySpecifics& server_specifics =
            (*it)->ref(SERVER_SPECIFICS);
        ModelType local_type = GetModelTypeFromSpecifics(local_specifics);
        ModelType server_type = GetModelTypeFromSpecifics(server_specifics);

        if (types_to_unapply.Has(local_type) ||
            types_to_unapply.Has(server_type)) {
          UnapplyEntry(entry);
        } else {
          bool save_to_journal =
              (types_to_journal.Has(local_type) ||
               types_to_journal.Has(server_type)) &&
              (delete_journal_->IsDeleteJournalEnabled(local_type) ||
               delete_journal_->IsDeleteJournalEnabled(server_type));
          DeleteEntry(save_to_journal, entry, &entries_to_journal, lock);
        }
      }

      delete_journal_->AddJournalBatch(&trans, entries_to_journal);

      // Ensure meta tracking for these data types reflects the purged state.
      for (ModelTypeSet::Iterator it = disabled_types.First();
           it.Good(); it.Inc()) {
        kernel_->persisted_info.transaction_version[it.Get()] = 0;

        // Don't discard progress markers or context for unapplied types.
        if (!types_to_unapply.Has(it.Get())) {
          kernel_->persisted_info.ResetDownloadProgress(it.Get());
          kernel_->persisted_info.datatype_context[it.Get()].Clear();
        }
      }

      kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
    }
  }
  return true;
}

bool Directory::ResetVersionsForType(BaseWriteTransaction* trans,
                                     ModelType type) {
  if (!ProtocolTypes().Has(type))
    return false;
  DCHECK_NE(type, BOOKMARKS) << "Only non-hierarchical types are supported";

  EntryKernel* type_root = GetEntryByServerTag(ModelTypeToRootTag(type));
  if (!type_root)
    return false;

  ScopedKernelLock lock(this);
  const Id& type_root_id = type_root->ref(ID);
  Directory::Metahandles children;
  AppendChildHandles(lock, type_root_id, &children);

  for (Metahandles::iterator it = children.begin(); it != children.end();
       ++it) {
    EntryKernel* entry = GetEntryByHandle(*it, &lock);
    if (!entry)
      continue;
    if (entry->ref(BASE_VERSION) > 1)
      entry->put(BASE_VERSION, 1);
    if (entry->ref(SERVER_VERSION) > 1)
      entry->put(SERVER_VERSION, 1);

    // Note that we do not unset IS_UNSYNCED or IS_UNAPPLIED_UPDATE in order
    // to ensure no in-transit data is lost.

    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  return true;
}

bool Directory::IsAttachmentLinked(
    const sync_pb::AttachmentIdProto& attachment_id_proto) const {
  ScopedKernelLock lock(this);
  IndexByAttachmentId::const_iterator iter =
      kernel_->index_by_attachment_id.find(attachment_id_proto.unique_id());
  if (iter != kernel_->index_by_attachment_id.end() && !iter->second.empty()) {
    return true;
  }
  return false;
}

void Directory::HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot) {
  WriteTransaction trans(FROM_HERE, HANDLE_SAVE_FAILURE, this);
  ScopedKernelLock lock(this);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;

  // Because we optimistically cleared the dirty bit on the real entries when
  // taking the snapshot, we must restore it on failure.  Not doing this could
  // cause lost data, if no other changes are made to the in-memory entries
  // that would cause the dirty bit to get set again. Setting the bit ensures
  // that SaveChanges will at least try again later.
  for (EntryKernelSet::const_iterator i = snapshot.dirty_metas.begin();
       i != snapshot.dirty_metas.end(); ++i) {
    MetahandlesMap::iterator found =
        kernel_->metahandles_map.find((*i)->ref(META_HANDLE));
    if (found != kernel_->metahandles_map.end()) {
      found->second->mark_dirty(&kernel_->dirty_metahandles);
    }
  }

  kernel_->metahandles_to_purge.insert(snapshot.metahandles_to_purge.begin(),
                                       snapshot.metahandles_to_purge.end());

  // Restore delete journals.
  delete_journal_->AddJournalBatch(&trans, snapshot.delete_journals);
  delete_journal_->PurgeDeleteJournals(&trans,
                                       snapshot.delete_journals_to_purge);
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
  return kernel_->metahandles_map.size();
}

void Directory::SetDownloadProgress(
    ModelType model_type,
    const sync_pb::DataTypeProgressMarker& new_progress) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].CopyFrom(new_progress);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

int64 Directory::GetTransactionVersion(ModelType type) const {
  kernel_->transaction_mutex.AssertAcquired();
  return kernel_->persisted_info.transaction_version[type];
}

void Directory::IncrementTransactionVersion(ModelType type) {
  kernel_->transaction_mutex.AssertAcquired();
  kernel_->persisted_info.transaction_version[type]++;
}

void Directory::GetDataTypeContext(BaseTransaction* trans,
                                   ModelType type,
                                   sync_pb::DataTypeContext* context) const {
  ScopedKernelLock lock(this);
  context->CopyFrom(kernel_->persisted_info.datatype_context[type]);
}

void Directory::SetDataTypeContext(
    BaseWriteTransaction* trans,
    ModelType type,
    const sync_pb::DataTypeContext& context) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.datatype_context[type].CopyFrom(context);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

ModelTypeSet Directory::InitialSyncEndedTypes() {
  syncable::ReadTransaction trans(FROM_HERE, this);
  ModelTypeSet protocol_types = ProtocolTypes();
  ModelTypeSet initial_sync_ended_types;
  for (ModelTypeSet::Iterator i = protocol_types.First(); i.Good(); i.Inc()) {
    if (InitialSyncEndedForType(&trans, i.Get())) {
      initial_sync_ended_types.Put(i.Get());
    }
  }
  return initial_sync_ended_types;
}

bool Directory::InitialSyncEndedForType(ModelType type) {
  syncable::ReadTransaction trans(FROM_HERE, this);
  return InitialSyncEndedForType(&trans, type);
}

bool Directory::InitialSyncEndedForType(
    BaseTransaction* trans, ModelType type) {
  // True iff the type's root node has been received and applied.
  syncable::Entry entry(trans, syncable::GET_TYPE_ROOT, type);
  return entry.good() && entry.GetBaseVersion() != CHANGES_VERSION;
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
  for (MetahandlesMap::iterator i = kernel_->metahandles_map.begin();
       i != kernel_->metahandles_map.end(); ++i) {
    result->insert(i->first);
  }
}

void Directory::GetUnsyncedMetaHandles(BaseTransaction* trans,
                                       Metahandles* result) {
  result->clear();
  ScopedKernelLock lock(this);
  copy(kernel_->unsynced_metahandles.begin(),
       kernel_->unsynced_metahandles.end(), back_inserter(*result));
}

int64 Directory::unsynced_entity_count() const {
  ScopedKernelLock lock(this);
  return kernel_->unsynced_metahandles.size();
}

bool Directory::TypeHasUnappliedUpdates(ModelType type) {
  ScopedKernelLock lock(this);
  return !kernel_->unapplied_update_metahandles[type].empty();
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

void Directory::GetMetaHandlesOfType(BaseTransaction* trans,
                                     ModelType type,
                                     std::vector<int64>* result) {
  result->clear();
  ScopedKernelLock lock(this);
  for (MetahandlesMap::iterator it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    EntryKernel* entry = it->second;
    const ModelType entry_type =
        GetModelTypeFromSpecifics(entry->ref(SPECIFICS));
    if (entry_type == type)
      result->push_back(it->first);
  }
}

void Directory::CollectMetaHandleCounts(
    std::vector<int>* num_entries_by_type,
    std::vector<int>* num_to_delete_entries_by_type) {
  syncable::ReadTransaction trans(FROM_HERE, this);
  ScopedKernelLock lock(this);

  for (MetahandlesMap::iterator it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    EntryKernel* entry = it->second;
    const ModelType type = GetModelTypeFromSpecifics(entry->ref(SPECIFICS));
    (*num_entries_by_type)[type]++;
    if (entry->ref(IS_DEL))
      (*num_to_delete_entries_by_type)[type]++;
  }
}

scoped_ptr<base::ListValue> Directory::GetNodeDetailsForType(
    BaseTransaction* trans,
    ModelType type) {
  scoped_ptr<base::ListValue> nodes(new base::ListValue());

  ScopedKernelLock lock(this);
  for (MetahandlesMap::iterator it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    if (GetModelTypeFromSpecifics(it->second->ref(SPECIFICS)) != type) {
      continue;
    }

    EntryKernel* kernel = it->second;
    scoped_ptr<base::DictionaryValue> node(
        kernel->ToValue(GetCryptographer(trans)));

    // Add the position index if appropriate.  This must be done here (and not
    // in EntryKernel) because the EntryKernel does not have access to its
    // siblings.
    if (kernel->ShouldMaintainPosition() && !kernel->ref(IS_DEL)) {
      node->SetInteger("positionIndex", GetPositionIndex(trans, kernel));
    }

    nodes->Append(node.release());
  }

  return nodes.Pass();
}

bool Directory::CheckInvariantsOnTransactionClose(
    syncable::BaseTransaction* trans,
    const MetahandleSet& modified_handles) {
  // NOTE: The trans may be in the process of being destructed.  Be careful if
  // you wish to call any of its virtual methods.
  switch (invariant_check_level_) {
    case FULL_DB_VERIFICATION: {
      MetahandleSet all_handles;
      GetAllMetaHandles(trans, &all_handles);
      return CheckTreeInvariants(trans, all_handles);
    }
    case VERIFY_CHANGES: {
      return CheckTreeInvariants(trans, modified_handles);
    }
    case OFF: {
      return true;
    }
  }
  NOTREACHED();
  return false;
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
    syncable::Id id = e.GetId();
    syncable::Id parentid = e.GetParentId();

    if (id.IsRoot()) {
      if (!SyncAssert(e.GetIsDir(), FROM_HERE,
                      "Entry should be a directory",
                      trans))
        return false;
      if (!SyncAssert(parentid.IsRoot(), FROM_HERE,
                      "Entry should be root",
                      trans))
         return false;
      if (!SyncAssert(!e.GetIsUnsynced(), FROM_HERE,
                      "Entry should be sycned",
                      trans))
         return false;
      continue;
    }

    if (!e.GetIsDel()) {
      if (!SyncAssert(id != parentid, FROM_HERE,
                      "Id should be different from parent id.",
                      trans))
         return false;
      if (!SyncAssert(!e.GetNonUniqueName().empty(), FROM_HERE,
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
        if (handles.end() == handles.find(parent.GetMetahandle()))
            break; // Skip further checking if parent was unmodified.
        if (!SyncAssert(parent.GetIsDir(), FROM_HERE,
                        "Parent should be a directory",
                        trans))
          return false;
        if (!SyncAssert(!parent.GetIsDel(), FROM_HERE,
                        "Parent should not have been marked for deletion.",
                        trans))
          return false;
        if (!SyncAssert(handles.end() != handles.find(parent.GetMetahandle()),
                        FROM_HERE,
                        "Parent should be in the index.",
                        trans))
          return false;
        parentid = parent.GetParentId();
        if (!SyncAssert(--safety_count > 0, FROM_HERE,
                        "Count should be greater than zero.",
                        trans))
          return false;
      }
    }
    int64 base_version = e.GetBaseVersion();
    int64 server_version = e.GetServerVersion();
    bool using_unique_client_tag = !e.GetUniqueClientTag().empty();
    if (CHANGES_VERSION == base_version || 0 == base_version) {
      if (e.GetIsUnappliedUpdate()) {
        // Must be a new item, or a de-duplicated unique client tag
        // that was created both locally and remotely.
        if (!using_unique_client_tag) {
          if (!SyncAssert(e.GetIsDel(), FROM_HERE,
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
        if (e.GetIsDir()) {
          // TODO(chron): Implement this mode if clients ever need it.
          // For now, you can't combine a client tag and a directory.
          if (!SyncAssert(!using_unique_client_tag, FROM_HERE,
                          "Directory cannot have a client tag.",
                          trans))
            return false;
        }
        // Should be an uncomitted item, or a successfully deleted one.
        if (!e.GetIsDel()) {
          if (!SyncAssert(e.GetIsUnsynced(), FROM_HERE,
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
    if (!SyncAssert(!(!id.ServerKnows() && e.GetIsDel() && e.GetIsUnsynced()),
                    FROM_HERE,
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
  return kernel_->parent_child_index.GetChildren(id) != NULL;
}

Id Directory::GetFirstChildId(BaseTransaction* trans,
                              const EntryKernel* parent) {
  DCHECK(parent);
  DCHECK(parent->ref(IS_DIR));

  ScopedKernelLock lock(this);
  const OrderedChildSet* children =
      kernel_->parent_child_index.GetChildren(parent->ref(ID));

  // We're expected to return root if there are no children.
  if (!children)
    return Id();

  return (*children->begin())->ref(ID);
}

syncable::Id Directory::GetPredecessorId(EntryKernel* e) {
  ScopedKernelLock lock(this);

  DCHECK(ParentChildIndex::ShouldInclude(e));
  const OrderedChildSet* children =
      kernel_->parent_child_index.GetChildren(e->ref(PARENT_ID));
  DCHECK(children && !children->empty());
  OrderedChildSet::const_iterator i = children->find(e);
  DCHECK(i != children->end());

  if (i == children->begin()) {
    return Id();
  } else {
    i--;
    return (*i)->ref(ID);
  }
}

syncable::Id Directory::GetSuccessorId(EntryKernel* e) {
  ScopedKernelLock lock(this);

  DCHECK(ParentChildIndex::ShouldInclude(e));
  const OrderedChildSet* children =
      kernel_->parent_child_index.GetChildren(e->ref(PARENT_ID));
  DCHECK(children && !children->empty());
  OrderedChildSet::const_iterator i = children->find(e);
  DCHECK(i != children->end());

  i++;
  if (i == children->end()) {
    return Id();
  } else {
    return (*i)->ref(ID);
  }
}

// TODO(rlarocque): Remove all support for placing ShouldMaintainPosition()
// items as siblings of items that do not maintain postions.  It is required
// only for tests.  See crbug.com/178282.
void Directory::PutPredecessor(EntryKernel* e, EntryKernel* predecessor) {
  DCHECK(!e->ref(IS_DEL));
  if (!e->ShouldMaintainPosition()) {
    DCHECK(!e->ref(UNIQUE_POSITION).IsValid());
    return;
  }
  std::string suffix = e->ref(UNIQUE_BOOKMARK_TAG);
  DCHECK(!suffix.empty());

  // Remove our item from the ParentChildIndex and remember to re-add it later.
  ScopedKernelLock lock(this);
  ScopedParentChildIndexUpdater updater(lock, e, &kernel_->parent_child_index);

  // Note: The ScopedParentChildIndexUpdater will update this set for us as we
  // leave this function.
  const OrderedChildSet* siblings =
      kernel_->parent_child_index.GetChildren(e->ref(PARENT_ID));

  if (!siblings) {
    // This parent currently has no other children.
    DCHECK(predecessor->ref(ID).IsRoot());
    UniquePosition pos = UniquePosition::InitialPosition(suffix);
    e->put(UNIQUE_POSITION, pos);
    return;
  }

  if (predecessor->ref(ID).IsRoot()) {
    // We have at least one sibling, and we're inserting to the left of them.
    UniquePosition successor_pos = (*siblings->begin())->ref(UNIQUE_POSITION);

    UniquePosition pos;
    if (!successor_pos.IsValid()) {
      // If all our successors are of non-positionable types, just create an
      // initial position.  We arbitrarily choose to sort invalid positions to
      // the right of the valid positions.
      //
      // We really shouldn't need to support this.  See TODO above.
      pos = UniquePosition::InitialPosition(suffix);
    } else  {
      DCHECK(!siblings->empty());
      pos = UniquePosition::Before(successor_pos, suffix);
    }

    e->put(UNIQUE_POSITION, pos);
    return;
  }

  // We can't support placing an item after an invalid position.  Fortunately,
  // the tests don't exercise this particular case.  We should not support
  // siblings with invalid positions at all.  See TODO above.
  DCHECK(predecessor->ref(UNIQUE_POSITION).IsValid());

  OrderedChildSet::const_iterator neighbour = siblings->find(predecessor);
  DCHECK(neighbour != siblings->end());

  ++neighbour;
  if (neighbour == siblings->end()) {
    // Inserting at the end of the list.
    UniquePosition pos = UniquePosition::After(
        predecessor->ref(UNIQUE_POSITION),
        suffix);
    e->put(UNIQUE_POSITION, pos);
    return;
  }

  EntryKernel* successor = *neighbour;

  // Another mixed valid and invalid position case.  This one could be supported
  // in theory, but we're trying to deprecate support for siblings with and
  // without valid positions.  See TODO above.
  DCHECK(successor->ref(UNIQUE_POSITION).IsValid());

  // Finally, the normal case: inserting between two elements.
  UniquePosition pos = UniquePosition::Between(
      predecessor->ref(UNIQUE_POSITION),
      successor->ref(UNIQUE_POSITION),
      suffix);
  e->put(UNIQUE_POSITION, pos);
  return;
}

// TODO(rlarocque): Avoid this indirection.  Just return the set.
void Directory::AppendChildHandles(const ScopedKernelLock& lock,
                                   const Id& parent_id,
                                   Directory::Metahandles* result) {
  const OrderedChildSet* children =
      kernel_->parent_child_index.GetChildren(parent_id);
  if (!children)
    return;

  for (OrderedChildSet::const_iterator i = children->begin();
       i != children->end(); ++i) {
    DCHECK_EQ(parent_id, (*i)->ref(PARENT_ID));
    result->push_back((*i)->ref(META_HANDLE));
  }
}

void Directory::UnmarkDirtyEntry(WriteTransaction* trans, Entry* entry) {
  CHECK(trans);
  entry->kernel_->clear_dirty(&kernel_->dirty_metahandles);
}

}  // namespace syncable
}  // namespace syncer
