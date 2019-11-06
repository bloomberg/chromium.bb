// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPE_LOCK_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPE_LOCK_H_

#include <iosfwd>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/browser/indexed_db/scopes/scope_lock_range.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {

// Represents a granted lock in the ScopesLockManager. When this object is
// destroyed, the lock is released. Since default construction is supported,
// |is_locked()| can be used to inquire locked status. Also, |Release()| can
// be called to manually release the lock, which appropriately updates the
// |is_locked()| result.
class CONTENT_EXPORT ScopeLock {
 public:
  ScopeLock();
  ~ScopeLock();
  ScopeLock(ScopeLock&&) noexcept;
  // The |closure| is called when the lock is released, either by destruction
  // of this object or by the |Released()| call. It will be called
  // synchronously on the sequence runner this lock is released on.
  ScopeLock(ScopeLockRange range, int level, base::OnceClosure closure);
  // The lock in |other| is not released, and |this| must not be holding a lock.
  ScopeLock& operator=(ScopeLock&& other) noexcept;

  // Returns true if this object is holding a lock.
  bool is_locked() const { return is_locked_; }

  // Explicitly releases the granted lock.
  //
  // The lock is also released implicitly when this instance is destroyed.
  // This method is idempotent, i.e. it's valid to call Release() on an
  // instance that does not hold a granted lock.
  void Release();

  int level() const { return level_; }
  const ScopeLockRange& range() const { return range_; }

 private:
  friend std::ostream& operator<<(std::ostream& out, const ScopeLock& range);
  friend bool operator==(const ScopeLock& x, const ScopeLock& y);
  friend bool operator<(const ScopeLock& x, const ScopeLock& y);

  bool is_locked_ = false;
  ScopeLockRange range_;
  int level_ = 0;
  base::ScopedClosureRunner closure_runner_;

  DISALLOW_COPY_AND_ASSIGN(ScopeLock);
};

// Logging support.
std::ostream& operator<<(std::ostream& out, const ScopeLock& range);

// Equality doesn't take into account whether the lock 'is_locked()' or not,
// only the level and the range.
bool operator==(const ScopeLock& x, const ScopeLock& y);
CONTENT_EXPORT bool operator!=(const ScopeLock& x, const ScopeLock& y);
// Comparison operator to allow sorting for locking / unlocking order.
bool operator<(const ScopeLock& x, const ScopeLock& y);

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPE_LOCK_H_
