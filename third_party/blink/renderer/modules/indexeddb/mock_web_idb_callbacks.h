// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_metadata.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_name_and_version.h"
#include "third_party/blink/renderer/modules/indexeddb/web_idb_value.h"

namespace blink {

class MockWebIDBCallbacks : public WebIDBCallbacks {
 public:
  MockWebIDBCallbacks();
  ~MockWebIDBCallbacks() override;
  MOCK_METHOD1(OnError, void(const WebIDBDatabaseError&));

  void OnSuccess(std::unique_ptr<IDBKey>,
                 std::unique_ptr<IDBKey> primaryKey,
                 WebIDBValue) override;
  MOCK_METHOD3(DoOnSuccess,
               void(const std::unique_ptr<IDBKey>& key,
                    const std::unique_ptr<IDBKey>& primaryKey,
                    const WebIDBValue& value));

  MOCK_METHOD1(OnSuccess, void(const WebVector<WebIDBNameAndVersion>&));
  MOCK_METHOD1(OnSuccess, void(const WebVector<WebString>&));

  void OnSuccess(WebIDBCursor* cursor,
                 std::unique_ptr<IDBKey> key,
                 std::unique_ptr<IDBKey> primaryKey,
                 WebIDBValue value) override;
  MOCK_METHOD4(DoOnSuccess,
               void(WebIDBCursor*,
                    const std::unique_ptr<IDBKey>&,
                    const std::unique_ptr<IDBKey>& primaryKey,
                    const WebIDBValue&));

  MOCK_METHOD2(OnSuccess, void(WebIDBDatabase*, const IDBDatabaseMetadata&));
  void OnSuccess(std::unique_ptr<IDBKey>) override;
  MOCK_METHOD1(DoOnSuccess, void(const std::unique_ptr<IDBKey>&));

  void OnSuccess(WebIDBValue) override;
  MOCK_METHOD1(DoOnSuccess, void(const WebIDBValue&));

  void OnSuccess(WebVector<WebIDBValue>) override;
  MOCK_METHOD1(DoOnSuccess, void(const WebVector<WebIDBValue>&));

  MOCK_METHOD1(OnSuccess, void(long long));
  MOCK_METHOD0(OnSuccess, void());
  MOCK_METHOD1(OnBlocked, void(long long oldVersion));
  MOCK_METHOD5(OnUpgradeNeeded,
               void(long long oldVersion,
                    WebIDBDatabase*,
                    const IDBDatabaseMetadata&,
                    mojom::IDBDataLoss dataLoss,
                    WebString dataLossMessage));
  MOCK_METHOD0(Detach, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebIDBCallbacks);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_MOCK_WEB_IDB_CALLBACKS_H_
