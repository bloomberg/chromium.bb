// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_ID_H_
#define SYNC_SYNCABLE_SYNCABLE_ID_H_

#include <iosfwd>
#include <limits>
#include <sstream>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/hash_tables.h"
#include "sync/base/sync_export.h"

class MockConnectionManager;

namespace base {
class StringValue;
}

namespace sql {
class Statement;
}

namespace syncer {
namespace syncable {
struct EntryKernel;
class Id;

SYNC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& out, const Id& id);

// For historical reasons, 3 concepts got everloaded into the Id:
// 1. A unique, opaque identifier for the object.
// 2. Flag specifing whether server know about this object.
// 3. Flag for root.
//
// We originally wrapped an integer for this information, but now we use a
// string. It will have one of three forms:
// 1. c<client only opaque id> for client items that have not been committed.
// 2. r for the root item.
// 3. s<server provided opaque id> for items that the server knows about.
class SYNC_EXPORT Id {
 public:
  // This constructor will be handy even when we move away from int64s, just
  // for unit tests.
  inline Id() : s_("r") { }
  inline Id(const Id& that) {
    Copy(that);
  }
  inline Id& operator = (const Id& that) {
    Copy(that);
    return *this;
  }
  inline void Copy(const Id& that) {
    this->s_ = that.s_;
  }
  inline bool IsRoot() const {
    return "r" == s_;
  }
  inline bool ServerKnows() const {
    return s_[0] == 's' || s_ == "r";
  }

  // TODO(sync): We could use null here, but to ease conversion we use "r".
  // fix this, this is madness :)
  inline bool IsNull() const {
    return IsRoot();
  }
  inline void Clear() {
    s_ = "r";
  }
  inline int compare(const Id& that) const {
    return s_.compare(that.s_);
  }
  inline bool operator == (const Id& that) const {
    return s_ == that.s_;
  }
  inline bool operator != (const Id& that) const {
    return s_ != that.s_;
  }
  inline bool operator < (const Id& that) const {
    return s_ < that.s_;
  }
  inline bool operator > (const Id& that) const {
    return s_ > that.s_;
  }

  const std::string& value() const {
    return s_;
  }

  // Return the next highest ID in the lexicographic ordering.  This is
  // useful for computing upper bounds on std::sets that are ordered
  // by operator<.
  Id GetLexicographicSuccessor() const;

  // Dumps the ID as a value and returns it.  Transfers ownership of
  // the StringValue to the caller.
  base::StringValue* ToValue() const;

  // Three functions are used to work with our proto buffers.
  std::string GetServerId() const;
  static Id CreateFromServerId(const std::string& server_id);
  // This should only be used if you get back a reference to a local
  // id from the server. Returns a client only opaque id.
  static Id CreateFromClientString(const std::string& local_id);

  // This method returns an ID that will compare less than any valid ID.
  // The returned ID is not a valid ID itself.  This is useful for
  // computing lower bounds on std::sets that are ordered by operator<.
  static Id GetLeastIdForLexicographicComparison();

 private:
  friend scoped_ptr<EntryKernel> UnpackEntry(sql::Statement* statement);
  friend void BindFields(const EntryKernel& entry,
                         sql::Statement* statement);
  SYNC_EXPORT_PRIVATE friend std::ostream& operator<<(std::ostream& out,
                                                      const Id& id);
  friend class MockConnectionManager;
  friend class SyncableIdTest;

  std::string s_;
};

SYNC_EXPORT_PRIVATE Id GetNullId();

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_ID_H_
