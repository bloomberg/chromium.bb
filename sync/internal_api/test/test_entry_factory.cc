// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/test_entry_factory.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_id_factory.h"

using std::string;

namespace syncer {

using syncable::Id;
using syncable::MutableEntry;
using syncable::UNITTEST;
using syncable::WriteTransaction;

TestEntryFactory::TestEntryFactory(syncable::Directory *dir)
    : directory_(dir), next_revision_(1) {
}

TestEntryFactory::~TestEntryFactory() { }

int64 TestEntryFactory::CreateUnappliedNewItemWithParent(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    const string& parent_id) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
      Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(true);

  entry.PutServerNonUniqueName(item_id);
  entry.PutServerParentId(Id::CreateFromServerId(parent_id));
  entry.PutServerIsDir(true);
  entry.PutServerSpecifics(specifics);
  return entry.GetMetahandle();
}

int64 TestEntryFactory::CreateUnappliedNewBookmarkItemWithParent(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    const string& parent_id) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
      Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(true);

  entry.PutServerNonUniqueName(item_id);
  entry.PutServerParentId(Id::CreateFromServerId(parent_id));
  entry.PutServerIsDir(true);
  entry.PutServerSpecifics(specifics);

  return entry.GetMetahandle();
}

int64 TestEntryFactory::CreateUnappliedNewItem(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    bool is_unique) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
      Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(true);
  entry.PutServerNonUniqueName(item_id);
  entry.PutServerParentId(syncable::GetNullId());
  entry.PutServerIsDir(is_unique);
  entry.PutServerSpecifics(specifics);
  if (is_unique) { // For top-level nodes.
    entry.PutUniqueServerTag(
              ModelTypeToRootTag(GetModelTypeFromSpecifics(specifics)));
  }
  return entry.GetMetahandle();
}

void TestEntryFactory::CreateUnsyncedItem(
    const Id& item_id,
    const Id& parent_id,
    const string& name,
    bool is_folder,
    ModelType model_type,
    int64* metahandle_out) {
  if (is_folder) {
    DCHECK_EQ(model_type, BOOKMARKS);
  }

  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);

  MutableEntry entry(&trans, syncable::CREATE, model_type, parent_id, name);
  DCHECK(entry.good());
  entry.PutId(item_id);
  entry.PutBaseVersion(
      item_id.ServerKnows() ? GetNextRevision() : 0);
  entry.PutIsUnsynced(true);
  entry.PutIsDir(is_folder);
  entry.PutIsDel(false);
  entry.PutParentId(parent_id);
  sync_pb::EntitySpecifics default_specifics;
  AddDefaultFieldValue(model_type, &default_specifics);
  entry.PutSpecifics(default_specifics);

  if (item_id.ServerKnows()) {
    entry.PutServerSpecifics(default_specifics);
    entry.PutServerIsDir(false);
    entry.PutServerParentId(parent_id);
    entry.PutServerIsDel(false);
  }
  if (metahandle_out)
    *metahandle_out = entry.GetMetahandle();
}

int64 TestEntryFactory::CreateUnappliedAndUnsyncedBookmarkItem(
    const string& name) {
  int64 metahandle = 0;
  CreateUnsyncedItem(
      TestIdFactory::MakeServer(name), TestIdFactory::root(),
      name, false, BOOKMARKS, &metahandle);

  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, metahandle);
  if (!entry.good()) {
    NOTREACHED();
    return syncable::kInvalidMetaHandle;
  }

  entry.PutIsUnappliedUpdate(true);
  entry.PutServerVersion(GetNextRevision());

  return metahandle;
}

int64 TestEntryFactory::CreateSyncedItem(
    const std::string& name, ModelType model_type, bool is_folder) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);

  syncable::Id parent_id(TestIdFactory::root());
  syncable::Id item_id(TestIdFactory::MakeServer(name));
  int64 version = GetNextRevision();

  MutableEntry entry(&trans, syncable::CREATE, model_type, parent_id, name);
  if (!entry.good()) {
    NOTREACHED();
    return syncable::kInvalidMetaHandle;
  }

  entry.PutId(item_id);
  entry.PutBaseVersion(version);
  entry.PutIsUnsynced(false);
  entry.PutNonUniqueName(name);
  entry.PutIsDir(is_folder);
  entry.PutIsDel(false);
  entry.PutParentId(parent_id);

  entry.PutServerVersion(GetNextRevision());
  entry.PutIsUnappliedUpdate(false);
  entry.PutServerNonUniqueName(name);
  entry.PutServerParentId(parent_id);
  entry.PutServerIsDir(is_folder);
  entry.PutServerIsDel(false);
  entry.PutServerSpecifics(entry.GetSpecifics());

  return entry.GetMetahandle();
}

int64 TestEntryFactory::CreateUnappliedRootNode(
    ModelType model_type) {
  syncable::WriteTransaction trans(FROM_HERE, syncable::UNITTEST, directory_);
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  syncable::Id node_id = TestIdFactory::MakeServer("xyz");
  syncable::MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
                               node_id);
  DCHECK(entry.good());
  // Make it look like sort of like a pending creation from the server.
  // The SERVER_PARENT_ID and UNIQUE_CLIENT_TAG aren't quite right, but
  // it's good enough for our purposes.
  entry.PutServerVersion(1);
  entry.PutIsUnappliedUpdate(true);
  entry.PutServerIsDir(false);
  entry.PutServerParentId(TestIdFactory::root());
  entry.PutServerSpecifics(specifics);
  entry.PutNonUniqueName("xyz");

  return entry.GetMetahandle();
}

bool TestEntryFactory::SetServerSpecificsForItem(
    int64 meta_handle,
    const sync_pb::EntitySpecifics specifics) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.PutServerSpecifics(specifics);
  entry.PutIsUnappliedUpdate(true);
  return true;
}

bool TestEntryFactory::SetLocalSpecificsForItem(
    int64 meta_handle,
    const sync_pb::EntitySpecifics specifics) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.PutSpecifics(specifics);
  entry.PutIsUnsynced(true);
  return true;
}

const sync_pb::EntitySpecifics& TestEntryFactory::GetServerSpecificsForItem(
    int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.GetServerSpecifics();
}

const sync_pb::EntitySpecifics& TestEntryFactory::GetLocalSpecificsForItem(
    int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.GetSpecifics();
}

bool TestEntryFactory::SetServerAttachmentMetadataForItem(
    int64 meta_handle,
    const sync_pb::AttachmentMetadata metadata) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.PutServerAttachmentMetadata(metadata);
  entry.PutIsUnappliedUpdate(true);
  return true;

}

bool TestEntryFactory::SetLocalAttachmentMetadataForItem(
    int64 meta_handle,
    const sync_pb::AttachmentMetadata metadata) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.PutAttachmentMetadata(metadata);
  entry.PutIsUnsynced(true);
  return true;
}

const sync_pb::AttachmentMetadata&
TestEntryFactory::GetServerAttachmentMetadataForItem(int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.GetServerAttachmentMetadata();
}

const sync_pb::AttachmentMetadata&
TestEntryFactory::GetLocalAttachmentMetadataForItem(int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.GetAttachmentMetadata();
}

bool TestEntryFactory::GetIsUnsyncedForItem(int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    NOTREACHED();
    return false;
  }
  return entry.GetIsUnsynced();
}

bool TestEntryFactory::GetIsUnappliedForItem(int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    NOTREACHED();
    return false;
  }
  return entry.GetIsUnappliedUpdate();
}

int64 TestEntryFactory::GetNextRevision() {
  return next_revision_++;
}

}  // namespace syncer
