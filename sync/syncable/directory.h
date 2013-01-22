// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_DIRECTORY_H_
#define SYNC_SYNCABLE_DIRECTORY_H_

#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/gtest_prod_util.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/util/report_unrecoverable_error_function.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/syncable/delete_journal.h"
#include "sync/syncable/dir_open_result.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/metahandle_set.h"
#include "sync/syncable/scoped_kernel_lock.h"

namespace syncer {

class Cryptographer;
class TestUserShare;
class UnrecoverableErrorHandler;

namespace syncable {

class BaseTransaction;
class DirectoryChangeDelegate;
class DirectoryBackingStore;
class NigoriHandler;
class ScopedKernelLock;
class TransactionObserver;
class WriteTransaction;

// How syncable indices & Indexers work.
//
// The syncable Directory maintains several indices on the Entries it tracks.
// The indices follow a common pattern:
//   (a) The index allows efficient lookup of an Entry* with particular
//       field values.  This is done by use of a std::set<> and a custom
//       comparator.
//   (b) There may be conditions for inclusion in the index -- for example,
//       deleted items might not be indexed.
//   (c) Because the index set contains only Entry*, one must be careful
//       to remove Entries from the set before updating the value of
//       an indexed field.
// The traits of an index are a Comparator (to define the set ordering) and a
// ShouldInclude function (to define the conditions for inclusion).  For each
// index, the traits are grouped into a class called an Indexer which
// can be used as a template type parameter.

template <typename FieldType, FieldType field_index> class LessField;

// Traits type for metahandle index.
struct MetahandleIndexer {
  // This index is of the metahandle field values.
  typedef LessField<MetahandleField, META_HANDLE> Comparator;

  // This index includes all entries.
  inline static bool ShouldInclude(const EntryKernel* a) {
    return true;
  }
};

// Traits type for ID field index.
struct IdIndexer {
  // This index is of the ID field values.
  typedef LessField<IdField, ID> Comparator;

  // This index includes all entries.
  inline static bool ShouldInclude(const EntryKernel* a) {
    return true;
  }
};

// Traits type for unique client tag index.
struct ClientTagIndexer {
  // This index is of the client-tag values.
  typedef LessField<StringField, UNIQUE_CLIENT_TAG> Comparator;

  // Items are only in this index if they have a non-empty client tag value.
  static bool ShouldInclude(const EntryKernel* a);
};

// This index contains EntryKernels ordered by parent ID and metahandle.
// It allows efficient lookup of the children of a given parent.
struct ParentIdAndHandleIndexer {
  // This index is of the parent ID and metahandle.  We use a custom
  // comparator.
  class Comparator {
   public:
    bool operator() (const syncable::EntryKernel* a,
                     const syncable::EntryKernel* b) const;
  };

  // This index does not include deleted items.
  static bool ShouldInclude(const EntryKernel* a);
};

// Given an Indexer providing the semantics of an index, defines the
// set type used to actually contain the index.
template <typename Indexer>
struct Index {
  typedef std::set<EntryKernel*, typename Indexer::Comparator> Set;
};

// Reason for unlinking.
enum UnlinkReason {
  NODE_MANIPULATION, // To be used by any operation manipulating the linked
                     // list.
  DATA_TYPE_PURGE    // To be used when purging a dataype.
};

enum InvariantCheckLevel {
  OFF = 0,            // No checking.
  VERIFY_CHANGES = 1, // Checks only mutated entries.  Does not check hierarchy.
  FULL_DB_VERIFICATION = 2 // Check every entry.  This can be expensive.
};

class SYNC_EXPORT Directory {
  friend class BaseTransaction;
  friend class Entry;
  friend class MutableEntry;
  friend class ReadTransaction;
  friend class ReadTransactionWithoutDB;
  friend class ScopedKernelLock;
  friend class ScopedKernelUnlock;
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
  static const FilePath::CharType kSyncDatabaseFilename[];

  // Various data that the Directory::Kernel we are backing (persisting data
  // for) needs saved across runs of the application.
  struct SYNC_EXPORT_PRIVATE PersistedKernelInfo {
    PersistedKernelInfo();
    ~PersistedKernelInfo();

    // Set the |download_progress| entry for the given model to a
    // "first sync" start point.  When such a value is sent to the server,
    // a full download of all objects of the model will be initiated.
    void reset_download_progress(ModelType model_type);

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
    // The persisted notification state.
    std::string notification_state;
    // The serialized bag of chips we were given by the server. Contents are
    // opaque to the client. This is the serialization of a message of type
    // ChipBag defined in sync.proto. It can contains NULL characters.
    std::string bag_of_chips;
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

  // The dirty/clean state of kernel fields backed by the share_info table.
  // This is public so it can be used in SaveChangesSnapshot for persistence.
  enum KernelShareInfoStatus {
    KERNEL_SHARE_INFO_INVALID,
    KERNEL_SHARE_INFO_VALID,
    KERNEL_SHARE_INFO_DIRTY
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
  // Always returns a negative id.  Positive client ids are generated
  // by the server only.
  Id NextId();

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

  std::string GetNotificationState() const;
  void SetNotificationState(const std::string& notification_state);

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

 protected:  // for friends, mainly used by Entry constructors
  virtual EntryKernel* GetEntryByHandle(int64 handle);
  virtual EntryKernel* GetEntryByHandle(int64 metahandle,
      ScopedKernelLock* lock);
  virtual EntryKernel* GetEntryById(const Id& id);
  EntryKernel* GetEntryByServerTag(const std::string& tag);
  virtual EntryKernel* GetEntryByClientTag(const std::string& tag);
  EntryKernel* GetRootEntry();
  bool ReindexId(WriteTransaction* trans, EntryKernel* const entry,
                 const Id& new_id);
  bool ReindexParentId(WriteTransaction* trans, EntryKernel* const entry,
                       const Id& new_parent_id);
  void ClearDirtyMetahandles();

  // These don't do semantic checking.
  // The semantic checking is implemented higher up.
  bool UnlinkEntryFromOrder(EntryKernel* entry,
                            WriteTransaction* trans,
                            ScopedKernelLock* lock,
                            UnlinkReason unlink_reason);

  DirOpenResult OpenImpl(
      const std::string& name,
      DirectoryChangeDelegate* delegate,
      const WeakHandle<TransactionObserver>& transaction_observer);

 private:
  // These private versions expect the kernel lock to already be held
  // before calling.
  EntryKernel* GetEntryById(const Id& id, ScopedKernelLock* const lock);

  template <class T> void TestAndSet(T* kernel_data, const T* data_to_set);

 public:
  typedef std::vector<int64> ChildHandles;

  // Returns the child meta handles (even those for deleted/unlinked
  // nodes) for given parent id.  Clears |result| if there are no
  // children.
  bool GetChildHandlesById(BaseTransaction*, const Id& parent_id,
      ChildHandles* result);

  // Returns the child meta handles (even those for deleted/unlinked
  // nodes) for given meta handle.  Clears |result| if there are no
  // children.
  bool GetChildHandlesByHandle(BaseTransaction*, int64 handle,
      ChildHandles* result);

  // Returns true iff |id| has children.
  bool HasChildren(BaseTransaction* trans, const Id& id);

  // Find the first child in the positional ordering under a parent,
  // and fill in |*first_child_id| with its id.  Fills in a root Id if
  // parent has no children.  Returns true if the first child was
  // successfully found, or false if an error was encountered.
  bool GetFirstChildId(BaseTransaction* trans, const Id& parent_id,
                       Id* first_child_id) WARN_UNUSED_RESULT;

  // Find the last child in the positional ordering under a parent,
  // and fill in |*first_child_id| with its id.  Fills in a root Id if
  // parent has no children.  Returns true if the first child was
  // successfully found, or false if an error was encountered.
  bool GetLastChildIdForTest(BaseTransaction* trans, const Id& parent_id,
                             Id* last_child_id) WARN_UNUSED_RESULT;

  // Compute a local predecessor position for |update_item|.  The position
  // is determined by the SERVER_POSITION_IN_PARENT value of |update_item|,
  // as well as the SERVER_POSITION_IN_PARENT values of any up-to-date
  // children of |parent_id|.
  Id ComputePrevIdFromServerPosition(
      const EntryKernel* update_item,
      const syncable::Id& parent_id);

  // SaveChanges works by taking a consistent snapshot of the current Directory
  // state and indices (by deep copy) under a ReadTransaction, passing this
  // snapshot to the backing store under no transaction, and finally cleaning
  // up by either purging entries no longer needed (this part done under a
  // WriteTransaction) or rolling back the dirty bits.  It also uses
  // internal locking to enforce SaveChanges operations are mutually exclusive.
  //
  // WARNING: THIS METHOD PERFORMS SYNCHRONOUS I/O VIA SQLITE.
  bool SaveChanges();

  // Fill in |result| with all entry kernels.
  void GetAllEntryKernels(BaseTransaction* trans,
                          std::vector<const EntryKernel*>* result);

  // Returns the number of entities with the unsynced bit set.
  int64 unsynced_entity_count() const;

  // Get GetUnsyncedMetaHandles should only be called after SaveChanges and
  // before any new entries have been created. The intention is that the
  // syncer should call it from its PerformSyncQueries member.
  typedef std::vector<int64> UnsyncedMetaHandles;
  void GetUnsyncedMetaHandles(BaseTransaction* trans,
                              UnsyncedMetaHandles* result);

  // Returns all server types with unapplied updates.  A subset of
  // those types can then be passed into
  // GetUnappliedUpdateMetaHandles() below.
  FullModelTypeSet GetServerTypesWithUnappliedUpdates(
      BaseTransaction* trans) const;

  // Get all the metahandles for unapplied updates for a given set of
  // server types.
  void GetUnappliedUpdateMetaHandles(BaseTransaction* trans,
                                     FullModelTypeSet server_types,
                                     std::vector<int64>* result);

  // Get metahandle counts for various criteria to show on the
  // about:sync page. The information is computed on the fly
  // each time. If this results in a significant performance hit,
  // additional data structures can be added to cache results.
  void CollectMetaHandleCounts(std::vector<int>* num_entries_by_type,
                               std::vector<int>* num_to_delete_entries_by_type);

  // Sets the level of invariant checking performed after transactions.
  void SetInvariantCheckLevel(InvariantCheckLevel check_level);

  // Checks tree metadata consistency following a transaction.  It is intended
  // to provide a reasonable tradeoff between performance and comprehensiveness
  // and may be used in release code.
  bool CheckInvariantsOnTransactionClose(
      syncable::BaseTransaction* trans,
      const EntryKernelMutationMap& mutations);

  // Forces a full check of the directory.  This operation may be slow and
  // should not be invoked outside of tests.
  bool FullyCheckTreeInvariants(BaseTransaction *trans);

  // Purges all data associated with any entries whose ModelType or
  // ServerModelType is found in |types|, from _both_ memory and disk.
  // Only  valid, "real" model types are allowed in |types| (see model_type.h
  // for definitions).  "Purge" is just meant to distinguish from "deleting"
  // entries, which means something different in the syncable namespace.
  // WARNING! This can be real slow, as it iterates over all entries.
  // WARNING! Performs synchronous I/O.
  // Returns: true on success, false if an error was encountered.
  virtual bool PurgeEntriesWithTypeIn(ModelTypeSet types);

 private:
  // A helper that implements the logic of checking tree invariants.
  bool CheckTreeInvariants(syncable::BaseTransaction* trans,
                           const MetahandleSet& handles);

  // Helper to prime ids_index, parent_id_and_names_index, unsynced_metahandles
  // and unapplied_metahandles from metahandles_index.
  void InitializeIndices();

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
  bool InsertEntry(WriteTransaction* trans,
                   EntryKernel* entry, ScopedKernelLock* lock);
  bool InsertEntry(WriteTransaction* trans, EntryKernel* entry);

  // Used by CheckTreeInvariants
  void GetAllMetaHandles(BaseTransaction* trans, MetahandleSet* result);
  bool SafeToPurgeFromMemory(WriteTransaction* trans,
                             const EntryKernel* const entry) const;

  // Internal setters that do not acquire a lock internally.  These are unsafe
  // on their own; caller must guarantee exclusive access manually by holding
  // a ScopedKernelLock.
  void SetNotificationStateUnsafe(const std::string& notification_state);

  Directory& operator = (const Directory&);

 public:
  typedef Index<MetahandleIndexer>::Set MetahandlesIndex;
  typedef Index<IdIndexer>::Set IdsIndex;
  // All entries in memory must be in both the MetahandlesIndex and
  // the IdsIndex, but only non-deleted entries will be the
  // ParentIdChildIndex.
  typedef Index<ParentIdAndHandleIndexer>::Set ParentIdChildIndex;

  // Contains both deleted and existing entries with tags.
  // We can't store only existing tags because the client would create
  // items that had a duplicated ID in the end, resulting in a DB key
  // violation. ID reassociation would fail after an attempted commit.
  typedef Index<ClientTagIndexer>::Set ClientTagIndex;

 protected:
  // Used by tests. |delegate| must not be NULL.
  // |transaction_observer| must be initialized.
  void InitKernelForTest(
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
    // Entries indexed by metahandle
    MetahandlesIndex* metahandles_index;
    // Entries indexed by id
    IdsIndex* ids_index;
    ParentIdChildIndex* parent_id_child_index;
    ClientTagIndex* client_tag_index;
    // So we don't have to create an EntryKernel every time we want to
    // look something up in an index.  Needle in haystack metaphor.
    EntryKernel needle;

    // 3 in-memory indices on bits used extremely frequently by the syncer.
    // |unapplied_update_metahandles| is keyed by the server model type.
    MetahandleSet unapplied_update_metahandles[MODEL_TYPE_COUNT];
    MetahandleSet* const unsynced_metahandles;
    // Contains metahandles that are most likely dirty (though not
    // necessarily).  Dirtyness is confirmed in TakeSnapshotForSaveChanges().
    MetahandleSet* const dirty_metahandles;

    // When a purge takes place, we remove items from all our indices and stash
    // them in here so that SaveChanges can persist their permanent deletion.
    MetahandleSet* const metahandles_to_purge;

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

  // Helper method used to do searches on |parent_id_child_index|.
  ParentIdChildIndex::iterator LocateInParentChildIndex(
      const ScopedKernelLock& lock,
      const Id& parent_id,
      int64 position_in_parent,
      const Id& item_id_for_tiebreaking);

  // Return an iterator to the beginning of the range of the children of
  // |parent_id| in the kernel's parent_id_child_index.
  ParentIdChildIndex::iterator GetParentChildIndexLowerBound(
      const ScopedKernelLock& lock,
      const Id& parent_id);

  // Return an iterator to just past the end of the range of the
  // children of |parent_id| in the kernel's parent_id_child_index.
  ParentIdChildIndex::iterator GetParentChildIndexUpperBound(
      const ScopedKernelLock& lock,
      const Id& parent_id);

  // Append the handles of the children of |parent_id| to |result|.
  void AppendChildHandles(
      const ScopedKernelLock& lock,
      const Id& parent_id, Directory::ChildHandles* result);

  // Return a pointer to what is probably (but not certainly) the
  // first child of |parent_id|, or NULL if |parent_id| definitely has
  // no children.
  EntryKernel* GetPossibleFirstChild(
      const ScopedKernelLock& lock, const Id& parent_id);

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
};

}  // namespace syncable
}  // namespace syncer

#endif // SYNC_SYNCABLE_DIRECTORY_H_
