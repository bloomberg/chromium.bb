/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "modules/indexeddb/IDBRequest.h"

#include <memory>

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IDBValue.h"
#include "modules/indexeddb/IDBValueWrapping.h"
#include "modules/indexeddb/MockWebIDBDatabase.h"
#include "platform/SharedBuffer.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/dtoa/utils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

class IDBRequestTest : public ::testing::Test {
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
    db_ =
        IDBDatabase::Create(scope.GetExecutionContext(), std::move(backend),
                            IDBDatabaseCallbacks::Create(), scope.GetIsolate());

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

void EnsureIDBCallbacksDontThrow(IDBRequest* request,
                                 ExceptionState& exception_state) {
  ASSERT_TRUE(request->transaction());

  request->HandleResponse(
      DOMException::Create(kAbortError, "Description goes here."));
  request->HandleResponse(nullptr, IDBKey::CreateInvalid(),
                          IDBKey::CreateInvalid(), IDBValue::Create());
  request->HandleResponse(IDBKey::CreateInvalid());
  request->HandleResponse(IDBValue::Create());
  request->HandleResponse(static_cast<int64_t>(0));
  request->HandleResponse();
  request->HandleResponse(IDBKey::CreateInvalid(), IDBKey::CreateInvalid(),
                          IDBValue::Create());
  request->EnqueueResponse(Vector<String>());

  EXPECT_TRUE(!exception_state.HadException());
}

TEST_F(IDBRequestTest, EventsAfterEarlyDeathStop) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());

  EXPECT_EQ(request->readyState(), "pending");
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request->transaction());
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureIDBCallbacksDontThrow(request, scope.GetExceptionState());
}

TEST_F(IDBRequestTest, EventsAfterDoneStop) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request->transaction());
  request->HandleResponse(CreateIDBValue(scope.GetIsolate(), false));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureIDBCallbacksDontThrow(request, scope.GetExceptionState());
}

TEST_F(IDBRequestTest, EventsAfterEarlyDeathStopWithQueuedResult) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  EXPECT_EQ(request->readyState(), "pending");
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request->transaction());
  request->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureIDBCallbacksDontThrow(request, scope.GetExceptionState());
  url_loader_mock_factory_->ServeAsynchronousRequests();
  EnsureIDBCallbacksDontThrow(request, scope.GetExceptionState());
}

TEST_F(IDBRequestTest, EventsAfterEarlyDeathStopWithTwoQueuedResults) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
  EXPECT_CALL(*backend, Close()).Times(1);
  BuildTransaction(scope, std::move(backend));

  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(transaction_);

  IDBRequest* request1 =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  IDBRequest* request2 =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction_.Get(), IDBRequest::AsyncTraceState());
  EXPECT_EQ(request1->readyState(), "pending");
  EXPECT_EQ(request2->readyState(), "pending");
  ASSERT_TRUE(!scope.GetExceptionState().HadException());
  ASSERT_TRUE(request1->transaction());
  ASSERT_TRUE(request2->transaction());
  request1->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));
  request2->HandleResponse(CreateIDBValue(scope.GetIsolate(), true));
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EnsureIDBCallbacksDontThrow(request1, scope.GetExceptionState());
  EnsureIDBCallbacksDontThrow(request2, scope.GetExceptionState());
  url_loader_mock_factory_->ServeAsynchronousRequests();
  EnsureIDBCallbacksDontThrow(request1, scope.GetExceptionState());
  EnsureIDBCallbacksDontThrow(request2, scope.GetExceptionState());
}

TEST_F(IDBRequestTest, AbortErrorAfterAbort) {
  V8TestingScope scope;
  IDBTransaction* transaction = nullptr;
  IDBRequest* request =
      IDBRequest::Create(scope.GetScriptState(), IDBAny::Create(store_.Get()),
                         transaction, IDBRequest::AsyncTraceState());
  EXPECT_EQ(request->readyState(), "pending");

  // Simulate the IDBTransaction having received OnAbort from back end and
  // aborting the request:
  request->Abort();

  // Now simulate the back end having fired an abort error at the request to
  // clear up any intermediaries.  Ensure an assertion is not raised.
  request->HandleResponse(
      DOMException::Create(kAbortError, "Description goes here."));

  // Stop the request lest it be GCed and its destructor
  // finds the object in a pending state (and asserts.)
  scope.GetExecutionContext()->NotifyContextDestroyed();
}

TEST_F(IDBRequestTest, ConnectionsAfterStopping) {
  V8TestingScope scope;
  const int64_t kTransactionId = 1234;
  const int64_t kVersion = 1;
  const int64_t kOldVersion = 0;
  const WebIDBMetadata metadata;
  Persistent<IDBDatabaseCallbacks> callbacks = IDBDatabaseCallbacks::Create();

  {
    std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
    EXPECT_CALL(*backend, Close()).Times(1);
    IDBOpenDBRequest* request = IDBOpenDBRequest::Create(
        scope.GetScriptState(), callbacks, kTransactionId, kVersion,
        IDBRequest::AsyncTraceState());
    EXPECT_EQ(request->readyState(), "pending");
    std::unique_ptr<WebIDBCallbacks> callbacks = request->CreateWebCallbacks();

    scope.GetExecutionContext()->NotifyContextDestroyed();
    callbacks->OnUpgradeNeeded(kOldVersion, backend.release(), metadata,
                               kWebIDBDataLossNone, String());
  }

  {
    std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::Create();
    EXPECT_CALL(*backend, Close()).Times(1);
    IDBOpenDBRequest* request = IDBOpenDBRequest::Create(
        scope.GetScriptState(), callbacks, kTransactionId, kVersion,
        IDBRequest::AsyncTraceState());
    EXPECT_EQ(request->readyState(), "pending");
    std::unique_ptr<WebIDBCallbacks> callbacks = request->CreateWebCallbacks();

    scope.GetExecutionContext()->NotifyContextDestroyed();
    callbacks->OnSuccess(backend.release(), metadata);
  }
}

}  // namespace
}  // namespace blink
