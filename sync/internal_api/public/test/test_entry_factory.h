// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_TEST_ENTRY_FACTORY_H_
#define SYNC_TEST_TEST_ENTRY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncer {

namespace syncable {
class Directory;
class Id;
}

class TestEntryFactory {
 public:
  explicit TestEntryFactory(syncable::Directory* dir);
  ~TestEntryFactory();

  // Create a new unapplied folder node with a parent.
  int64 CreateUnappliedNewItemWithParent(
      const std::string& item_id,
      const sync_pb::EntitySpecifics& specifics,
      const std::string& parent_id);

  int64 CreateUnappliedNewBookmarkItemWithParent(
      const std::string& item_id,
      const sync_pb::EntitySpecifics& specifics,
      const std::string& parent_id);

  // Create a new unapplied update without a parent.
  int64 CreateUnappliedNewItem(const std::string& item_id,
                               const sync_pb::EntitySpecifics& specifics,
                               bool is_unique);

  // Create an unsynced unique_client_tag item in the database.  If item_id is a
  // local ID, it will be treated as a create-new.  Otherwise, if it's a server
  // ID, we'll fake the server data so that it looks like it exists on the
  // server.  Returns the methandle of the created item in |metahandle_out| if
  // not NULL.
  void CreateUnsyncedItem(const syncable::Id& item_id,
                          const syncable::Id& parent_id,
                          const std::string& name,
                          bool is_folder,
                          ModelType model_type,
                          int64* metahandle_out);

  // Creates a bookmark that is both unsynced an an unapplied update.  Returns
  // the metahandle of the created item.
  int64 CreateUnappliedAndUnsyncedBookmarkItem(const std::string& name);

  // Creates a unique_client_tag item that has neither IS_UNSYNED or
  // IS_UNAPPLIED_UPDATE.  The item is known to both the server and client.
  // Returns the metahandle of the created item.
  int64 CreateSyncedItem(const std::string& name,
                         ModelType model_type, bool is_folder);

  // Creates a root node that IS_UNAPPLIED. Smiilar to what one would find in
  // the database between the ProcessUpdates of an initial datatype configure
  // cycle and the ApplyUpdates step of the same sync cycle.
  int64 CreateUnappliedRootNode(ModelType model_type);

  // Looks up the item referenced by |meta_handle|. If successful, overwrites
  // the server specifics with |specifics|, sets
  // IS_UNAPPLIED_UPDATES/IS_UNSYNCED appropriately, and returns true.
  // Else, return false.
  bool SetServerSpecificsForItem(int64 meta_handle,
                                 const sync_pb::EntitySpecifics specifics);

  // Looks up the item referenced by |meta_handle|. If successful, overwrites
  // the local specifics with |specifics|, sets
  // IS_UNAPPLIED_UPDATES/IS_UNSYNCED appropriately, and returns true.
  // Else, return false.
  bool SetLocalSpecificsForItem(int64 meta_handle,
                                const sync_pb::EntitySpecifics specifics);

  // Looks up the item referenced by |meta_handle|. If successful, stores
  // the server specifics into |specifics| and returns true. Else, return false.
  const sync_pb::EntitySpecifics& GetServerSpecificsForItem(
      int64 meta_handle) const;

  // Looks up the item referenced by |meta_handle|. If successful, stores
  // the local specifics into |specifics| and returns true. Else, return false.
  const sync_pb::EntitySpecifics& GetLocalSpecificsForItem(
      int64 meta_handle) const;

  // Getters for IS_UNSYNCED and IS_UNAPPLIED_UPDATE bit fields.
  bool GetIsUnsyncedForItem(int64 meta_handle) const;
  bool GetIsUnappliedForItem(int64 meta_handle) const;

  int64 GetNextRevision();

 private:
  syncable::Directory* directory_;
  int64 next_revision_;

  DISALLOW_COPY_AND_ASSIGN(TestEntryFactory);
};

}  // namespace syncer

#endif  // SYNC_TEST_TEST_ENTRY_FACTORY_H_
