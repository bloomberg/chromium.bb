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
  trans->TrackChangesTo(kernel.get());
  kernel->put(IS_DEL, false);

  // Now swap the pointers.
  kernel_ = kernel.release();
}

MutableEntry::MutableEntry(WriteTransaction* trans,
                           Create,
                           ModelType model_type,
                           const Id& parent_id,
                           const string& name)
    : ModelNeutralMutableEntry(trans), write_transaction_(trans) {
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
    : ModelNeutralMutableEntry(trans, CREATE_NEW_UPDATE_ITEM, id),
      write_transaction_(trans) {}

MutableEntry::MutableEntry(WriteTransaction* trans, GetById, const Id& id)
    : ModelNeutralMutableEntry(trans, GET_BY_ID, id),
      write_transaction_(trans) {
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByHandle,
                           int64 metahandle)
    : ModelNeutralMutableEntry(trans, GET_BY_HANDLE, metahandle),
      write_transaction_(trans) {
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetByClientTag,
                           const std::string& tag)
    : ModelNeutralMutableEntry(trans, GET_BY_CLIENT_TAG, tag),
      write_transaction_(trans) {
}

MutableEntry::MutableEntry(WriteTransaction* trans, GetTypeRoot, ModelType type)
    : ModelNeutralMutableEntry(trans, GET_TYPE_ROOT, type),
      write_transaction_(trans) {
}

void MutableEntry::PutLocalExternalId(int64 value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
  if (kernel_->ref(LOCAL_EXTERNAL_ID) != value) {
    ScopedKernelLock lock(dir());
    kernel_->put(LOCAL_EXTERNAL_ID, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutMtime(base::Time value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
  if (kernel_->ref(MTIME) != value) {
    kernel_->put(MTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutCtime(base::Time value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
  if (kernel_->ref(CTIME) != value) {
    kernel_->put(CTIME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutParentId(const Id& value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
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

void MutableEntry::PutIsDir(bool value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
  bool old_value = kernel_->ref(IS_DIR);
  if (old_value != value) {
    kernel_->put(IS_DIR, value);
    kernel_->mark_dirty(GetDirtyIndexHelper());
  }
}

void MutableEntry::PutIsDel(bool value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
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

void MutableEntry::PutNonUniqueName(const std::string& value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);

  if (kernel_->ref(NON_UNIQUE_NAME) != value) {
    kernel_->put(NON_UNIQUE_NAME, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutSpecifics(const sync_pb::EntitySpecifics& value) {
  DCHECK(kernel_);
  CHECK(!value.password().has_client_only_encrypted_data());
  write_transaction()->TrackChangesTo(kernel_);
  // TODO(ncarter): This is unfortunately heavyweight.  Can we do
  // better?
  if (kernel_->ref(SPECIFICS).SerializeAsString() !=
      value.SerializeAsString()) {
    kernel_->put(SPECIFICS, value);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::PutUniquePosition(const UniquePosition& value) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
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

bool MutableEntry::PutPredecessor(const Id& predecessor_id) {
  MutableEntry predecessor(write_transaction(), GET_BY_ID, predecessor_id);
  if (!predecessor.good())
    return false;
  dir()->PutPredecessor(kernel_, predecessor.kernel_);
  return true;
}

void MutableEntry::PutAttachmentMetadata(
    const sync_pb::AttachmentMetadata& attachment_metadata) {
  DCHECK(kernel_);
  write_transaction()->TrackChangesTo(kernel_);
  if (kernel_->ref(ATTACHMENT_METADATA).SerializeAsString() !=
      attachment_metadata.SerializeAsString()) {
    dir()->UpdateAttachmentIndex(GetMetahandle(),
                                 kernel_->ref(ATTACHMENT_METADATA),
                                 attachment_metadata);
    kernel_->put(ATTACHMENT_METADATA, attachment_metadata);
    kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  }
}

void MutableEntry::MarkAttachmentAsOnServer(
    const sync_pb::AttachmentIdProto& attachment_id) {
  DCHECK(kernel_);
  DCHECK(!attachment_id.unique_id().empty());
  write_transaction()->TrackChangesTo(kernel_);
  sync_pb::AttachmentMetadata& attachment_metadata =
      kernel_->mutable_ref(ATTACHMENT_METADATA);
  for (int i = 0; i < attachment_metadata.record_size(); ++i) {
    sync_pb::AttachmentMetadataRecord* record =
        attachment_metadata.mutable_record(i);
    if (record->id().unique_id() != attachment_id.unique_id())
      continue;
    record->set_is_on_server(true);
  }
  kernel_->mark_dirty(&dir()->kernel_->dirty_metahandles);
  MarkForSyncing(this);
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
