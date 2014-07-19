// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_DIRECTORY_H_
#define SYNC_SYNCABLE_DIRECTORY_H_

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/file_util.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/report_unrecoverable_error_function.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/syncable/dir_open_result.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/metahandle_set.h"
#include "sync/syncable/parent_child_index.h"
#include "sync/syncable/syncable_delete_journal.h"

namespace syncer {

class Cryptographer;
class TestUserShare;
class UnrecoverableErrorHandler;

namespace syncable {

class BaseTransaction;
class BaseWriteTransaction;
class DirectoryChangeDelegate;
class DirectoryBackingStore;
class NigoriHandler;
class ScopedKernelLock;
class TransactionObserver;
class WriteTransaction;

enum InvariantCheckLevel {
  OFF = 0,            // No checking.
  VERIFY_CHANGES = 1, // Checks only mutated entries.  Does not check hierarchy.
  FULL_DB_VERIFICATION = 2 // Check every entry.  This can be expensive.
};

// Directory stores and manages EntryKernels.
//
// This class is tightly coupled to several other classes (see friends).
class SYNC_EXPORT Directory {
  friend class BaseTransaction;
  friend class Entry;
  friend class ModelNeutralMutableEntry;
  friend class MutableEntry;
  friend class ReadTransaction;
  friend class ScopedKernelLock;
  friend class WriteTransaction;
  friend class SyncableDirectoryTest;
  friend class syncer::TestUserShare;
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest, ManageDeleteJournals);
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest,
                           TakeSnapshotGetsAllDirtyHandlesTest);
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest,
                           TakeSnapshotGetsOnlyDirtyHandlesTest);
  FRIEND_TEST_ALL_PREFIXES(SyncableDirectoryTest,
                           TakeSnapshotGetsMetahandlesToPurge);

 public:
  typedef std::vector<int64> Metahandles;

  // Be careful when using these hash_map containers.  According to the spec,
  // inserting into them may invalidate all iterators.
  //
  // It gets worse, though.  The Anroid STL library has a bug that means it may
  // invalidate all iterators when you erase from the map, too.  That means that
  // you can't iterate while erasing.  STLDeleteElements(), std::remove_if(),
  // and other similar functions are off-limits too, until this bug is fixed.
  //
  // See http://sourceforge.net/p/stlport/bugs/239/.
  typedef base::hash_map<int64, EntryKernel*> MetahandlesMap;
  typedef base::hash_map<std::string, EntryKernel*> IdsMap;
  typedef base::hash_map<std::string, EntryKernel*> TagsMap;
  typedef std::string AttachmentIdUniqueId;
  typedef base::hash_map<AttachmentIdUniqueId, MetahandleSet>
      IndexByAttachmentId;

  static const base::FilePath::CharType kSyncDatabaseFilename[];

  // The dirty/clean state of kernel fields backed by the share_info table.
  // This is public so it can be used in SaveChangesSnapshot for persistence.
  enum KernelShareInfoStatus {
    KERNEL_SHARE_INFO_INVALID,
    KERNEL_SHARE_INFO_VALID,
    KERNEL_SHARE_INFO_DIRTY
  };

  // Various data that the Directory::Kernel we are backing (persisting data
  // for) needs saved across runs of the application.
  struct SYNC_EXPORT_PRIVATE PersistedKernelInfo {
    PersistedKernelInfo();
    ~PersistedKernelInfo();

    // Set the |download_progress| entry for the given model to a
    // "first sync" start point.  When such a value is sent to the server,
    // a full download of all objects of the model will be initiated.
    void ResetDownloadProgress(ModelType model_type);

    // Whether a valid progress marker exists for |model_type|.
    bool HasEmptyDownloadProgress(ModelType model_type);

    // Last sync timestamp fetched from the server.
    sync_pb::DataTypeProgressMarker download_progress[MODEL_TYPE_COUNT];
    // Sync-side transaction version per data type. Monotonically incremented
    // when updating native model. A copy is also saved in native model.
    // Later out-of-sync models can be detected and fixed by comparing
    // transaction versions of sync model and native model.
    // TODO(hatiaol): implement detection and fixing of out-of-sync models.
    //                Bug 154858.
    int64 transaction_version[MODEL_TYPE_COUNT];
    // The store birthday we were given by the server. Contents are opaque to
    // the client.
    std::string store_birthday;
    // The next local ID that has not been used with this cache-GUID.
    int64 next_id;
    // The serialized bag of chips we were given by the server. Contents are
    // opaque to the client. This is the serialization of a message of type
    // ChipBag defined in sync.proto. It can contains NULL characters.
    std::string bag_of_chips;
    // The per-datatype context.
    sync_pb::DataTypeContext datatype_context[MODEL_TYPE_COUNT];
  };

  // What the Directory needs on initialization to create itself and its Kernel.
  // Filled by DirectoryBackingStore::Load.
  struct KernelLoadInfo {
    PersistedKernelInfo kernel_info;
    std::string cache_guid;  // Created on first initialization, never changes.
    int64 max_metahandle;    // Computed (using sql MAX aggregate) on init.
    KernelLoadInfo() : max_metahandle(0) {
    }
  };

  // When the Directory is told to SaveChanges, a SaveChangesSnapshot is
  // constructed and forms a consistent snapshot of what needs to be sent to
  // the backing store.
  struct SYNC_EXPORT_PRIVATE SaveChangesSnapshot {
    SaveChangesSnapshot();
    ~SaveChangesSnapshot();

    KernelShareInfoStatus kernel_info_status;
    PersistedKernelInfo kernel_info;
    EntryKernelSet dirty_metas;
    MetahandleSet metahandles_to_purge;
    EntryKernelSet delete_journals;
    MetahandleSet delete_journals_to_purge;
  };

  // Does not take ownership of |encryptor|.
  // |report_unrecoverable_error_function| may be NULL.
  // Takes ownership of |store|.
  Directory(
      DirectoryBackingStore* store,
      UnrecoverableErrorHandler* unrecoverable_error_handler,
      ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      NigoriHandler* nigori_handler,
      Cryptographer* cryptographer);
  virtual ~Directory();

  // Does not take ownership of |delegate|, which must not be NULL.
  // Starts sending events to |delegate| if the returned result is
  // OPENED.  Note that events to |delegate| may be sent from *any*
  // thread.  |transaction_observer| must be initialized.
  DirOpenResult Open(const std::string& name,
                     DirectoryChangeDelegate* delegate,
                     const WeakHandle<TransactionObserver>&
                         transaction_observer);

  // Stops sending events to the delegate and the transaction
  // observer.
  void Close();

  int64 NextMetahandle();
  // Returns a negative integer unique to this client.
  syncable::Id NextId();

  bool good() const { return NULL != kernel_; }

  // The download progress is an opaque token provided by the sync server
  // to indicate the continuation state of the next GetUpdates operation.
  void GetDownloadProgress(
      ModelType type,
      sync_pb::DataTypeProgressMarker* value_out) const;
  void GetDownloadProgressAsString(
      ModelType type,
      std::string* value_out) const;
  size_t GetEntriesCount() const;
  void SetDownloadProgress(
      ModelType type,
      const sync_pb::DataTypeProgressMarker& value);

  // Gets/Increments transaction version of a model type. Must be called when
  // holding kernel mutex.
  int64 GetTransactionVersion(ModelType type) const;
  void IncrementTransactionVersion(ModelType type);

  // Getter/setters for the per datatype context.
  void GetDataTypeContext(BaseTransaction* trans,
                          ModelType type,
                          sync_pb::DataTypeContext* context) const;
  void SetDataTypeContext(BaseWriteTransaction* trans,
                          ModelType type,
                          const sync_pb::DataTypeContext& context);

  ModelTypeSet InitialSyncEndedTypes();
  bool InitialSyncEndedForType(ModelType type);
  bool InitialSyncEndedForType(BaseTransaction* trans, ModelType type);

  const std::string& name() const { return kernel_->name; }

  // (Account) Store birthday is opaque to the client, so we keep it in the
  // format it is in the proto buffer in case we switch to a binary birthday
  // later.
  std::string store_birthday() const;
  void set_store_birthday(const std::string& store_birthday);

  // (Account) Bag of chip is an opaque state used by the server to track the
  // client.
  std::string bag_of_chips() const;
  void set_bag_of_chips(const std::string& bag_of_chips);

  // Unique to each account / client pair.
  std::string cache_guid() const;

  // Returns a pointer to our Nigori node handler.
  NigoriHandler* GetNigoriHandler();

  // Returns a pointer to our cryptographer. Does not transfer ownership.
  // Not thread safe, so should only be accessed while holding a transaction.
  Cryptographer* GetCryptographer(const BaseTransaction* trans);

  // Returns true if the directory had encountered an unrecoverable error.
  // Note: Any function in |Directory| that can be called without holding a
  // transaction need to check if the Directory already has an unrecoverable
  // error on it.
  bool unrecoverable_error_set(const BaseTransaction* trans) const;

  // Called to immediately report an unrecoverable error (but don't
  // propagate it up).
  void ReportUnrecoverableError() {
    if (report_unrecoverable_error_function_) {
      report_unrecoverable_error_function_();
    }
  }

  // Called to set the unrecoverable error on the directory and to propagate
  // the error to upper layers.
  void OnUnrecoverableError(const BaseTransaction* trans,
                            const tracked_objects::Location& location,
                            const std::string & message);

  DeleteJournal* delete_journal();

  // Returns the child meta handles (even those for deleted/unlinked
  // nodes) for given parent id.  Clears |result| if there are no
  // children.
  bool GetChildHandlesById(BaseTransaction*, const Id& parent_id,
      Metahandles* result);

  // Counts all items under the given node, including the node itself.
  int GetTotalNodeCount(BaseTransaction*, EntryKernel* kernel_) const;

  // Returns this item's position within its parent folder.
  // The left-most item is 0, second left-most is 1, etc.
  int GetPositionIndex(BaseTransaction*, EntryKernel* kernel_) const;

  // Returns true iff |id| has children.
  bool HasChildren(BaseTransaction* trans, const Id& id);

  // Find the first child in the positional ordering under a parent,
  // and fill in |*first_child_id| with its id.  Fills in a root Id if
  // parent has no children.  Returns true if the first child was
  // successfully found, or false if an error was encountered.
  Id GetFirstChildId(BaseTransaction* trans, const EntryKernel* parent);

  // These functions allow one to fetch the next or previous item under
  // the same folder.  Returns the "root" ID if there is no predecessor
  // or successor.
  //
  // TODO(rlarocque): These functions are used mainly for tree traversal.  We
  // should replace these with an iterator API.  See crbug.com/178275.
  syncable::Id GetPredecessorId(EntryKernel*);
  syncable::Id GetSuccessorId(EntryKernel*);

  // Places |e| as a successor to |predecessor|.  If |predecessor| is NULL,
  // |e| will be placed as the left-most item in its folder.
  //
  // Both |e| and |predecessor| must be valid entries under the same parent.
  //
  // TODO(rlarocque): This function includes limited support for placing items
  // with valid positions (ie. Bookmarks) as siblings of items that have no set
  // ordering (ie. Autofill items).  This support is required only for tests,
  // and should be removed.  See crbug.com/178282.
  void PutPredecessor(EntryKernel* e, EntryKernel* predecessor);

  // SaveChanges works by taking a consistent snapshot of the current Directory
  // state and indices (by deep copy) under a ReadTransaction, passing this
  // snapshot to the backing store under no transaction, and finally cleaning
  // up by either purging entries no longer needed (this part done under a
  // WriteTransaction) or rolling back the dirty bits.  It also uses
  // internal locking to enforce SaveChanges operations are mutually exclusive.
  //
  // WARNING: THIS METHOD PERFORMS SYNCHRONOUS I/O VIA SQLITE.
  bool SaveChanges();

  // Returns the number of entities with the unsynced bit set.
  int64 unsynced_entity_count() const;

  // Get GetUnsyncedMetaHandles should only be called after SaveChanges and
  // before any new entries have been created. The intention is that the
  // syncer should call it from its PerformSyncQueries member.
  void GetUnsyncedMetaHandles(BaseTransaction* trans,
                              Metahandles* result);

  // Returns whether or not this |type| has unapplied updates.
  bool TypeHasUnappliedUpdates(ModelType type);

  // Get all the metahandles for unapplied updates for a given set of
  // server types.
  void GetUnappliedUpdateMetaHandles(BaseTransaction* trans,
                                     FullModelTypeSet server_types,
                                     std::vector<int64>* result);

  // Get all the metahandles of entries of |type|.
  void GetMetaHandlesOfType(BaseTransaction* trans,
                            ModelType type,
                            Metahandles* result);

  // Get metahandle counts for various criteria to show on the
  // about:sync page. The information is computed on the fly
  // each time. If this results in a significant performance hit,
  // additional data structures can be added to cache results.
  void CollectMetaHandleCounts(std::vector<int>* num_entries_by_type,
                               std::vector<int>* num_to_delete_entries_by_type);

  // Returns a ListValue serialization of all nodes for the given type.
  scoped_ptr<base::ListValue> GetNodeDetailsForType(
      BaseTransaction* trans,
      ModelType type);

  // Sets the level of invariant checking performed after transactions.
  void SetInvariantCheckLevel(InvariantCheckLevel check_level);

  // Checks tree metadata consistency following a transaction.  It is intended
  // to provide a reasonable tradeoff between performance and comprehensiveness
  // and may be used in release code.
  bool CheckInvariantsOnTransactionClose(
      syncable::BaseTransaction* trans,
      const MetahandleSet& modified_handles);

  // Forces a full check of the directory.  This operation may be slow and
  // should not be invoked outside of tests.
  bool FullyCheckTreeInvariants(BaseTransaction *trans);

  // Purges data associated with any entries whose ModelType or ServerModelType
  // is found in |disabled_types|, from sync directory _both_ in memory and on
  // disk. Only valid, "real" model types are allowed in |disabled_types| (see
  // model_type.h for definitions).
  // 1. Data associated with |types_to_journal| is saved in the delete journal
  // to help prevent back-from-dead problem due to offline delete in the next
  // sync session. |types_to_journal| must be a subset of |disabled_types|.
  // 2. Data associated with |types_to_unapply| is reset to an "unapplied"
  // state, wherein all local data is deleted and IS_UNAPPLIED is set to true.
  // This is useful when there's no benefit in discarding the currently
  // downloaded state, such as when there are cryptographer errors.
  // |types_to_unapply| must be a subset of |disabled_types|.
  // 3. All other data is purged entirely.
  // Note: "Purge" is just meant to distinguish from "deleting" entries, which
  // means something different in the syncable namespace.
  // WARNING! This can be real slow, as it iterates over all entries.
  // WARNING! Performs synchronous I/O.
  // Returns: true on success, false if an error was encountered.
  virtual bool PurgeEntriesWithTypeIn(ModelTypeSet disabled_types,
                                      ModelTypeSet types_to_journal,
                                      ModelTypeSet types_to_unapply);

  // Resets the base_versions and server_versions of all synced entities
  // associated with |type| to 1.
  // WARNING! This can be slow, as it iterates over all entries for a type.
  bool ResetVersionsForType(BaseWriteTransaction* trans, ModelType type);

  // Returns true iff the attachment identified by |attachment_id_proto| is
  // linked to an entry.
  //
  // An attachment linked to a deleted entry is still considered linked if the
  // entry hasn't yet been purged.
  bool IsAttachmentLinked(
      const sync_pb::AttachmentIdProto& attachment_id_proto) const;

  // Given attachment id return metahandles to all entries that reference this
  // attachment.
  void GetMetahandlesByAttachmentId(
      BaseTransaction* trans,
      const sync_pb::AttachmentIdProto& attachment_id_proto,
      Metahandles* result);

  // Change entry to not dirty. Used in special case when we don't want to
  // persist modified entry on disk. e.g. SyncBackupManager uses this to
  // preserve sync preferences in DB on disk.
  void UnmarkDirtyEntry(WriteTransaction* trans, Entry* entry);

 protected:  // for friends, mainly used by Entry constructors
  virtual EntryKernel* GetEntryByHandle(int64 handle);
  virtual EntryKernel* GetEntryByHandle(int64 metahandle,
      ScopedKernelLock* lock);
  virtual EntryKernel* GetEntryById(const Id& id);
  EntryKernel* GetEntryByServerTag(const std::string& tag);
  virtual EntryKernel* GetEntryByClientTag(const std::string& tag);
  bool ReindexId(BaseWriteTransaction* trans, EntryKernel* const entry,
                 const Id& new_id);
  bool ReindexParentId(BaseWriteTransaction* trans, EntryKernel* const entry,
                       const Id& new_parent_id);
  // Update the attachment index for |metahandle| removing it from the index
  // under |old_metadata| entries and add it under |new_metadata| entries.
  void UpdateAttachmentIndex(const int64 metahandle,
                             const sync_pb::AttachmentMetadata& old_metadata,
                             const sync_pb::AttachmentMetadata& new_metadata);
  void ClearDirtyMetahandles();

  DirOpenResult OpenImpl(
      const std::string& name,
      DirectoryChangeDelegate* delegate,
      const WeakHandle<TransactionObserver>& transaction_observer);

 private:
  struct Kernel {
    // |delegate| must not be NULL.  |transaction_observer| must be
    // initialized.
    Kernel(const std::string& name, const KernelLoadInfo& info,
           DirectoryChangeDelegate* delegate,
           const WeakHandle<TransactionObserver>& transaction_observer);

    ~Kernel();

    // Implements ReadTransaction / WriteTransaction using a simple lock.
    base::Lock transaction_mutex;

    // Protected by transaction_mutex.  Used by WriteTransactions.
    int64 next_write_transaction_id;

    // The name of this directory.
    std::string const name;

    // Protects all members below.
    // The mutex effectively protects all the indices, but not the
    // entries themselves.  So once a pointer to an entry is pulled
    // from the index, the mutex can be unlocked and entry read or written.
    //
    // Never hold the mutex and do anything with the database or any
    // other buffered IO.  Violating this rule will result in deadlock.
    base::Lock mutex;

    // Entries indexed by metahandle.  This container is considered to be the
    // owner of all EntryKernels, which may be referened by the other
    // containers.  If you remove an EntryKernel from this map, you probably
    // want to remove it from all other containers and delete it, too.
    MetahandlesMap metahandles_map;

    // Entries indexed by id
    IdsMap ids_map;

    // Entries indexed by server tag.
    // This map does not include any entries with non-existent server tags.
    TagsMap server_tags_map;

    // Entries indexed by client tag.
    // This map does not include any entries with non-existent client tags.
    // IS_DEL items are included.
    TagsMap client_tags_map;

    // Contains non-deleted items, indexed according to parent and position
    // within parent.  Protected by the ScopedKernelLock.
    ParentChildIndex parent_child_index;

    // This index keeps track of which metahandles refer to a given attachment.
    // Think of it as the inverse of EntryKernel's AttachmentMetadata Records.
    //
    // Because entries can be undeleted (e.g. PutIsDel(false)), entries should
    // not removed from the index until they are actually deleted from memory.
    //
    // All access should go through IsAttachmentLinked,
    // RemoveFromAttachmentIndex, AddToAttachmentIndex, and
    // UpdateAttachmentIndex methods to avoid iterator invalidation errors.
    IndexByAttachmentId index_by_attachment_id;

    // 3 in-memory indices on bits used extremely frequently by the syncer.
    // |unapplied_update_metahandles| is keyed by the server model type.
    MetahandleSet unapplied_update_metahandles[MODEL_TYPE_COUNT];
    MetahandleSet unsynced_metahandles;
    // Contains metahandles that are most likely dirty (though not
    // necessarily).  Dirtyness is confirmed in TakeSnapshotForSaveChanges().
    MetahandleSet dirty_metahandles;

    // When a purge takes place, we remove items from all our indices and stash
    // them in here so that SaveChanges can persist their permanent deletion.
    MetahandleSet metahandles_to_purge;

    KernelShareInfoStatus info_status;

    // These 3 members are backed in the share_info table, and
    // their state is marked by the flag above.

    // A structure containing the Directory state that is written back into the
    // database on SaveChanges.
    PersistedKernelInfo persisted_info;

    // A unique identifier for this account's cache db, used to generate
    // unique server IDs. No need to lock, only written at init time.
    const std::string cache_guid;

    // It doesn't make sense for two threads to run SaveChanges at the same
    // time; this mutex protects that activity.
    base::Lock save_changes_mutex;

    // The next metahandle is protected by kernel mutex.
    int64 next_metahandle;

    // The delegate for directory change events.  Must not be NULL.
    DirectoryChangeDelegate* const delegate;

    // The transaction observer.
    const WeakHandle<TransactionObserver> transaction_observer;
  };

  // These private versions expect the kernel lock to already be held
  // before calling.
  EntryKernel* GetEntryById(const Id& id, ScopedKernelLock* const lock);

  // A helper that implements the logic of checking tree invariants.
  bool CheckTreeInvariants(syncable::BaseTransaction* trans,
                           const MetahandleSet& handles);

  // Helper to prime metahandles_map, ids_map, parent_child_index,
  // unsynced_metahandles, unapplied_update_metahandles, server_tags_map and
  // client_tags_map from metahandles_index.  The input |handles_map| will be
  // cleared during the initialization process.
  void InitializeIndices(MetahandlesMap* handles_map);

  // Constructs a consistent snapshot of the current Directory state and
  // indices (by deep copy) under a ReadTransaction for use in |snapshot|.
  // See SaveChanges() for more information.
  void TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot);

  // Purges from memory any unused, safe to remove entries that were
  // successfully deleted on disk as a result of the SaveChanges that processed
  // |snapshot|.  See SaveChanges() for more information.
  bool VacuumAfterSaveChanges(const SaveChangesSnapshot& snapshot);

  // Rolls back dirty bits in the event that the SaveChanges that
  // processed |snapshot| failed, for example, due to no disk space.
  void HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot);

  // For new entry creation only
  bool InsertEntry(BaseWriteTransaction* trans,
                   EntryKernel* entry, ScopedKernelLock* lock);
  bool InsertEntry(BaseWriteTransaction* trans, EntryKernel* entry);

  // Used by CheckTreeInvariants
  void GetAllMetaHandles(BaseTransaction* trans, MetahandleSet* result);
  bool SafeToPurgeFromMemory(WriteTransaction* trans,
                             const EntryKernel* const entry) const;

  // A helper used by GetTotalNodeCount.
  void GetChildSetForKernel(
      BaseTransaction*,
      EntryKernel* kernel_,
      std::deque<const OrderedChildSet*>* child_sets) const;

  // Append the handles of the children of |parent_id| to |result|.
  void AppendChildHandles(
      const ScopedKernelLock& lock,
      const Id& parent_id, Directory::Metahandles* result);

  // Helper methods used by PurgeDisabledTypes.
  void UnapplyEntry(EntryKernel* entry);
  void DeleteEntry(bool save_to_journal,
                   EntryKernel* entry,
                   EntryKernelSet* entries_to_journal,
                   const ScopedKernelLock& lock);

  // Remove each of |metahandle|'s attachment ids from index_by_attachment_id.
  void RemoveFromAttachmentIndex(
      const int64 metahandle,
      const sync_pb::AttachmentMetadata& attachment_metadata,
      const ScopedKernelLock& lock);
  // Add each of |metahandle|'s attachment ids to the index_by_attachment_id.
  void AddToAttachmentIndex(
      const int64 metahandle,
      const sync_pb::AttachmentMetadata& attachment_metadata,
      const ScopedKernelLock& lock);

  Kernel* kernel_;

  scoped_ptr<DirectoryBackingStore> store_;

  UnrecoverableErrorHandler* const unrecoverable_error_handler_;
  const ReportUnrecoverableErrorFunction report_unrecoverable_error_function_;
  bool unrecoverable_error_set_;

  // Not owned.
  NigoriHandler* const nigori_handler_;
  Cryptographer* const cryptographer_;

  InvariantCheckLevel invariant_check_level_;

  // Maintain deleted entries not in |kernel_| until it's verified that they
  // are deleted in native models as well.
  scoped_ptr<DeleteJournal> delete_journal_;

  DISALLOW_COPY_AND_ASSIGN(Directory);
};

}  // namespace syncable
}  // namespace syncer

#endif // SYNC_SYNCABLE_DIRECTORY_H_
