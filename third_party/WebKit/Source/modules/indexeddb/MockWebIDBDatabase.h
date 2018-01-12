// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockWebIDBDatabase_h
#define MockWebIDBDatabase_h

#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"
#include <gmock/gmock.h>
#include <memory>

namespace blink {

class MockWebIDBDatabase : public ::testing::StrictMock<WebIDBDatabase> {
 public:
  virtual ~MockWebIDBDatabase();

  static std::unique_ptr<MockWebIDBDatabase> Create();

  MOCK_METHOD5(CreateObjectStore,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebString& name,
                    const WebIDBKeyPath&,
                    bool auto_increment));
  MOCK_METHOD2(DeleteObjectStore,
               void(long long transaction_id, long long object_store_id));
  MOCK_METHOD3(RenameObjectStore,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebString& new_name));
  MOCK_METHOD3(CreateTransaction,
               void(long long id,
                    const WebVector<long long>& scope,
                    WebIDBTransactionMode));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(VersionChangeIgnored, void());
  MOCK_METHOD1(Abort, void(long long transaction_id));
  MOCK_METHOD1(Commit, void(long long transaction_id));
  MOCK_METHOD7(CreateIndex,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebString& name,
                    const WebIDBKeyPath&,
                    bool unique,
                    bool multi_entry));
  MOCK_METHOD3(DeleteIndex,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id));
  MOCK_METHOD4(RenameIndex,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebString& new_name));
  MOCK_METHOD6(
      AddObserver,
      void(long long transaction_id,
           int32_t observer_id,
           bool include_transaction,
           bool no_records,
           bool values,
           const std::bitset<kWebIDBOperationTypeCount>& operation_types));
  MOCK_CONST_METHOD1(ContainsObserverId, bool(int32_t id));
  MOCK_METHOD1(RemoveObservers,
               void(const WebVector<int32_t>& observer_ids_to_remove));
  MOCK_METHOD6(Get,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    bool key_only,
                    WebIDBCallbacks*));
  MOCK_METHOD7(GetAll,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    long long max_count,
                    bool key_only,
                    WebIDBCallbacks*));

  // Google Mock doesn't support methods with move-only arguments.
  void Put(long long transaction_id,
           long long object_store_id,
           const WebData& value,
           const WebVector<WebBlobInfo>&,
           WebIDBKeyView primary_key,
           WebIDBPutMode,
           WebIDBCallbacks*,
           const WebVector<long long>& index_ids,
           WebVector<WebIndexKeys>) override;
  MOCK_METHOD9(DoPut,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebData& value,
                    const WebVector<WebBlobInfo>&,
                    WebIDBKeyView primary_key,
                    WebIDBPutMode,
                    WebIDBCallbacks*,
                    const WebVector<long long>& index_ids,
                    const WebVector<WebIndexKeys>&));

  MOCK_METHOD5(SetIndexKeys,
               void(long long transaction_id,
                    long long object_store_id,
                    WebIDBKeyView primary_key,
                    const WebVector<long long>& index_ids,
                    const WebVector<WebIndexKeys>&));
  MOCK_METHOD3(SetIndexesReady,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebVector<long long>& index_ids));
  MOCK_METHOD8(OpenCursor,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    WebIDBCursorDirection,
                    bool key_only,
                    WebIDBTaskType,
                    WebIDBCallbacks*));
  MOCK_METHOD5(Count,
               void(long long transaction_id,
                    long long object_store_id,
                    long long index_id,
                    const WebIDBKeyRange&,
                    WebIDBCallbacks*));
  MOCK_METHOD4(Delete,
               void(long long transaction_id,
                    long long object_store_id,
                    WebIDBKeyView primary_key,
                    WebIDBCallbacks*));
  MOCK_METHOD4(DeleteRange,
               void(long long transaction_id,
                    long long object_store_id,
                    const WebIDBKeyRange&,
                    WebIDBCallbacks*));
  MOCK_METHOD3(Clear,
               void(long long transaction_id,
                    long long object_store_id,
                    WebIDBCallbacks*));
  MOCK_METHOD1(AckReceivedBlobs, void(const WebVector<WebString>& uuids));

 private:
  MockWebIDBDatabase();
};

}  // namespace blink

#endif  // MockWebIDBDatabase_h
