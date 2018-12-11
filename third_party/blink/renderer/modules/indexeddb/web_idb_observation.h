// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_OBSERVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_OBSERVATION_H_

#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

struct WebIDBObservation {
  int64_t object_store_id;
  mojom::IDBOperationType type;
  Persistent<IDBKeyRange> key_range;
  std::unique_ptr<IDBValue> value;

  WebIDBObservation(int64_t object_store_id,
                    mojom::IDBOperationType type,
                    IDBKeyRange* key_range,
                    std::unique_ptr<IDBValue> value)
      : object_store_id(object_store_id),
        type(type),
        key_range(key_range),
        value(std::move(value)) {}

  WebIDBObservation(WebIDBObservation&&) = default;
  WebIDBObservation& operator=(WebIDBObservation&&) = default;

 private:
  // WebIDBObservation has to be move-only, because IDBValue is move-only.
  // Making the restriction explicit results in slightly better compilation
  // error messages in code that attempts copying.
  WebIDBObservation(const WebIDBObservation&) = delete;
  WebIDBObservation& operator=(const WebIDBObservation&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_WEB_IDB_OBSERVATION_H_
