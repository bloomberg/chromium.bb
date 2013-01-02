// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_MUTABLE_ENTRY_H_
#define SYNC_SYNCABLE_MUTABLE_ENTRY_H_

#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/node_ordinal.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/metahandle_set.h"

namespace syncer {
class WriteNode;

namespace syncable {

class WriteTransaction;

enum Create {
  CREATE
};

enum CreateNewUpdateItem {
  CREATE_NEW_UPDATE_ITEM
};

// A mutable meta entry.  Changes get committed to the database when the
// WriteTransaction is destroyed.
class SYNC_EXPORT_PRIVATE MutableEntry : public Entry {
  void Init(WriteTransaction* trans, const Id& parent_id,
      const std::string& name);

 public:
  MutableEntry(WriteTransaction* trans, Create, const Id& parent_id,
               const std::string& name);
  MutableEntry(WriteTransaction* trans, CreateNewUpdateItem, const Id& id);
  MutableEntry(WriteTransaction* trans, GetByHandle, int64);
  MutableEntry(WriteTransaction* trans, GetById, const Id&);
  MutableEntry(WriteTransaction* trans, GetByClientTag, const std::string& tag);
  MutableEntry(WriteTransaction* trans, GetByServerTag, const std::string& tag);

  inline WriteTransaction* write_transaction() const {
    return write_transaction_;
  }

  // Field Accessors.  Some of them trigger the re-indexing of the entry.
  // Return true on success, return false on failure, which means
  // that putting the value would have caused a duplicate in the index.
  // TODO(chron): Remove some of these unecessary return values.
  bool Put(Int64Field field, const int64& value);
  bool Put(TimeField field, const base::Time& value);
  bool Put(IdField field, const Id& value);
  bool Put(OrdinalField field, const NodeOrdinal& value);

  // Do a simple property-only update if the PARENT_ID field.  Use with caution.
  //
  // The normal Put(IS_PARENT) call will move the item to the front of the
  // sibling order to maintain the linked list invariants when the parent
  // changes.  That's usually what you want to do, but it's inappropriate
  // when the caller is trying to change the parent ID of a the whole set
  // of children (e.g. because the ID changed during a commit).  For those
  // cases, there's this function.  It will corrupt the sibling ordering
  // if you're not careful.
  void PutParentIdPropertyOnly(const Id& parent_id);

  bool Put(StringField field, const std::string& value);
  bool Put(BaseVersion field, int64 value);

  bool Put(ProtoField field, const sync_pb::EntitySpecifics& value);
  bool Put(BitField field, bool value);
  inline bool Put(IsDelField field, bool value) {
    return PutIsDel(value);
  }
  bool Put(IndexedBitField field, bool value);

  // Sets the position of this item, and updates the entry kernels of the
  // adjacent siblings so that list invariants are maintained.  Returns false
  // and fails if |predecessor_id| does not identify a sibling.  Pass the root
  // ID to put the node in first position.
  bool PutPredecessor(const Id& predecessor_id);

  bool Put(BitTemp field, bool value);

 protected:
  syncable::MetahandleSet* GetDirtyIndexHelper();

  bool PutIsDel(bool value);

 private:
  friend class Directory;
  friend class WriteTransaction;
  friend class syncer::WriteNode;

  // Don't allow creation on heap, except by sync API wrappers.
  void* operator new(size_t size) { return (::operator new)(size); }

  bool PutUniqueClientTag(const std::string& value);

  // Adjusts the successor and predecessor entries so that they no longer
  // refer to this entry.
  bool UnlinkFromOrder();

  // Kind of redundant. We should reduce the number of pointers
  // floating around if at all possible. Could we store this in Directory?
  // Scope: Set on construction, never changed after that.
  WriteTransaction* const write_transaction_;

 protected:
  MutableEntry();

  DISALLOW_COPY_AND_ASSIGN(MutableEntry);
};

// This function sets only the flags needed to get this entry to sync.
bool MarkForSyncing(syncable::MutableEntry* e);

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_MUTABLE_ENTRY_H_
