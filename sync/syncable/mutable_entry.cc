// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/mutable_entry.h"

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/node_ordinal.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/scoped_index_updater.h"
#include "sync/syncable/scoped_kernel_lock.h"
#include "sync/syncable/syncable-inl.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"

using std::string;

namespace syncer {
namespace syncable {

void MutableEntry::Init(WriteTransaction* trans,
                        ModelType model_type,
                        const Id& parent_id,
                        const string& name) {
  scoped_ptr<EntryKernel> kernel(new EntryKernel);
  kernel_ = NULL;

  kernel->put(ID, trans->directory_->NextId());
  kernel->put(META_HANDLE, trans->directory_->NextMetahandle());
  kernel->mark_dirty(trans->directory_->kernel_->dirty_metahandles);
  kernel->put(PARENT_ID, parent_id);
  kernel->put(NON_UNIQUE_NAME, name);
  const base::Time& now = base::Time::Now();
  kernel->put(CTIME, now);
  kernel->put(MTIME, now);
  // We match the database defaults here
  kernel->put(BASE_VERSION, CHANGES_VERSION);
  kernel->put(SERVER_ORDINAL_IN_PARENT, NodeOrdinal::CreateInitialOrdinal());

  // Normally the SPECIFICS setting code is wrapped in logic to deal with
  // unknown fields and encryption.  Since all we want to do here is ensure that
  // GetModelType() returns a correct value from the very beginning, these
  // few lines are sufficient.
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  kernel->put(SPECIFICS, specifics);

  // Because this entry is new, it was originally deleted.
  kernel->put(IS_DEL, true);
  trans->SaveOriginal(kernel.get());
  kernel->put(IS_DEL, false);

  // Now swap the pointers.
  kernel_ = kernel.release();
}

MutableEntry::MutableEntry(WriteTransaction* trans,
                           Create,
                           ModelType model_type,
                           const Id& parent_id,
                           const string& name)
    : Entry(trans),
      write_transaction_(trans) {
  Init(trans, model_type, parent_id, name);
  bool insert_result = trans->directory()->InsertEntry(trans, kernel_);
  DCHECK(insert_result);
}

MutableEntry::MutableEntry(WriteTransaction* trans, CreateNewUpdateItem,
                           const Id& id)
    : Entry(trans), write_transaction_(trans) {
  Entry same_id(trans, GET_BY_ID, id);
  kernel_ = NULL;
  if (same_id.good()) {
    return;  // already have an item with this ID.
  }
  scoped_ptr<EntryKernel> kernel(new EntryKernel());

  kernel->put(ID, id);
  kernel->put(META_HANDLE, trans->directory_->NextMetahandle());
  kernel->mark_dirty(trans->directory_->kernel_->dirty_metahandles);
  kernel->put(SERVER_ORDINAL_IN_PARENT, NodeOrdinal::CreateInitialOrdinal());
  kernel->put(IS_DEL, true);
  // We match the database defaults here
  kernel->put(BASE_VERSION, CHANGES_VERSION);
  if (!trans->directory()->InsertEntry(trans, kernel.get())) {
    return;  // Failed inserting.
  }
  trans->SaveOriginal(kernel.get());

  kernel_ = kernel.release();
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetById, const Id& id)
    : Entry(trans, GET_BY_ID, id), write_transaction_(trans) {
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByHandle,
                           int64 metahandle)
    : Entry(trans, GET_BY_HANDLE, metahandle), write_transaction_(trans) {
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByClientTag,
                           const std::string& tag)
    : Entry(trans, GET_BY_CLIENT_TAG, tag), write_transaction_(trans) {
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByServerTag,
                           const string& tag)
    : Entry(trans, GET_BY_SERVER_TAG, tag), write_transaction_(trans) {
}

bool MutableEntry::PutIsDel(bool is_del) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (is_del == kernel_->ref(IS_DEL)) {
    return true;
  }
  if (is_del) {
    if (!UnlinkFromOrder()) {
      return false;
    }

    // If the server never knew about this item and it's deleted then we don't
    // need to keep it around.  Unsetting IS_UNSYNCED will:
    // - Ensure that the item is never committed to the server.
    // - Allow any items with the same UNIQUE_CLIENT_TAG created on other
    //   clients to override this entry.
    // - Let us delete this entry permanently through
    //   DirectoryBackingStore::DropDeletedEntries() when we next restart sync.
    //   This will save memory and avoid crbug.com/125381.
    if (!Get(ID).ServerKnows()) {
      Put(IS_UNSYNCED, false);
    }
  }

  {
    ScopedKernelLock lock(dir());
    // Some indices don't include deleted items and must be updated
    // upon a value change.
    ScopedIndexUpdater<ParentIdAndHandleIndexer> updater(lock, kernel_,
        dir()->kernel_->parent_id_child_index);

    kernel_->put(IS_DEL, is_del);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }

  if (!is_del)
    // Restores position to the 0th index.
    if (!PutPredecessor(Id())) {
      // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
      NOTREACHED();
    }

  return true;
}

bool MutableEntry::Put(Int64Field field, const int64& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(field) != value) {
    ScopedKernelLock lock(dir());
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(TimeField field, const base::Time& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(IdField field, const Id& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(field) != value) {
    if (ID == field) {
      if (!dir()->ReindexId(write_transaction(), kernel_, value))
        return false;
    } else if (PARENT_ID == field) {
      PutParentIdPropertyOnly(value);  // Makes sibling order inconsistent.
      // Fixes up the sibling order inconsistency.
      if (!PutPredecessor(Id())) {
        // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
        NOTREACHED();
      }
    } else {
      kernel_->put(field, value);
    }
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(OrdinalField field, const NodeOrdinal& value) {
  DCHECK(kernel_);
  DCHECK(value.IsValid());
  write_transaction_->SaveOriginal(kernel_);
  if(!kernel_->ref(field).Equals(value)) {
    ScopedKernelLock lock(dir());
    if (SERVER_ORDINAL_IN_PARENT == field) {
      ScopedIndexUpdater<ParentIdAndHandleIndexer> updater(
          lock, kernel_, dir()->kernel_->parent_id_child_index);
      kernel_->put(field, value);
    } else {
      kernel_->put(field, value);
    }
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void MutableEntry::PutParentIdPropertyOnly(const Id& parent_id) {
  write_transaction_->SaveOriginal(kernel_);
  dir()->ReindexParentId(write_transaction(), kernel_, parent_id);
  kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
}

bool MutableEntry::Put(BaseVersion field, int64 value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(StringField field, const string& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (field == UNIQUE_CLIENT_TAG) {
    return PutUniqueClientTag(value);
  }

  if (kernel_->ref(field) != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(ProtoField field,
                       const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(field).SerializeAsString() != value.SerializeAsString()) {
    const bool update_unapplied_updates_index =
        (field == SERVER_SPECIFICS) && kernel_->ref(IS_UNAPPLIED_UPDATE);
    if (update_unapplied_updates_index) {
      // Remove ourselves from unapplied_update_metahandles with our
      // old server type.
      const ModelType old_server_type = kernel_->GetServerModelType();
      const int64 metahandle = kernel_->ref(META_HANDLE);
      size_t erase_count =
          dir()->kernel_->unapplied_update_metahandles[old_server_type]
          .erase(metahandle);
      DCHECK_EQ(erase_count, 1u);
    }

    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);

    if (update_unapplied_updates_index) {
      // Add ourselves back into unapplied_update_metahandles with our
      // new server type.
      const ModelType new_server_type = kernel_->GetServerModelType();
      const int64 metahandle = kernel_->ref(META_HANDLE);
      dir()->kernel_->unapplied_update_metahandles[new_server_type]
          .insert(metahandle);
    }
  }
  return true;
}

bool MutableEntry::Put(BitField field, bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  bool old_value = kernel_->ref(field);
  if (old_value != value) {
    kernel_->put(field, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }

  // Update delete journal for existence status change on server side here
  // instead of in PutIsDel() because IS_DEL may not be updated due to
  // early returns when processing updates. And because
  // UpdateDeleteJournalForServerDelete() checks for SERVER_IS_DEL, it has
  // to be called on sync thread.
  if (field == SERVER_IS_DEL) {
    dir()->delete_journal()->UpdateDeleteJournalForServerDelete(
        write_transaction(), old_value, *kernel_);
  }

  return true;
}

MetahandleSet* MutableEntry::GetDirtyIndexHelper() {
  return dir()->kernel_->dirty_metahandles;
}

bool MutableEntry::PutUniqueClientTag(const string& new_tag) {
  write_transaction_->SaveOriginal(kernel_);
  // There is no SERVER_UNIQUE_CLIENT_TAG. This field is similar to ID.
  string old_tag = kernel_->ref(UNIQUE_CLIENT_TAG);
  if (old_tag == new_tag) {
    return true;
  }

  ScopedKernelLock lock(dir());
  if (!new_tag.empty()) {
    // Make sure your new value is not in there already.
    EntryKernel lookup_kernel_ = *kernel_;
    lookup_kernel_.put(UNIQUE_CLIENT_TAG, new_tag);
    bool new_tag_conflicts =
        (dir()->kernel_->client_tag_index->count(&lookup_kernel_) > 0);
    if (new_tag_conflicts) {
      return false;
    }
  }

  {
    ScopedIndexUpdater<ClientTagIndexer> index_updater(lock, kernel_,
        dir()->kernel_->client_tag_index);
    kernel_->put(UNIQUE_CLIENT_TAG, new_tag);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(IndexedBitField field, bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(field) != value) {
    MetahandleSet* index;
    if (IS_UNSYNCED == field) {
      index = dir()->kernel_->unsynced_metahandles;
    } else {
      // Use kernel_->GetServerModelType() instead of
      // GetServerModelType() as we may trigger some DCHECKs in the
      // latter.
      index =
          &dir()->kernel_->unapplied_update_metahandles[
              kernel_->GetServerModelType()];
    }

    ScopedKernelLock lock(dir());
    if (value) {
      if (!SyncAssert(index->insert(kernel_->ref(META_HANDLE)).second,
                      FROM_HERE,
                      "Could not insert",
                      write_transaction())) {
        return false;
      }
    } else {
      if (!SyncAssert(1U == index->erase(kernel_->ref(META_HANDLE)),
                      FROM_HERE,
                      "Entry Not succesfully erased",
                      write_transaction())) {
        return false;
      }
    }
    kernel_->put(field, value);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::UnlinkFromOrder() {
  ScopedKernelLock lock(dir());
  return dir()->UnlinkEntryFromOrder(kernel_,
                                     write_transaction(),
                                     &lock,
                                     NODE_MANIPULATION);
}

bool MutableEntry::PutPredecessor(const Id& predecessor_id) {
  if (!UnlinkFromOrder())
    return false;

  if (Get(IS_DEL)) {
    DCHECK(predecessor_id.IsNull());
    return true;
  }

  // TODO(ncarter): It should be possible to not maintain position for
  // non-bookmark items.  However, we'd need to robustly handle all possible
  // permutations of setting IS_DEL and the SPECIFICS to identify the
  // object type; or else, we'd need to add a ModelType to the
  // MutableEntry's Create ctor.
  //   if (!ShouldMaintainPosition()) {
  //     return false;
  //   }

  // This is classic insert-into-doubly-linked-list from CS 101 and your last
  // job interview.  An "IsRoot" Id signifies the head or tail.
  Id successor_id;
  if (!predecessor_id.IsRoot()) {
    MutableEntry predecessor(write_transaction(), GET_BY_ID, predecessor_id);
    if (!predecessor.good()) {
      LOG(ERROR) << "Predecessor is not good : "
                 << predecessor_id.GetServerId();
      return false;
    }
    if (predecessor.Get(PARENT_ID) != Get(PARENT_ID))
      return false;
    successor_id = predecessor.GetSuccessorId();
    predecessor.Put(NEXT_ID, Get(ID));
  } else {
    syncable::Directory* dir = trans()->directory();
    if (!dir->GetFirstChildId(trans(), Get(PARENT_ID), &successor_id)) {
      return false;
    }
  }
  if (!successor_id.IsRoot()) {
    MutableEntry successor(write_transaction(), GET_BY_ID, successor_id);
    if (!successor.good()) {
      LOG(ERROR) << "Successor is not good: "
                 << successor_id.GetServerId();
      return false;
    }
    if (successor.Get(PARENT_ID) != Get(PARENT_ID))
      return false;
    successor.Put(PREV_ID, Get(ID));
  }
  DCHECK(predecessor_id != Get(ID));
  DCHECK(successor_id != Get(ID));
  Put(PREV_ID, predecessor_id);
  Put(NEXT_ID, successor_id);
  return true;
}

bool MutableEntry::Put(BitTemp field, bool value) {
  DCHECK(kernel_);
  kernel_->put(field, value);
  return true;
}

// This function sets only the flags needed to get this entry to sync.
bool MarkForSyncing(MutableEntry* e) {
  DCHECK_NE(static_cast<MutableEntry*>(NULL), e);
  DCHECK(!e->IsRoot()) << "We shouldn't mark a permanent object for syncing.";
  if (!(e->Put(IS_UNSYNCED, true)))
    return false;
  e->Put(SYNCING, false);
  return true;
}

}  // namespace syncable
}  // namespace syncer
