// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_ENTRY_KERNEL_H_
#define SYNC_SYNCABLE_ENTRY_KERNEL_H_

#include <set>

#include "base/time.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/internal_api/public/util/immutable.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/metahandle_set.h"
#include "sync/syncable/syncable_id.h"
#include "sync/util/time.h"

namespace syncer {

class Cryptographer;

namespace syncable {

// Things you need to update if you change any of the fields below:
//  - EntryKernel struct in this file
//  - syncable_columns.h
//  - syncable_enum_conversions{.h,.cc,_unittest.cc}
//  - EntryKernel::EntryKernel(), EntryKernel::ToValue() in entry_kernel.cc
//  - operator<< in Entry.cc
//  - BindFields() and UnpackEntry() in directory_backing_store.cc
//  - TestSimpleFieldsPreservedDuringSaveChanges in syncable_unittest.cc

static const int64 kInvalidMetaHandle = 0;

enum {
  BEGIN_FIELDS = 0,
  INT64_FIELDS_BEGIN = BEGIN_FIELDS
};

enum MetahandleField {
  // Primary key into the table.  Keep this as a handle to the meta entry
  // across transactions.
  META_HANDLE = INT64_FIELDS_BEGIN
};

enum BaseVersion {
  // After initial upload, the version is controlled by the server, and is
  // increased whenever the data or metadata changes on the server.
  BASE_VERSION = META_HANDLE + 1,
};

enum Int64Field {
  SERVER_VERSION = BASE_VERSION + 1,
  LOCAL_EXTERNAL_ID,  // ID of an item in the external local storage that this
                      // entry is associated with. (such as bookmarks.js)
  TRANSACTION_VERSION,
  INT64_FIELDS_END
};

enum {
  INT64_FIELDS_COUNT = INT64_FIELDS_END - INT64_FIELDS_BEGIN,
  TIME_FIELDS_BEGIN = INT64_FIELDS_END,
};

enum TimeField {
  MTIME = TIME_FIELDS_BEGIN,
  SERVER_MTIME,
  CTIME,
  SERVER_CTIME,
  TIME_FIELDS_END,
};

enum {
  TIME_FIELDS_COUNT = TIME_FIELDS_END - TIME_FIELDS_BEGIN,
  ID_FIELDS_BEGIN = TIME_FIELDS_END,
};

enum IdField {
  // Code in InitializeTables relies on ID being the first IdField value.
  ID = ID_FIELDS_BEGIN,
  PARENT_ID,
  SERVER_PARENT_ID,
  ID_FIELDS_END
};

enum {
  ID_FIELDS_COUNT = ID_FIELDS_END - ID_FIELDS_BEGIN,
  BIT_FIELDS_BEGIN = ID_FIELDS_END
};

enum IndexedBitField {
  IS_UNSYNCED = BIT_FIELDS_BEGIN,
  IS_UNAPPLIED_UPDATE,
  INDEXED_BIT_FIELDS_END,
};

enum IsDelField {
  IS_DEL = INDEXED_BIT_FIELDS_END,
};

enum BitField {
  IS_DIR = IS_DEL + 1,
  SERVER_IS_DIR,
  SERVER_IS_DEL,
  BIT_FIELDS_END
};

enum {
  BIT_FIELDS_COUNT = BIT_FIELDS_END - BIT_FIELDS_BEGIN,
  STRING_FIELDS_BEGIN = BIT_FIELDS_END
};

enum StringField {
  // Name, will be truncated by server. Can be duplicated in a folder.
  NON_UNIQUE_NAME = STRING_FIELDS_BEGIN,
  // The server version of |NON_UNIQUE_NAME|.
  SERVER_NON_UNIQUE_NAME,

  // A tag string which identifies this node as a particular top-level
  // permanent object.  The tag can be thought of as a unique key that
  // identifies a singleton instance.
  UNIQUE_SERVER_TAG,  // Tagged by the server
  UNIQUE_CLIENT_TAG,  // Tagged by the client
  UNIQUE_BOOKMARK_TAG,  // Client tags for bookmark items
  STRING_FIELDS_END,
};

enum {
  STRING_FIELDS_COUNT = STRING_FIELDS_END - STRING_FIELDS_BEGIN,
  PROTO_FIELDS_BEGIN = STRING_FIELDS_END
};

// From looking at the sqlite3 docs, it's not directly stated, but it
// seems the overhead for storing a NULL blob is very small.
enum ProtoField {
  SPECIFICS = PROTO_FIELDS_BEGIN,
  SERVER_SPECIFICS,
  BASE_SERVER_SPECIFICS,
  PROTO_FIELDS_END,
};

enum {
  PROTO_FIELDS_COUNT = PROTO_FIELDS_END - PROTO_FIELDS_BEGIN,
  UNIQUE_POSITION_FIELDS_BEGIN = PROTO_FIELDS_END
};

enum UniquePositionField {
  SERVER_UNIQUE_POSITION = UNIQUE_POSITION_FIELDS_BEGIN,
  UNIQUE_POSITION,
  UNIQUE_POSITION_FIELDS_END
};

enum {
  UNIQUE_POSITION_FIELDS_COUNT =
      UNIQUE_POSITION_FIELDS_END - UNIQUE_POSITION_FIELDS_BEGIN,
  FIELD_COUNT = UNIQUE_POSITION_FIELDS_END - BEGIN_FIELDS,
  // Past this point we have temporaries, stored in memory only.
  BEGIN_TEMPS = UNIQUE_POSITION_FIELDS_END,
  BIT_TEMPS_BEGIN = BEGIN_TEMPS,
};

enum BitTemp {
  // Not to be confused with IS_UNSYNCED, this bit is used to detect local
  // changes to items that happen during the server Commit operation.
  SYNCING = BIT_TEMPS_BEGIN,
  BIT_TEMPS_END,
};

enum {
  BIT_TEMPS_COUNT = BIT_TEMPS_END - BIT_TEMPS_BEGIN
};



struct SYNC_EXPORT_PRIVATE EntryKernel {
 private:
  std::string string_fields[STRING_FIELDS_COUNT];
  sync_pb::EntitySpecifics specifics_fields[PROTO_FIELDS_COUNT];
  int64 int64_fields[INT64_FIELDS_COUNT];
  base::Time time_fields[TIME_FIELDS_COUNT];
  Id id_fields[ID_FIELDS_COUNT];
  UniquePosition unique_position_fields[UNIQUE_POSITION_FIELDS_COUNT];
  std::bitset<BIT_FIELDS_COUNT> bit_fields;
  std::bitset<BIT_TEMPS_COUNT> bit_temps;

 public:
  EntryKernel();
  ~EntryKernel();

  // Set the dirty bit, and optionally add this entry's metahandle to
  // a provided index on dirty bits in |dirty_index|. Parameter may be null,
  // and will result only in setting the dirty bit of this entry.
  inline void mark_dirty(syncable::MetahandleSet* dirty_index) {
    if (!dirty_ && dirty_index) {
      DCHECK_NE(0, ref(META_HANDLE));
      dirty_index->insert(ref(META_HANDLE));
    }
    dirty_ = true;
  }

  // Clear the dirty bit, and optionally remove this entry's metahandle from
  // a provided index on dirty bits in |dirty_index|. Parameter may be null,
  // and will result only in clearing dirty bit of this entry.
  inline void clear_dirty(syncable::MetahandleSet* dirty_index) {
    if (dirty_ && dirty_index) {
      DCHECK_NE(0, ref(META_HANDLE));
      dirty_index->erase(ref(META_HANDLE));
    }
    dirty_ = false;
  }

  inline bool is_dirty() const {
    return dirty_;
  }

  // Setters.
  inline void put(MetahandleField field, int64 value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(Int64Field field, int64 value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(TimeField field, const base::Time& value) {
    // Round-trip to proto time format and back so that we have
    // consistent time resolutions (ms).
    time_fields[field - TIME_FIELDS_BEGIN] =
        ProtoTimeToTime(TimeToProtoTime(value));
  }
  inline void put(IdField field, const Id& value) {
    id_fields[field - ID_FIELDS_BEGIN] = value;
  }
  inline void put(BaseVersion field, int64 value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(IndexedBitField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(IsDelField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(BitField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(StringField field, const std::string& value) {
    string_fields[field - STRING_FIELDS_BEGIN] = value;
  }
  inline void put(ProtoField field, const sync_pb::EntitySpecifics& value) {
    specifics_fields[field - PROTO_FIELDS_BEGIN].CopyFrom(value);
  }
  inline void put(UniquePositionField field, const UniquePosition& value) {
    unique_position_fields[field - UNIQUE_POSITION_FIELDS_BEGIN] = value;
  }
  inline void put(BitTemp field, bool value) {
    bit_temps[field - BIT_TEMPS_BEGIN] = value;
  }

  // Const ref getters.
  inline int64 ref(MetahandleField field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline int64 ref(Int64Field field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline const base::Time& ref(TimeField field) const {
    return time_fields[field - TIME_FIELDS_BEGIN];
  }
  inline const Id& ref(IdField field) const {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline int64 ref(BaseVersion field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline bool ref(IndexedBitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(IsDelField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(BitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline const std::string& ref(StringField field) const {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline const sync_pb::EntitySpecifics& ref(ProtoField field) const {
    return specifics_fields[field - PROTO_FIELDS_BEGIN];
  }
  inline const UniquePosition& ref(UniquePositionField field) const {
    return unique_position_fields[field - UNIQUE_POSITION_FIELDS_BEGIN];
  }
  inline bool ref(BitTemp field) const {
    return bit_temps[field - BIT_TEMPS_BEGIN];
  }

  // Non-const, mutable ref getters for object types only.
  inline std::string& mutable_ref(StringField field) {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline sync_pb::EntitySpecifics& mutable_ref(ProtoField field) {
    return specifics_fields[field - PROTO_FIELDS_BEGIN];
  }
  inline Id& mutable_ref(IdField field) {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline UniquePosition& mutable_ref(UniquePositionField field) {
    return unique_position_fields[field - UNIQUE_POSITION_FIELDS_BEGIN];
  }

  ModelType GetModelType() const;
  ModelType GetServerModelType() const;
  bool ShouldMaintainPosition() const;

  // Dumps all kernel info into a DictionaryValue and returns it.
  // Transfers ownership of the DictionaryValue to the caller.
  // Note: |cryptographer| is an optional parameter for use in decrypting
  // encrypted specifics. If it is NULL or the specifics are not decryptsble,
  // they will be serialized as empty proto's.
  base::DictionaryValue* ToValue(Cryptographer* cryptographer) const;

 private:
  // Tracks whether this entry needs to be saved to the database.
  bool dirty_;
};

class EntryKernelLessByMetaHandle {
 public:
  inline bool operator()(const EntryKernel* a,
                         const EntryKernel* b) const {
    return a->ref(META_HANDLE) < b->ref(META_HANDLE);
  }
};

typedef std::set<const EntryKernel*, EntryKernelLessByMetaHandle>
    EntryKernelSet;

struct EntryKernelMutation {
  EntryKernel original, mutated;
};

typedef std::map<int64, EntryKernelMutation> EntryKernelMutationMap;

typedef Immutable<EntryKernelMutationMap> ImmutableEntryKernelMutationMap;

// Caller owns the return value.
base::DictionaryValue* EntryKernelMutationToValue(
    const EntryKernelMutation& mutation);

// Caller owns the return value.
base::ListValue* EntryKernelMutationMapToValue(
    const EntryKernelMutationMap& mutations);

}  // namespace syncable
}  // namespace syncer

#endif // SYNC_SYNCABLE_ENTRY_KERNEL_H_
