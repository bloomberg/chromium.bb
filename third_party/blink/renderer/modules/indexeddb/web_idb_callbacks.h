/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class IDBKey;
class IDBValue;
class WebIDBCursorImpl;
struct IDBDatabaseMetadata;

class WebIDBCallbacks {
 public:
  virtual ~WebIDBCallbacks() = default;

  virtual void SetState(base::WeakPtr<WebIDBCursorImpl> cursor,
                        int64_t transaction_id) = 0;

  // Pointers transfer ownership.
  virtual void Error(int32_t code, const String& message) = 0;
  virtual void SuccessNamesAndVersionsList(
      Vector<mojom::blink::IDBNameAndVersionPtr>) = 0;
  virtual void SuccessStringList(const Vector<String>&) = 0;
  virtual void SuccessCursor(
      mojom::blink::IDBCursorAssociatedPtrInfo cursor_info,
      std::unique_ptr<IDBKey> key,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>> optional_value) = 0;
  virtual void SuccessCursorPrefetch(
      Vector<std::unique_ptr<IDBKey>> keys,
      Vector<std::unique_ptr<IDBKey>> primary_keys,
      Vector<std::unique_ptr<IDBValue>> values) = 0;
  virtual void SuccessDatabase(
      mojom::blink::IDBDatabaseAssociatedPtrInfo database_info,
      const IDBDatabaseMetadata& metadata) = 0;
  virtual void SuccessKey(std::unique_ptr<IDBKey>) = 0;
  virtual void SuccessValue(mojom::blink::IDBReturnValuePtr) = 0;
  virtual void SuccessArray(Vector<mojom::blink::IDBReturnValuePtr>) = 0;
  virtual void SuccessInteger(int64_t) = 0;
  virtual void Success() = 0;
  virtual void SuccessCursorContinue(
      std::unique_ptr<IDBKey>,
      std::unique_ptr<IDBKey> primary_key,
      base::Optional<std::unique_ptr<IDBValue>>) = 0;
  virtual void Blocked(int64_t old_version) = 0;
  virtual void UpgradeNeeded(mojom::blink::IDBDatabaseAssociatedPtrInfo,
                             int64_t old_version,
                             mojom::IDBDataLoss data_loss,
                             const String& data_loss_message,
                             const IDBDatabaseMetadata&) = 0;
  virtual void Detach() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_H_
