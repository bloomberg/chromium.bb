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

#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {
class String;
}

namespace blink {

class IDBDatabaseError;
class IDBKey;
class IDBValue;
class WebIDBCursor;
class WebIDBDatabase;
struct IDBDatabaseMetadata;
struct IDBNameAndVersion;

class WebIDBCallbacks {
 public:
  virtual ~WebIDBCallbacks() = default;

  // Pointers transfer ownership.
  virtual void OnError(const IDBDatabaseError&) = 0;
  virtual void OnSuccess(const Vector<IDBNameAndVersion>&) = 0;
  virtual void OnSuccess(const Vector<WTF::String>&) = 0;
  virtual void OnSuccess(WebIDBCursor*,
                         std::unique_ptr<IDBKey>,
                         std::unique_ptr<IDBKey> primary_key,
                         std::unique_ptr<IDBValue>) = 0;
  virtual void OnSuccess(WebIDBDatabase*, const IDBDatabaseMetadata&) = 0;
  virtual void OnSuccess(std::unique_ptr<IDBKey>) = 0;
  virtual void OnSuccess(std::unique_ptr<IDBValue>) = 0;
  virtual void OnSuccess(Vector<std::unique_ptr<IDBValue>>) = 0;
  virtual void OnSuccess(long long) = 0;
  virtual void OnSuccess() = 0;
  virtual void OnSuccess(std::unique_ptr<IDBKey>,
                         std::unique_ptr<IDBKey> primary_key,
                         std::unique_ptr<IDBValue>) = 0;
  virtual void OnBlocked(long long old_version) = 0;
  virtual void OnUpgradeNeeded(long long old_version,
                               WebIDBDatabase*,
                               const IDBDatabaseMetadata&,
                               mojom::IDBDataLoss data_loss,
                               WTF::String data_loss_message) = 0;
  virtual void Detach() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_CALLBACKS_H_
