// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/model_neutral_mutable_entry.h"

#include <string>

#include "sync/internal_api/public/base/unique_position.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/scoped_kernel_lock.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"

using std::string;

namespace syncer {

namespace syncable {

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans,
                                                   CreateNewUpdateItem,
                                                   const Id& id)
    : Entry(trans), base_write_transaction_(trans) {
  Entry same_id(trans, GET_BY_ID, id);
  kernel_ = NULL;
  if (same_id.good()) {
    return;  // already have an item with this ID.
  }
  scoped_ptr<EntryKernel> kernel(new EntryKernel());

  kernel->put(ID, id);
  kernel->put(META_HANDLE, trans->directory()->NextMetahandle());
  kernel->mark_dirty(&trans->directory()->kernel_->dirty_metahandles);
  kernel->put(IS_DEL, true);
  // We match the database defaults here
  kernel->put(BASE_VERSION, CHANGES_VERSION);
  if (!trans->directory()->InsertEntry(trans, kernel.get())) {
    return;  // Failed inserting.
  }
  trans->TrackChangesTo(kernel.get());

  kernel_ = kernel.release();
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(
    BaseWriteTransaction* trans, GetById, const Id& id)
    : Entry(trans, GET_BY_ID, id), base_write_transaction_(trans) {
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(
    BaseWriteTransaction* trans, GetByHandle, int64 metahandle)
    : Entry(trans, GET_BY_HANDLE, metahandle), base_write_transaction_(trans) {
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(
    BaseWriteTransaction* trans, GetByClientTag, const std::string& tag)
    : Entry(trans, GET_BY_CLIENT_TAG, tag), base_write_transaction_(trans) {
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(
    BaseWriteTransaction* trans, GetTypeRoot, ModelType type)
    : Entry(trans, GET_TYPE_ROOT, type), base_write_transaction_(trans) {
}

void ModelNeutralMutableEntry::PutBaseVersion(int64 value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if (kernel_->ref(BASE_VERSION) != value) {
    kernel_->put(BASE_VERSION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void ModelNeutralMutableEntry::PutServerVersion(int64 value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if (kernel_->ref(SERVER_VERSION) != value) {
    ScopedKernelLock lock(dir());
    kernel_->put(SERVER_VERSION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void ModelNeutralMutableEntry::PutServerMtime(base::Time value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if (kernel_->ref(SERVER_MTIME) != value) {
    kernel_->put(SERVER_MTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void ModelNeutralMutableEntry::PutServerCtime(base::Time value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if (kernel_->ref(SERVER_CTIME) != value) {
    kernel_->put(SERVER_CTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

bool ModelNeutralMutableEntry::PutId(const Id& value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if (kernel_->ref(ID) != value) {
    if (!dir()->ReindexId(base_write_transaction(), kernel_, value))
      return false;
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void ModelNeutralMutableEntry::PutServerParentId(const Id& value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);

  if (kernel_->ref(SERVER_PARENT_ID) != value) {
    kernel_->put(SERVER_PARENT_ID, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

bool ModelNeutralMutableEntry::PutIsUnsynced(bool value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if (kernel_->ref(IS_UNSYNCED) != value) {
    MetahandleSet* index = &dir()->kernel_->unsynced_metahandles;

    ScopedKernelLock lock(dir());
    if (value) {
      if (!SyncAssert(index->insert(kernel_->ref(META_HANDLE)).second,
                      FROM_HERE,
                      "Could not insert",
                      base_write_transaction())) {
        return false;
      }
    } else {
      if (!SyncAssert(1U == index->erase(kernel_->ref(META_HANDLE)),
                      FROM_HERE,
                      "Entry Not succesfully erased",
                      base_write_transaction())) {
        return false;
      }
    }
    kernel_->put(IS_UNSYNCED, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
  return true;
}

bool ModelNeutralMutableEntry::PutIsUnappliedUpdate(bool value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
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
                      base_write_transaction())) {
        return false;
      }
    } else {
      if (!SyncAssert(1U == index->erase(kernel_->ref(META_HANDLE)),
                      FROM_HERE,
                      "Entry Not succesfully erased",
                      base_write_transaction())) {
        return false;
      }
    }
    kernel_->put(IS_UNAPPLIED_UPDATE, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
  return true;
}

void ModelNeutralMutableEntry::PutServerIsDir(bool value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  bool old_value = kernel_->ref(SERVER_IS_DIR);
  if (old_value != value) {
    kernel_->put(SERVER_IS_DIR, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }
}

void ModelNeutralMutableEntry::PutServerIsDel(bool value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
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
      base_write_transaction(), old_value, *kernel_);
}

void ModelNeutralMutableEntry::PutServerNonUniqueName(
    const std::string& value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);

  if (kernel_->ref(SERVER_NON_UNIQUE_NAME) != value) {
    kernel_->put(SERVER_NON_UNIQUE_NAME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

bool ModelNeutralMutableEntry::PutUniqueServerTag(const string& new_tag) {
  if (new_tag == kernel_->ref(UNIQUE_SERVER_TAG)) {
    return true;
  }

  base_write_transaction_->TrackChangesTo(kernel_);
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

bool ModelNeutralMutableEntry::PutUniqueClientTag(const string& new_tag) {
  if (new_tag == kernel_->ref(UNIQUE_CLIENT_TAG)) {
    return true;
  }

  base_write_transaction_->TrackChangesTo(kernel_);
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

void ModelNeutralMutableEntry::PutUniqueBookmarkTag(const std::string& tag) {
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

void ModelNeutralMutableEntry::PutServerSpecifics(
    const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  CHECK(!value.password().has_client_only_encrypted_data());
  base_write_transaction_->TrackChangesTo(kernel_);
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

void ModelNeutralMutableEntry::PutBaseServerSpecifics(
    const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  CHECK(!value.password().has_client_only_encrypted_data());
  base_write_transaction_->TrackChangesTo(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(BASE_SERVER_SPECIFICS).SerializeAsString()
      != value.SerializeAsString()) {
    kernel_->put(BASE_SERVER_SPECIFICS, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void ModelNeutralMutableEntry::PutServerUniquePosition(
    const UniquePosition& value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);
  if(!kernel_->ref(SERVER_UNIQUE_POSITION).Equals(value)) {
    // We should never overwrite a valid position with an invalid one.
    DCHECK(value.IsValid());
    ScopedKernelLock lock(dir());
    kernel_->put(SERVER_UNIQUE_POSITION, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void ModelNeutralMutableEntry::PutServerAttachmentMetadata(
    const sync_pb::AttachmentMetadata& value) {
  DCHECK(kernel_);
  base_write_transaction_->TrackChangesTo(kernel_);

  if (kernel_->ref(SERVER_ATTACHMENT_METADATA).SerializeAsString() !=
      value.SerializeAsString()) {
    kernel_->put(SERVER_ATTACHMENT_METADATA, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void ModelNeutralMutableEntry::PutSyncing(bool value) {
  kernel_->put(SYNCING, value);
}

void ModelNeutralMutableEntry::PutParentIdPropertyOnly(const Id& parent_id) {
  base_write_transaction_->TrackChangesTo(kernel_);
  dir()->ReindexParentId(base_write_transaction(), kernel_, parent_id);
  kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
}

void ModelNeutralMutableEntry::UpdateTransactionVersion(int64 value) {
  ScopedKernelLock lock(dir());
  kernel_->put(TRANSACTION_VERSION, value);
  kernel_->mark_dirty(&(dir()->kernel_->dirty_metahandles));
}

ModelNeutralMutableEntry::ModelNeutralMutableEntry(BaseWriteTransaction* trans)
  : Entry(trans), base_write_transaction_(trans) {}

MetahandleSet* ModelNeutralMutableEntry::GetDirtyIndexHelper() {
  return &dir()->kernel_->dirty_metahandles;
}

}  // namespace syncable

}  // namespace syncer
