// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/mutable_entry.h"

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/scoped_index_updater.h"
#include "sync/syncable/scoped_kernel_lock.h"
#include "sync/syncable/scoped_parent_child_index_updater.h"
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
  // We need to have a valid position ready before we can index the item.
  if (model_type == BOOKMARKS) {
    // Base the tag off of our cache-guid and local "c-" style ID.
    std::string unique_tag = syncable::GenerateSyncableBookmarkHash(
        trans->directory()->cache_guid(), Get(ID).GetServerId());
    kernel_->put(UNIQUE_BOOKMARK_TAG, unique_tag);
    kernel_->put(UNIQUE_POSITION, UniquePosition::InitialPosition(unique_tag));
  } else {
    DCHECK(!ShouldMaintainPosition());
  }

  bool result = trans->directory()->InsertEntry(trans, kernel_);
  DCHECK(result);
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
    ScopedParentChildIndexUpdater updater(lock, kernel_,
        dir()->kernel_->parent_child_index);

    kernel_->put(IS_DEL, is_del);
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
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
      PutParentIdPropertyOnly(value);
      if (!Get(IS_DEL)) {
        if (!PutPredecessor(Id())) {
          // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
          NOTREACHED();
        }
      }
    } else {
      kernel_->put(field, value);
    }
    kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::Put(UniquePositionField field, const UniquePosition& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if(!kernel_->ref(field).Equals(value)) {
    // We should never overwrite a valid position with an invalid one.
    DCHECK(value.IsValid());
    ScopedKernelLock lock(dir());
    if (UNIQUE_POSITION == field) {
      ScopedParentChildIndexUpdater updater(
          lock, kernel_, dir()->kernel_->parent_child_index);
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

void MutableEntry::PutUniqueBookmarkTag(const std::string& tag) {
  // This unique tag will eventually be used as the unique suffix when adjusting
  // this bookmark's position.  Let's make sure it's a valid suffix.
  if (!UniquePosition::IsValidSuffix(tag)) {
    NOTREACHED();
    return;
  }

  if (!kernel_->ref(UNIQUE_BOOKMARK_TAG).empty()
      && tag != kernel_->ref(UNIQUE_BOOKMARK_TAG)) {
    // There is only one scenario where our tag is expected to change.  That
    // scenario occurs when our current tag is a non-correct tag assigned during
    // the UniquePosition migration.
    std::string migration_generated_tag =
        GenerateSyncableBookmarkHash(std::string(),
                                     kernel_->ref(ID).GetServerId());
    DCHECK_EQ(migration_generated_tag, kernel_->ref(UNIQUE_BOOKMARK_TAG));
  }

  kernel_->put(UNIQUE_BOOKMARK_TAG, tag);
  kernel_->mark_dirty(dir()->kernel_->dirty_metahandles);
}

bool MutableEntry::PutPredecessor(const Id& predecessor_id) {
  MutableEntry predecessor(write_transaction_, GET_BY_ID, predecessor_id);
  if (!predecessor.good())
    return false;
  dir()->PutPredecessor(kernel_, predecessor.kernel_);
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
