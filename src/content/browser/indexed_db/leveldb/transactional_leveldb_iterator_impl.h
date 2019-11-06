// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_iterator.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace leveldb {
class Snapshot;
}

namespace content {
class TransactionalLevelDBDatabase;

namespace indexed_db {
class DefaultLevelDBFactory;
}  // namespace indexed_db

class CONTENT_EXPORT TransactionalLevelDBIteratorImpl
    : public content::TransactionalLevelDBIterator {
 public:
  ~TransactionalLevelDBIteratorImpl() override;
  bool IsValid() const override;
  leveldb::Status SeekToLast() override;
  leveldb::Status Seek(const base::StringPiece& target) override;
  leveldb::Status Next() override;
  leveldb::Status Prev() override;
  base::StringPiece Key() const override;
  base::StringPiece Value() const override;
  void Detach() override;
  bool IsDetached() const override;

 protected:
  explicit TransactionalLevelDBIteratorImpl(
      std::unique_ptr<leveldb::Iterator> iterator,
      TransactionalLevelDBDatabase* db,
      const leveldb::Snapshot* snapshot);

 private:
  enum class IteratorState { ACTIVE, EVICTED_AND_VALID, EVICTED_AND_INVALID };

  leveldb::Status CheckStatus();

  // Notifies the database of iterator usage and recreates iterator if needed.
  void WillUseDBIterator();

  friend class indexed_db::DefaultLevelDBFactory;
  friend class MockBrowserTestIndexedDBClassFactory;

  std::unique_ptr<leveldb::Iterator> iterator_;

  // State used to facilitate memory purging. Sometimes this is destroyed before
  // we are, so use a WeakPtr.
  base::WeakPtr<TransactionalLevelDBDatabase> db_;
  IteratorState iterator_state_ = IteratorState::ACTIVE;
  std::string key_before_eviction_;
  const leveldb::Snapshot* snapshot_;

  DISALLOW_COPY_AND_ASSIGN(TransactionalLevelDBIteratorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_IMPL_H_
