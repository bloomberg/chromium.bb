// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/mutable_entry.h"

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/syncable/directory.h"
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
  kernel->mark_dirty(&trans->directory_->kernel_->dirty_metahandles);
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
        trans->directory()->cache_guid(), GetId().GetServerId());
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
  kernel->mark_dirty(&trans->directory_->kernel_->dirty_metahandles);
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

void MutableEntry::PutBaseVersion(int64 value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(BASE_VERSION) != value) {
    kernel_->put(BASE_VERSION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerVersion(int64 value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(SERVER_VERSION) != value) {
    ScopedKernelLock lock(dir());
    kernel_->put(SERVER_VERSION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutLocalExternalId(int64 value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(LOCAL_EXTERNAL_ID) != value) {
    ScopedKernelLock lock(dir());
    kernel_->put(LOCAL_EXTERNAL_ID, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutMtime(base::Time value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(MTIME) != value) {
    kernel_->put(MTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerMtime(base::Time value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(SERVER_MTIME) != value) {
    kernel_->put(SERVER_MTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutCtime(base::Time value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(CTIME) != value) {
    kernel_->put(CTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerCtime(base::Time value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(SERVER_CTIME) != value) {
    kernel_->put(SERVER_CTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

bool MutableEntry::PutId(const Id& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(ID) != value) {
    if (!dir()->ReindexId(write_transaction(), kernel_, value))
      return false;
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void MutableEntry::PutParentId(const Id& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(PARENT_ID) != value) {
    PutParentIdPropertyOnly(value);
    if (!GetIsDel()) {
      if (!PutPredecessor(Id())) {
        // TODO(lipalani) : Propagate the error to caller. crbug.com/100444.
        NOTREACHED();
      }
    }
  }
}

void MutableEntry::PutServerParentId(const Id& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);

  if (kernel_->ref(SERVER_PARENT_ID) != value) {
    kernel_->put(SERVER_PARENT_ID, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

bool MutableEntry::PutIsUnsynced(bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(IS_UNSYNCED) != value) {
    MetahandleSet* index = &dir()->kernel_->unsynced_metahandles;

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
    kernel_->put(IS_UNSYNCED, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool MutableEntry::PutIsUnappliedUpdate(bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (kernel_->ref(IS_UNAPPLIED_UPDATE) != value) {
    // Use kernel_->GetServerModelType() instead of
    // GetServerModelType() as we may trigger some DCHECKs in the
    // latter.
    MetahandleSet* index = &dir()->kernel_->unapplied_update_metahandles[
        kernel_->GetServerModelType()];

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
    kernel_->put(IS_UNAPPLIED_UPDATE, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void MutableEntry::PutIsDir(bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  bool old_value = kernel_->ref(IS_DIR);
  if (old_value != value) {
    kernel_->put(IS_DIR, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }
}

void MutableEntry::PutServerIsDir(bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  bool old_value = kernel_->ref(SERVER_IS_DIR);
  if (old_value != value) {
    kernel_->put(SERVER_IS_DIR, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }
}

void MutableEntry::PutIsDel(bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if (value == kernel_->ref(IS_DEL)) {
    return;
  }
  if (value) {
    // If the server never knew about this item and it's deleted then we don't
    // need to keep it around.  Unsetting IS_UNSYNCED will:
    // - Ensure that the item is never committed to the server.
    // - Allow any items with the same UNIQUE_CLIENT_TAG created on other
    //   clients to override this entry.
    // - Let us delete this entry permanently through
    //   DirectoryBackingStore::DropDeletedEntries() when we next restart sync.
    //   This will save memory and avoid crbug.com/125381.
    if (!GetId().ServerKnows()) {
      PutIsUnsynced(false);
    }
  }

  {
    ScopedKernelLock lock(dir());
    // Some indices don't include deleted items and must be updated
    // upon a value change.
    ScopedParentChildIndexUpdater updater(lock, kernel_,
        &dir()->kernel_->parent_child_index);

    kernel_->put(IS_DEL, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerIsDel(bool value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  bool old_value = kernel_->ref(SERVER_IS_DEL);
  if (old_value != value) {
    kernel_->put(SERVER_IS_DEL, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }

  // Update delete journal for existence status change on server side here
  // instead of in PutIsDel() because IS_DEL may not be updated due to
  // early returns when processing updates. And because
  // UpdateDeleteJournalForServerDelete() checks for SERVER_IS_DEL, it has
  // to be called on sync thread.
  dir()->delete_journal()->UpdateDeleteJournalForServerDelete(
      write_transaction(), old_value, *kernel_);
}

void MutableEntry::PutNonUniqueName(const std::string& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);

  if (kernel_->ref(NON_UNIQUE_NAME) != value) {
    kernel_->put(NON_UNIQUE_NAME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerNonUniqueName(const std::string& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);

  if (kernel_->ref(SERVER_NON_UNIQUE_NAME) != value) {
    kernel_->put(SERVER_NON_UNIQUE_NAME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

bool MutableEntry::PutUniqueServerTag(const string& new_tag) {
  if (new_tag == kernel_->ref(UNIQUE_SERVER_TAG)) {
    return true;
  }

  write_transaction_->SaveOriginal(kernel_);
  ScopedKernelLock lock(dir());
  // Make sure your new value is not in there already.
  if (dir()->kernel_->server_tags_map.find(new_tag) !=
      dir()->kernel_->server_tags_map.end()) {
    DVLOG(1) << "Detected duplicate server tag";
    return false;
  }
  dir()->kernel_->server_tags_map.erase(
      kernel_->ref(UNIQUE_SERVER_TAG));
  kernel_->put(UNIQUE_SERVER_TAG, new_tag);
  kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  if (!new_tag.empty()) {
    dir()->kernel_->server_tags_map[new_tag] = kernel_;
  }

  return true;
}

bool MutableEntry::PutUniqueClientTag(const string& new_tag) {
  if (new_tag == kernel_->ref(UNIQUE_CLIENT_TAG)) {
    return true;
  }

  write_transaction_->SaveOriginal(kernel_);
  ScopedKernelLock lock(dir());
  // Make sure your new value is not in there already.
  if (dir()->kernel_->client_tags_map.find(new_tag) !=
      dir()->kernel_->client_tags_map.end()) {
    DVLOG(1) << "Detected duplicate client tag";
    return false;
  }
  dir()->kernel_->client_tags_map.erase(
      kernel_->ref(UNIQUE_CLIENT_TAG));
  kernel_->put(UNIQUE_CLIENT_TAG, new_tag);
  kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  if (!new_tag.empty()) {
    dir()->kernel_->client_tags_map[new_tag] = kernel_;
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

  if (!kernel_->ref(UNIQUE_BOOKMARK_TAG).empty() &&
      tag != kernel_->ref(UNIQUE_BOOKMARK_TAG)) {
    // There is only one scenario where our tag is expected to change.  That
    // scenario occurs when our current tag is a non-correct tag assigned during
    // the UniquePosition migration.
    std::string migration_generated_tag =
        GenerateSyncableBookmarkHash(std::string(),
                                     kernel_->ref(ID).GetServerId());
    DCHECK_EQ(migration_generated_tag, kernel_->ref(UNIQUE_BOOKMARK_TAG));
  }

  kernel_->put(UNIQUE_BOOKMARK_TAG, tag);
  kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
}

void MutableEntry::PutSpecifics(const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  CHECK(!value.password().has_client_only_encrypted_data());
  write_transaction_->SaveOriginal(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(SPECIFICS).SerializeAsString() !=
      value.SerializeAsString()) {
    kernel_->put(SPECIFICS, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerSpecifics(const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  CHECK(!value.password().has_client_only_encrypted_data());
  write_transaction_->SaveOriginal(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(SERVER_SPECIFICS).SerializeAsString() !=
      value.SerializeAsString()) {
    if (kernel_->ref(IS_UNAPPLIED_UPDATE)) {
      // Remove ourselves from unapplied_update_metahandles with our
      // old server type.
      const ModelType old_server_type = kernel_->GetServerModelType();
      const int64 metahandle = kernel_->ref(META_HANDLE);
      size_t erase_count =
          dir()->kernel_->unapplied_update_metahandles[old_server_type]
          .erase(metahandle);
      DCHECK_EQ(erase_count, 1u);
    }

    kernel_->put(SERVER_SPECIFICS, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);

    if (kernel_->ref(IS_UNAPPLIED_UPDATE)) {
      // Add ourselves back into unapplied_update_metahandles with our
      // new server type.
      const ModelType new_server_type = kernel_->GetServerModelType();
      const int64 metahandle = kernel_->ref(META_HANDLE);
      dir()->kernel_->unapplied_update_metahandles[new_server_type]
          .insert(metahandle);
    }
  }
}

void MutableEntry::PutBaseServerSpecifics(
    const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  CHECK(!value.password().has_client_only_encrypted_data());
  write_transaction_->SaveOriginal(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(BASE_SERVER_SPECIFICS).SerializeAsString()
      != value.SerializeAsString()) {
    kernel_->put(BASE_SERVER_SPECIFICS, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutUniquePosition(const UniquePosition& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if(!kernel_->ref(UNIQUE_POSITION).Equals(value)) {
    // We should never overwrite a valid position with an invalid one.
    DCHECK(value.IsValid());
    ScopedKernelLock lock(dir());
    ScopedParentChildIndexUpdater updater(
        lock, kernel_, &dir()->kernel_->parent_child_index);
    kernel_->put(UNIQUE_POSITION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutServerUniquePosition(const UniquePosition& value) {
  DCHECK(kernel_);
  write_transaction_->SaveOriginal(kernel_);
  if(!kernel_->ref(SERVER_UNIQUE_POSITION).Equals(value)) {
    // We should never overwrite a valid position with an invalid one.
    DCHECK(value.IsValid());
    ScopedKernelLock lock(dir());
    kernel_->put(SERVER_UNIQUE_POSITION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutSyncing(bool value) {
  kernel_->put(SYNCING, value);
}

void MutableEntry::PutParentIdPropertyOnly(const Id& parent_id) {
  write_transaction_->SaveOriginal(kernel_);
  dir()->ReindexParentId(write_transaction(), kernel_, parent_id);
  kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
}

MetahandleSet* MutableEntry::GetDirtyIndexHelper() {
  return &dir()->kernel_->dirty_metahandles;
}

bool MutableEntry::PutPredecessor(const Id& predecessor_id) {
  MutableEntry predecessor(write_transaction_, GET_BY_ID, predecessor_id);
  if (!predecessor.good())
    return false;
  dir()->PutPredecessor(kernel_, predecessor.kernel_);
  return true;
}

void MutableEntry::UpdateTransactionVersion(int64 value) {
  ScopedKernelLock lock(dir());
  kernel_->put(TRANSACTION_VERSION, value);
  kernel_->mark_dirty(&(dir()->kernel_->dirty_metahandles));
}

// This function sets only the flags needed to get this entry to sync.
bool MarkForSyncing(MutableEntry* e) {
  DCHECK_NE(static_cast<MutableEntry*>(NULL), e);
  DCHECK(!e->IsRoot()) << "We shouldn't mark a permanent object for syncing.";
  if (!(e->PutIsUnsynced(true)))
    return false;
  e->PutSyncing(false);
  return true;
}

}  // namespace syncable
}  // namespace syncer
