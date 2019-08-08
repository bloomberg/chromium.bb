// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_class_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator_impl.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"

namespace content {

static IndexedDBClassFactory::GetterCallback* s_factory_getter;
static ::base::LazyInstance<IndexedDBClassFactory>::Leaky s_factory =
    LAZY_INSTANCE_INITIALIZER;

void IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetterCallback* cb) {
  s_factory_getter = cb;
}

IndexedDBClassFactory* IndexedDBClassFactory::Get() {
  if (s_factory_getter)
    return (*s_factory_getter)();
  else
    return s_factory.Pointer();
}

std::unique_ptr<IndexedDBDatabase>
IndexedDBClassFactory::CreateIndexedDBDatabase(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    IndexedDBDatabase::ErrorCallback error_callback,
    base::OnceClosure destroy_me,
    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
    const IndexedDBDatabase::Identifier& unique_identifier,
    ScopesLockManager* transaction_lock_manager) {
  return base::WrapUnique(new IndexedDBDatabase(
      name, backing_store, factory, std::move(error_callback),
      std::move(destroy_me), std::move(metadata_coding), unique_identifier,
      transaction_lock_manager));
}

std::unique_ptr<IndexedDBTransaction>
IndexedDBClassFactory::CreateIndexedDBTransaction(
    int64_t id,
    IndexedDBConnection* connection,
    ErrorCallback error_callback,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  return base::WrapUnique(
      new IndexedDBTransaction(id, connection, std::move(error_callback), scope,
                               mode, backing_store_transaction));
}

scoped_refptr<LevelDBTransaction>
IndexedDBClassFactory::CreateLevelDBTransaction(LevelDBDatabase* db) {
  return new LevelDBTransaction(db);
}

std::unique_ptr<LevelDBIteratorImpl> IndexedDBClassFactory::CreateIteratorImpl(
    std::unique_ptr<leveldb::Iterator> iterator,
    LevelDBDatabase* db,
    const leveldb::Snapshot* snapshot) {
  return base::WrapUnique(
      new LevelDBIteratorImpl(std::move(iterator), db, snapshot));
}

}  // namespace content
