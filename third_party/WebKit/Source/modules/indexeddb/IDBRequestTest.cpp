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

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBValue.h"
#include "modules/indexeddb/MockWebIDBDatabase.h"
#include "platform/SharedBuffer.h"
#include "public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/dtoa/utils.h"

namespace blink {
namespace {

TEST(IDBRequestTest, EventsAfterStopping) {
  V8TestingScope scope;
  IDBTransaction* transaction = nullptr;
  IDBRequest* request = IDBRequest::Create(
      scope.GetScriptState(), IDBAny::CreateUndefined(), transaction);
  EXPECT_EQ(request->readyState(), "pending");
  scope.GetExecutionContext()->NotifyContextDestroyed();

  // Ensure none of the following raise assertions in stopped state:
  request->OnError(DOMException::Create(kAbortError, "Description goes here."));
  request->OnSuccess(Vector<String>());
  request->OnSuccess(nullptr, IDBKey::CreateInvalid(), IDBKey::CreateInvalid(),
                     IDBValue::Create());
  request->OnSuccess(IDBKey::CreateInvalid());
  request->OnSuccess(IDBValue::Create());
  request->OnSuccess(static_cast<int64_t>(0));
  request->OnSuccess();
  request->OnSuccess(IDBKey::CreateInvalid(), IDBKey::CreateInvalid(),
                     IDBValue::Create());
}

TEST(IDBRequestTest, AbortErrorAfterAbort) {
  V8TestingScope scope;
  IDBTransaction* transaction = nullptr;
  IDBRequest* request = IDBRequest::Create(
      scope.GetScriptState(), IDBAny::CreateUndefined(), transaction);
  EXPECT_EQ(request->readyState(), "pending");

  // Simulate the IDBTransaction having received onAbort from back end and
  // aborting the request:
  request->Abort();

  // Now simulate the back end having fired an abort error at the request to
  // clear up any intermediaries.  Ensure an assertion is not raised.
  request->OnError(DOMException::Create(kAbortError, "Description goes here."));

  // Stop the request lest it be GCed and its destructor
  // finds the object in a pending state (and asserts.)
  scope.GetExecutionContext()->NotifyContextDestroyed();
}

TEST(IDBRequestTest, ConnectionsAfterStopping) {
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
        scope.GetScriptState(), callbacks, kTransactionId, kVersion);
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
        scope.GetScriptState(), callbacks, kTransactionId, kVersion);
    EXPECT_EQ(request->readyState(), "pending");
    std::unique_ptr<WebIDBCallbacks> callbacks = request->CreateWebCallbacks();

    scope.GetExecutionContext()->NotifyContextDestroyed();
    callbacks->OnSuccess(backend.release(), metadata);
  }
}

}  // namespace
}  // namespace blink
