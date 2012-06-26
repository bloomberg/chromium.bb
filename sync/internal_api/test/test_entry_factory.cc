// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/test_entry_factory.h"

#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/write_transaction.h"
#include "sync/test/engine/test_id_factory.h"

using std::string;
using syncable::Id;
using syncable::MutableEntry;
using syncable::UNITTEST;
using syncable::WriteTransaction;

namespace csync {

TestEntryFactory::TestEntryFactory(syncable::Directory *dir)
    : directory_(dir), next_revision_(1) {
}

TestEntryFactory::~TestEntryFactory() { }

void TestEntryFactory::CreateUnappliedNewItemWithParent(
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
}

void TestEntryFactory::CreateUnappliedNewItem(
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
  entry.Put(syncable::SERVER_IS_DIR, false);
  entry.Put(syncable::SERVER_SPECIFICS, specifics);
  if (is_unique)  // For top-level nodes.
    entry.Put(syncable::UNIQUE_SERVER_TAG, item_id);
}

void TestEntryFactory::CreateUnsyncedItem(
    const Id& item_id,
    const Id& parent_id,
    const string& name,
    bool is_folder,
    syncable::ModelType model_type,
    int64* metahandle_out) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);
  Id predecessor_id;
  DCHECK(
      directory_->GetLastChildIdForTest(&trans, parent_id, &predecessor_id));
  MutableEntry entry(&trans, syncable::CREATE, parent_id, name);
  DCHECK(entry.good());
  entry.Put(syncable::ID, item_id);
  entry.Put(syncable::BASE_VERSION,
      item_id.ServerKnows() ? GetNextRevision() : 0);
  entry.Put(syncable::IS_UNSYNCED, true);
  entry.Put(syncable::IS_DIR, is_folder);
  entry.Put(syncable::IS_DEL, false);
  entry.Put(syncable::PARENT_ID, parent_id);
  CHECK(entry.PutPredecessor(predecessor_id));
  sync_pb::EntitySpecifics default_specifics;
  syncable::AddDefaultFieldValue(model_type, &default_specifics);
  entry.Put(syncable::SPECIFICS, default_specifics);
  if (item_id.ServerKnows()) {
    entry.Put(syncable::SERVER_SPECIFICS, default_specifics);
    entry.Put(syncable::SERVER_IS_DIR, is_folder);
    entry.Put(syncable::SERVER_PARENT_ID, parent_id);
    entry.Put(syncable::SERVER_IS_DEL, false);
  }
  if (metahandle_out)
    *metahandle_out = entry.Get(syncable::META_HANDLE);
}

int64 TestEntryFactory::CreateUnappliedAndUnsyncedItem(
    const string& name,
    syncable::ModelType model_type) {
  int64 metahandle = 0;
  CreateUnsyncedItem(
      TestIdFactory::MakeServer(name), TestIdFactory::root(),
      name, false, model_type, &metahandle);

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
    const std::string& name, syncable::ModelType
    model_type, bool is_folder) {
  WriteTransaction trans(FROM_HERE, UNITTEST, directory_);

  syncable::Id parent_id(TestIdFactory::root());
  syncable::Id item_id(TestIdFactory::MakeServer(name));
  int64 version = GetNextRevision();

  sync_pb::EntitySpecifics default_specifics;
  syncable::AddDefaultFieldValue(model_type, &default_specifics);

  MutableEntry entry(&trans, syncable::CREATE, parent_id, name);
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

  if (!entry.PutPredecessor(TestIdFactory::root())) {
    NOTREACHED();
    return syncable::kInvalidMetaHandle;
  }
  entry.Put(syncable::SPECIFICS, default_specifics);

  entry.Put(syncable::SERVER_VERSION, GetNextRevision());
  entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  entry.Put(syncable::SERVER_NON_UNIQUE_NAME, "X");
  entry.Put(syncable::SERVER_PARENT_ID, TestIdFactory::MakeServer("Y"));
  entry.Put(syncable::SERVER_IS_DIR, is_folder);
  entry.Put(syncable::SERVER_IS_DEL, false);
  entry.Put(syncable::SERVER_SPECIFICS, default_specifics);
  entry.Put(syncable::SERVER_PARENT_ID, parent_id);

  return entry.Get(syncable::META_HANDLE);
}

int64 TestEntryFactory::GetNextRevision() {
  return next_revision_++;
}

}  // namespace csync
