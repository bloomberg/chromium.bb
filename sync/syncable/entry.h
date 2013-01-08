// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_ENTRY_H_
#define SYNC_SYNCABLE_ENTRY_H_

#include "sync/base/sync_export.h"
#include "sync/syncable/entry_kernel.h"

namespace syncer {
class Cryptographer;
class ReadNode;

namespace syncable {

class Directory;
class BaseTransaction;

// A read-only meta entry
// Instead of:
//   Entry e = transaction.GetById(id);
// use:
//   Entry e(transaction, GET_BY_ID, id);
//
// Why?  The former would require a copy constructor, and it would be difficult
// to enforce that an entry never outlived its transaction if there were a copy
// constructor.
enum GetById {
  GET_BY_ID
};

enum GetByClientTag {
  GET_BY_CLIENT_TAG
};

enum GetByServerTag {
  GET_BY_SERVER_TAG
};

enum GetByHandle {
  GET_BY_HANDLE
};

class SYNC_EXPORT Entry {
 public:
  // After constructing, you must check good() to test whether the Get
  // succeeded.
  Entry(BaseTransaction* trans, GetByHandle, int64 handle);
  Entry(BaseTransaction* trans, GetById, const Id& id);
  Entry(BaseTransaction* trans, GetByServerTag, const std::string& tag);
  Entry(BaseTransaction* trans, GetByClientTag, const std::string& tag);

  bool good() const { return 0 != kernel_; }

  BaseTransaction* trans() const { return basetrans_; }

  // Field accessors.
  inline int64 Get(MetahandleField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline Id Get(IdField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline int64 Get(Int64Field field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline const base::Time& Get(TimeField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline int64 Get(BaseVersion field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(IndexedBitField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(IsDelField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(BitField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  const std::string& Get(StringField field) const;
  inline const sync_pb::EntitySpecifics& Get(ProtoField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline const NodeOrdinal& Get(OrdinalField field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }
  inline bool Get(BitTemp field) const {
    DCHECK(kernel_);
    return kernel_->ref(field);
  }

  ModelType GetServerModelType() const;
  ModelType GetModelType() const;

  Id GetPredecessorId() const;
  Id GetSuccessorId() const;

  inline bool ExistsOnClientBecauseNameIsNonEmpty() const {
    DCHECK(kernel_);
    return !kernel_->ref(NON_UNIQUE_NAME).empty();
  }

  inline bool IsRoot() const {
    DCHECK(kernel_);
    return kernel_->ref(ID).IsRoot();
  }

  Directory* dir() const;

  const EntryKernel GetKernelCopy() const {
    return *kernel_;
  }

  // Compute a local predecessor position for |update_item|, based on its
  // absolute server position.  The returned ID will be a valid predecessor
  // under SERVER_PARENT_ID that is consistent with the
  // SERVER_POSITION_IN_PARENT ordering.
  Id ComputePrevIdFromServerPosition(const Id& parent_id) const;

  // Dumps all entry info into a DictionaryValue and returns it.
  // Transfers ownership of the DictionaryValue to the caller.
  base::DictionaryValue* ToValue(Cryptographer* cryptographer) const;

 protected:  // Don't allow creation on heap, except by sync API wrappers.
  void* operator new(size_t size) { return (::operator new)(size); }

  inline explicit Entry(BaseTransaction* trans)
      : basetrans_(trans),
        kernel_(NULL) { }

 protected:
  BaseTransaction* const basetrans_;

  EntryKernel* kernel_;

 private:
  friend class Directory;
  friend class syncer::ReadNode;
  friend std::ostream& operator << (std::ostream& s, const Entry& e);

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

std::ostream& operator<<(std::ostream& os, const Entry& entry);

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_ENTRY_H_
