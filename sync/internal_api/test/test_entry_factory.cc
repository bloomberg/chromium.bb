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
  entry.Put(syncable::SERVER_VERSION, GetNextRevision());
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);

  entry.Put(syncable::SERVER_NON_UNIQUE_NAME, item_id);
  entry.Put(syncable::SERVER_PARENT_ID, Id::CreateFromServerId(parent_id));
  entry.Put(syncable::SERVER_IS_DIR, true);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  return entry.Get(syncable::META_HANDLE);
}

int64 TestEntryFactory::CreateUnappliedNewBookmarkItemWithParent(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    const string& parent_id) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
      Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.Put(syncable::SERVER_VERSION, GetNextRevision());
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);

  entry.Put(syncable::SERVER_NON_UNIQUE_NAME, item_id);
  entry.Put(syncable::SERVER_PARENT_ID, Id::CreateFromServerId(parent_id));
  entry.Put(syncable::SERVER_IS_DIR, true);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);

  return entry.Get(syncable::META_HANDLE);
}

int64 TestEntryFactory::CreateUnappliedNewItem(
    const string& item_id,
    const sync_pb::EntitySpecifics& specifics,
    bool is_unique) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::CREATE_NEW_UPDATE_ITEM,
      Id::CreateFromServerId(item_id));
  DCHECK(entry.good());
  entry.Put(syncable::SERVER_VERSION, GetNextRevision());
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  entry.Put(syncable::SERVER_NON_UNIQUE_NAME, item_id);
  entry.Put(syncable::SERVER_PARENT_ID, syncable::GetNullId());
  entry.Put(syncable::SERVER_IS_DIR, is_unique);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  if (is_unique) { // For top-level nodes.
    entry.Put(syncable::UNIQUE_SERVER_TAG,
              ModelTypeToRootTag(GetModelTypeFromSpecifics(specifics)));
  }
  return entry.Get(syncable::META_HANDLE);
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
  entry.Put(syncable::ID, item_id);
  entry.Put(syncable::BASE_VERSION,
      item_id.ServerKnows() ? GetNextRevision() : 0);
  entry.Put(syncable::IS_UNSYNCED, true);
  entry.Put(syncable::IS_DIR, is_folder);
  entry.Put(syncable::IS_DEL, false);
  entry.Put(syncable::PARENT_ID, parent_id);
  sync_pb::EntitySpecifics default_specifics;
  AddDefaultFieldValue(model_type, &default_specifics);
  entry.Put(syncable::SPECIFICS, default_specifics);

  if (item_id.ServerKnows()) {
    entry.Put(syncable::SERVER_SPECIFICS, default_specifics);
    entry.Put(syncable::SERVER_IS_DIR, false);
    entry.Put(syncable::SERVER_PARENT_ID, parent_id);
    entry.Put(syncable::SERVER_IS_DEL, false);
  }
  if (metahandle_out)
    *metahandle_out = entry.Get(syncable::META_HANDLE);
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

  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  entry.Put(syncable::SERVER_VERSION, GetNextRevision());

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

  entry.Put(syncable::ID, item_id);
  entry.Put(syncable::BASE_VERSION, version);
  entry.Put(syncable::IS_UNSYNCED, false);
  entry.Put(syncable::NON_UNIQUE_NAME, name);
  entry.Put(syncable::IS_DIR, is_folder);
  entry.Put(syncable::IS_DEL, false);
  entry.Put(syncable::PARENT_ID, parent_id);

  entry.Put(syncable::SERVER_VERSION, GetNextRevision());
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, false);
  entry.Put(syncable::SERVER_NON_UNIQUE_NAME, name);
  entry.Put(syncable::SERVER_PARENT_ID, parent_id);
  entry.Put(syncable::SERVER_IS_DIR, is_folder);
  entry.Put(syncable::SERVER_IS_DEL, false);
  entry.Put(syncable::SERVER_SPECIFICS, entry.Get(syncable::SPECIFICS));

  return entry.Get(syncable::META_HANDLE);
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
  entry.Put(syncable::SERVER_VERSION, 1);
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  entry.Put(syncable::SERVER_IS_DIR, false);
  entry.Put(syncable::SERVER_PARENT_ID, TestIdFactory::root());
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  entry.Put(syncable::NON_UNIQUE_NAME, "xyz");

  return entry.Get(syncable::META_HANDLE);
}

bool TestEntryFactory::SetServerSpecificsForItem(
    int64 meta_handle,
    const sync_pb::EntitySpecifics specifics) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  MutableEntry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    return false;
  }
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
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
  entry.Put(syncable::SPECIFICS, specifics);
  entry.Put(syncable::IS_UNSYNCED, true);
  return true;
}

const sync_pb::EntitySpecifics& TestEntryFactory::GetServerSpecificsForItem(
    int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.Get(syncable::SERVER_SPECIFICS);
}

const sync_pb::EntitySpecifics& TestEntryFactory::GetLocalSpecificsForItem(
    int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  DCHECK(entry.good());
  return entry.Get(syncable::SPECIFICS);
}

bool TestEntryFactory::GetIsUnsyncedForItem(int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    NOTREACHED();
    return false;
  }
  return entry.Get(syncable::IS_UNSYNCED);
}

bool TestEntryFactory::GetIsUnappliedForItem(int64 meta_handle) const {
  syncable::ReadTransaction trans(FROM_HERE, directory_);
  syncable::Entry entry(&trans, syncable::GET_BY_HANDLE, meta_handle);
  if (!entry.good()) {
    NOTREACHED();
    return false;
  }
  return entry.Get(syncable::IS_UNAPPLIED_UPDATE);
}

int64 TestEntryFactory::GetNextRevision() {
  return next_revision_++;
}

}  // namespace syncer
