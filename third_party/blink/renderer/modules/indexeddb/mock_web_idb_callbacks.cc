// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_callbacks.h"

#include "third_party/blink/renderer/modules/indexeddb/idb_key.h"

namespace blink {

MockWebIDBCallbacks::MockWebIDBCallbacks() {}

MockWebIDBCallbacks::~MockWebIDBCallbacks() {}

void MockWebIDBCallbacks::OnSuccess(std::unique_ptr<IDBKey> key,
                                    std::unique_ptr<IDBKey> primaryKey,
                                    WebIDBValue value) {
  DoOnSuccess(key, primaryKey, value);
}

void MockWebIDBCallbacks::OnSuccess(WebIDBCursor* cursor,
                                    std::unique_ptr<IDBKey> key,
                                    std::unique_ptr<IDBKey> primaryKey,
                                    WebIDBValue value) {
  DoOnSuccess(cursor, key, primaryKey, value);
}

void MockWebIDBCallbacks::OnSuccess(std::unique_ptr<IDBKey> key) {
  DoOnSuccess(key);
}

void MockWebIDBCallbacks::OnSuccess(WebIDBValue value) {
  DoOnSuccess(value);
}

void MockWebIDBCallbacks::OnSuccess(WebVector<WebIDBValue> values) {
  DoOnSuccess(values);
}

}  // namespace blink
