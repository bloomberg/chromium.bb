// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_TEST_ENTRY_FACTORY_H_
#define SYNC_TEST_TEST_ENTRY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/protocol/sync.pb.h"

namespace syncable {
class Directory;
class Id;
}

namespace syncer {

class TestEntryFactory {
 public:
  explicit TestEntryFactory(syncable::Directory* dir);
  ~TestEntryFactory();

  // Create a new unapplied folder node with a parent.
  void CreateUnappliedNewItemWithParent(
      const std::string& item_id,
      const sync_pb::EntitySpecifics& specifics,
      const std::string& parent_id);

  // Create a new unapplied update without a parent.
  void CreateUnappliedNewItem(const std::string& item_id,
                              const sync_pb::EntitySpecifics& specifics,
                              bool is_unique);

  // Create an unsynced item in the database.  If item_id is a local ID, it will
  // be treated as a create-new.  Otherwise, if it's a server ID, we'll fake the
  // server data so that it looks like it exists on the server.  Returns the
  // methandle of the created item in |metahandle_out| if not NULL.
  void CreateUnsyncedItem(const syncable::Id& item_id,
                          const syncable::Id& parent_id,
                          const std::string& name,
                          bool is_folder,
                          syncable::ModelType model_type,
                          int64* metahandle_out);

  // Creates an item that is both unsynced an an unapplied update.  Returns the
  // metahandle of the created item.
  int64 CreateUnappliedAndUnsyncedItem(const std::string& name,
                                       syncable::ModelType model_type);

  // Creates an item that has neither IS_UNSYNED or IS_UNAPPLIED_UPDATE.  The
  // item is known to both the server and client.  Returns the metahandle of
  // the created item.
  int64 CreateSyncedItem(const std::string& name, syncable::ModelType
                         model_type, bool is_folder);

  int64 GetNextRevision();

 private:
  syncable::Directory* directory_;
  int64 next_revision_;

  DISALLOW_COPY_AND_ASSIGN(TestEntryFactory);
};

}  // namespace syncer

#endif  // SYNC_TEST_TEST_ENTRY_FACTORY_H_
