/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBTransaction.h"

#include <memory>

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/EventQueue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBValue.h"
#include "modules/indexeddb/IDBValueWrapping.h"
#include "modules/indexeddb/MockWebIDBDatabase.h"
#include "platform/SharedBuffer.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

void DeactivateNewTransactions(v8::Isolate* isolate) {
  V8PerIsolateData::From(isolate)->RunEndOfScopeTasks();
}

class FakeIDBDatabaseCallbacks final : public IDBDatabaseCallbacks {
 public:
  static FakeIDBDatabaseCallbacks* Create() {
    return new FakeIDBDatabaseCallbacks();
  }
  void OnVersionChange(int64_t old_version, int64_t new_version) override {}
  void OnForcedClose() override {}
  void OnAbort(int64_t transaction_id, DOMException* error) override {}
  void OnComplete(int64_t transaction_id) override {}

 private:
  FakeIDBDatabaseCallbacks() {}
};

class IDBTransactionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    url_loader_mock_factory_ = Platform::Current()->GetURLLoaderMockFactory();
    WebURLResponse response;
    response.SetURL(KURL(NullURL(), "blob:"));
    url_loader_mock_factory_->RegisterURLProtocol(WebString("blob"), response,
                                                  "");
  }

  void TearDown() override {
    url_loader_mock_factory_->UnregisterAllURLsAndClearMemoryCache();
  }

  void BuildTransaction(V8TestingScope& scope,
                        std::unique_ptr<MockWebIDBDatabase> backend) {
    db_ = IDBDatabase::Create(scope.GetExecutionContext(), std::move(backend),
                              FakeIDBDatabaseCallbacks::Create(),
                              scope.GetIsolate());

    HashSet<String> transaction_scope = {"store"};
    transaction_ = IDBTransaction::CreateNonVersionChange(
        scope.GetScriptState(), kTransactionId, transaction_scope,
        kWebIDBTransactionModeReadOnly, db_.Get());

    IDBKeyPath store_key_path("primaryKey");
    RefPtr<IDBObjectStoreMetadata> store_metadata = AdoptRef(
        new IDBObjectStoreMetadata("store", kStoreId, store_key_path, true, 1));
    store_ = IDBObjectStore::Create(store_metadata, transaction_);
  }

  WebURLLoaderMockFactory* url_loader_mock_factory_;
  Persistent<IDBDatabase> db_;
  Persistent<IDBTransaction> transaction_;
  Persistent<IDBObjectStore> store_;

  static constexpr int64_t kTransactionId = 1234;
  static constexpr int64_t kStoreId = 5678;
};

// The created value is an array of true. If create_wrapped_value is true, the
// IDBValue's byte array will be wrapped in a Blob, otherwise it will not be.
RefPtr<IDBValue> CreateIDBValue(v8::Isolate* isolate,
                                bool create_wrapped_value) {
  size_t element_count = create_wrapped_value ? 16 : 2;
  v8::Local<v8::Array> v8_array = v8::Array::New(isolate, element_count);
  for (size_t i = 0; i < element_count; ++i)
    v8_array->Set(i, v8::True(isolate));

  NonThrowableExceptionState non_throwable_exception_state;
  IDBValueWrapper wrapper(isolate, v8_array,
                          SerializedScriptValue::SerializeOptions::kSerialize,
                          non_throwable_exception_state);
  wrapper.WrapIfBiggerThan(create_wrapped_value ? 0 : 1024 * element_count);

  std::unique_ptr<Vector<RefPtr<BlobDataHandle>>> blob_data_handles =
      WTF::MakeUnique<Vector<RefPtr<BlobDataHandle>>>();
  wrapper.ExtractBlobDataHandles(blob_data_handles.get());
  Vector<WebBlobInfo>& blob_infos = wrapper.WrappedBlobInfo();
  RefPtr<SharedBuffer> wrapped_marker_buffer = wrapper.ExtractWireBytes();
  IDBKey* key = IDBKey::CreateNumber(42.0);
  IDBKeyPath key_path(String("primaryKey"));

  RefPtr<IDBValue> idb_value = IDBValue::Create(
      std::move(wrapped_marker_buffer), std::move(blob_data_handles),
      WTF::MakeUnique<Vector<WebBlobInfo>>(blob_infos), key, key_path);

  DCHECK_EQ(create_wrapped_value,
            IDBValueUnwrapper::IsWrapped(idb_value.Get()));
  return idb_value;
}

TEST_F(IDBTransactionTest, ContextDestroyedEarlyDeath) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  PersistentHeapHashSet<WeakMember<IDBTransaction>> live_transactions;
  live_transactions.insert(transaction_);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1u, live_transactions.size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());

  DeactivateNewTransactions(scope.GetIsolate());

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1u, live_transactions.size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(DOMException::Create(kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0U, live_transactions.size());
}

TEST_F(IDBTransactionTest, ContextDestroyedAfterDone) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  PersistentHeapHashSet<WeakMember<IDBTransaction>> live_transactions;
  live_transactions.insert(transaction_);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  // This response should result in an event being enqueued immediately.
  request->HandleResponse(CreateIDBValue(scope.GetIsolate(), false));

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(DOMException::Create(kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  // The request completed, so it has enqueued a success event. Discard the
  // event, so that the transaction can go away.
  EXPECT_EQ(1U, live_transactions.size());
  scope.GetExecutionContext()->GetEventQueue()->Close();

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0U, live_transactions.size());
}

TEST_F(IDBTransactionTest, ContextDestroyedWithQueuedResult) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  PersistentHeapHashSet<WeakMember<IDBTransaction>> live_transactions;
  live_transactions.insert(transaction_);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  Persistent<IDBRequest> request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  request->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));

  request.Clear();  // The transaction is holding onto the request.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(DOMException::Create(kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0U, live_transactions.size());
}

TEST_F(IDBTransactionTest, ContextDestroyedWithTwoQueuedResults) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  PersistentHeapHashSet<WeakMember<IDBTransaction>> live_transactions;
  live_transactions.insert(transaction_);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  Persistent<IDBRequest> request1 =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  Persistent<IDBRequest> request2 =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  request1->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));
  request2->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));

  request1.Clear();  // The transaction is holding onto the requests.
  request2.Clear();
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetExecutionContext()->NotifyContextDestroyed();
  transaction_->OnAbort(DOMException::Create(kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0U, live_transactions.size());
}

TEST_F(IDBTransactionTest, DocumentShutdownWithQueuedAndBlockedResults) {
  // This test covers the conditions of https://crbug.com/733642

  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  PersistentHeapHashSet<WeakMember<IDBTransaction>> live_transactions;
  live_transactions.insert(transaction_);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  Persistent<IDBRequest> request1 =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  Persistent<IDBRequest> request2 =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  DeactivateNewTransactions(scope.GetIsolate());

  request1->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));
  request2->HandleResponse(CreateIDBValue(scope.GetIsolate(), false));

  request1.Clear();  // The transaction is holding onto the requests.
  request2.Clear();
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  // This will generate an Abort() call to the back end which is dropped by the
  // fake proxy, so an explicit OnAbort call is made.
  scope.GetDocument().Shutdown();
  transaction_->OnAbort(DOMException::Create(kAbortError, "Aborted"));
  transaction_.Clear();
  store_.Clear();

  url_loader_mock_factory_->ServeAsynchronousRequests();

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0U, live_transactions.size());
}

TEST_F(IDBTransactionTest, TransactionFinish) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Commit(kTransactionId)).Times(1);
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  PersistentHeapHashSet<WeakMember<IDBTransaction>> live_transactions;
  live_transactions.insert(transaction_);

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  DeactivateNewTransactions(scope.GetIsolate());

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  transaction_.Clear();
  store_.Clear();

  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1U, live_transactions.size());

  // Stop the context, so events don't get queued (which would keep the
  // transaction alive).
  scope.GetExecutionContext()->NotifyContextDestroyed();

  // Fire an abort to make sure this doesn't free the transaction during use.
  // The test will not fail if it is, but ASAN would notice the error.
  db_->OnAbort(kTransactionId, DOMException::Create(kAbortError, "Aborted"));

  // OnAbort() should have cleared the transaction's reference to the database.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0U, live_transactions.size());
}

}  // namespace
}  // namespace blink
