// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_DISJOINT_RANGE_LOCK_MANAGER_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_DISJOINT_RANGE_LOCK_MANAGER_H_

#include <list>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {

// Holds locks of the scopes system. To be performant without an Interval Tree,
// this implementation has the following invariants:
// * All lock range requests at a level must be disjoint - they cannot overlap.
// * Lock ranges are remembered for future performance - remove them using
//   RemoveLockRange.
// * All calls must happen from the same sequenced task runner.
class CONTENT_EXPORT DisjointRangeLockManager : public ScopesLockManager {
 public:
  // Creates a lock manager with the given number of levels, the comparator for
  // leveldb keys, and the current task runner that we are running on. The task
  // runner will be used for the lock acquisition callbacks.
  DisjointRangeLockManager(
      int level_count,
      const leveldb::Comparator* comparator,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~DisjointRangeLockManager() override;

  int64_t LocksHeldForTesting() const override;
  int64_t RequestsWaitingForTesting() const override;

  void AcquireLock(int level,
                   const LockRange& range,
                   LockType type,
                   LockAquiredCallback callback) override;

  // Remove the given lock range at the given level. The lock range must not be
  // in use. Use this if the lock will never be used again.
  void RemoveLockRange(int level, const LockRange& range);

 private:
  struct LockRequest {
   public:
    LockRequest();
    LockRequest(LockRequest&&) noexcept;
    LockRequest(LockType type, LockAquiredCallback callback);
    ~LockRequest();

    LockType requested_type = LockType::kShared;
    LockAquiredCallback callback;
  };

  // Represents a lock, which has a range and a level. To support shared access,
  // there can be multiple acquisitions of this lock, represented in
  // |acquired_count|. Also holds the pending requests for this lock.
  struct Lock {
   public:
    Lock();
    Lock(Lock&&) noexcept;
    ~Lock();
    Lock& operator=(Lock&&) noexcept;

    bool CanBeAcquired(LockType lock_type) {
      return acquired_count == 0 ||
             (queue.empty() && this->lock_mode == LockType::kShared &&
              lock_type == LockType::kShared);
    }

    int acquired_count = 0;
    LockType lock_mode = LockType::kShared;
    std::list<LockRequest> queue;

   private:
    DISALLOW_COPY_AND_ASSIGN(Lock);
  };

  // This is a proxy between a LevelDB Comparator and the interface expected by
  // std::map. It sorts using the |begin| entry of the ranges.
  class RangeLessThan {
   public:
    explicit RangeLessThan(const leveldb::Comparator* comparator)
        : comparator_(comparator) {}
    bool operator()(const LockRange& a, const LockRange& b) const {
      return comparator_->Compare(leveldb::Slice(a.begin),
                                  leveldb::Slice(b.begin)) < 0;
    }

   private:
    const leveldb::Comparator* const comparator_;
  };

  using LockLevelMap = base::flat_map<LockRange, Lock, RangeLessThan>;

  void LockReleased(int level, LockRange range);

#if DCHECK_IS_ON()
  static bool IsRangeDisjointFromNeighbors(
      const LockLevelMap& map,
      const LockRange& range,
      const leveldb::Comparator* const comparator_);
#endif

  const leveldb::Comparator* const comparator_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // This vector should never be modified after construction.
  std::vector<LockLevelMap> locks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DisjointRangeLockManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(DisjointRangeLockManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_SCOPES_DISJOINT_RANGE_LOCK_MANAGER_H_
