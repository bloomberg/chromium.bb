// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_CALLBACKS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_CALLBACKS_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"

namespace blink {

class WebIDBCallbacks;

// Implements the child-process end of the pipe used to deliver callbacks.
// |callback_runner_| is used to post tasks back to the thread which owns the
// blink::WebIDBCallbacks.
class IndexedDBCallbacksImpl : public mojom::blink::IDBCallbacks {
 public:
  // |kNoTransaction| is used as the default transaction ID when instantiating
  // an IndexedDBCallbacksImpl instance.  See web_idb_factory_impl.cc for those
  // cases.
  enum : int64_t { kNoTransaction = -1 };

  IndexedDBCallbacksImpl(std::unique_ptr<WebIDBCallbacks> callbacks);
  ~IndexedDBCallbacksImpl() override;

  // mojom::blink::IDBCallbacks implementation:
  void Error(int32_t code, const String& message) override;
  void SuccessNamesAndVersionsList(
      Vector<mojom::blink::IDBNameAndVersionPtr> names_and_versions) override;
  void SuccessStringList(const Vector<String>& value) override;
  void Blocked(int64_t existing_version) override;
  void UpgradeNeeded(mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
                     int64_t old_version,
                     mojom::IDBDataLoss data_loss,
                     const String& data_loss_message,
                     const IDBDatabaseMetadata& metadata) override;
  void SuccessDatabase(mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
                       const IDBDatabaseMetadata& metadata) override;
  void SuccessCursor(mojom::blink::IDBCursorAssociatedPtrInfo cursor,
                     std::unique_ptr<IDBKey> key,
                     std::unique_ptr<IDBKey> primary_key,
                     base::Optional<std::unique_ptr<IDBValue>> value) override;
  void SuccessValue(mojom::blink::IDBReturnValuePtr value) override;
  void SuccessCursorContinue(
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>> value) override;
  void SuccessCursorPrefetch(Vector<std::unique_ptr<IDBKey>> keys,
                             Vector<std::unique_ptr<IDBKey>> primary_keys,
                             Vector<std::unique_ptr<IDBValue>> values) override;
  void SuccessArray(Vector<mojom::blink::IDBReturnValuePtr> values) override;
  void SuccessKey(std::unique_ptr<IDBKey> key) override;
  void SuccessInteger(int64_t value) override;
  void Success() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> callback_runner_;
  std::unique_ptr<WebIDBCallbacks> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBCallbacksImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_INDEXED_DB_CALLBACKS_IMPL_H_
