// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/MockWebIDBDatabase.h"

#include <memory>

#include "base/memory/ptr_util.h"

namespace blink {

MockWebIDBDatabase::MockWebIDBDatabase() = default;

MockWebIDBDatabase::~MockWebIDBDatabase() = default;

std::unique_ptr<MockWebIDBDatabase> MockWebIDBDatabase::Create() {
  return base::WrapUnique(new MockWebIDBDatabase());
}

void MockWebIDBDatabase::Put(long long transaction_id,
                             long long object_store_id,
                             const WebData& value,
                             const WebVector<WebBlobInfo>& web_blob_info,
                             WebIDBKeyView primary_key,
                             WebIDBPutMode put_mode,
                             WebIDBCallbacks* callbacks,
                             const WebVector<long long>& index_ids,
                             WebVector<WebIndexKeys> index_keys) {
  DoPut(transaction_id, object_store_id, value, web_blob_info, primary_key,
        put_mode, callbacks, index_ids, index_keys);
}

}  // namespace blink
