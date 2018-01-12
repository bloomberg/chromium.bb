// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserverChanges.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBObservation.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/modules/indexeddb/WebIDBObservation.h"

namespace blink {

ScriptValue IDBObserverChanges::records(ScriptState* script_state) {
  v8::Local<v8::Context> context(script_state->GetContext());
  v8::Isolate* isolate(script_state->GetIsolate());
  v8::Local<v8::Map> map = v8::Map::New(isolate);
  for (const auto& it : records_) {
    v8::Local<v8::String> key =
        V8String(isolate, database_->GetObjectStoreName(it.key));
    v8::Local<v8::Value> value = ToV8(it.value, context->Global(), isolate);
    map->Set(context, key, value).ToLocalChecked();
  }
  return ScriptValue::From(script_state, map);
}

IDBObserverChanges* IDBObserverChanges::Create(
    IDBDatabase* database,
    IDBTransaction* transaction,
    const WebVector<WebIDBObservation>& web_observations,
    const HeapVector<Member<IDBObservation>>& observations,
    const WebVector<int32_t>& observation_indices) {
  DCHECK_EQ(web_observations.size(), observations.size());
  return new IDBObserverChanges(database, transaction, web_observations,
                                observations, observation_indices);
}

IDBObserverChanges::IDBObserverChanges(
    IDBDatabase* database,
    IDBTransaction* transaction,
    const WebVector<WebIDBObservation>& web_observations,
    const HeapVector<Member<IDBObservation>>& observations,
    const WebVector<int32_t>& observation_indices)
    : database_(database), transaction_(transaction) {
  DCHECK_EQ(web_observations.size(), observations.size());
  ExtractChanges(web_observations, observations, observation_indices);
}

void IDBObserverChanges::ExtractChanges(
    const WebVector<WebIDBObservation>& web_observations,
    const HeapVector<Member<IDBObservation>>& observations,
    const WebVector<int32_t>& observation_indices) {
  DCHECK_EQ(web_observations.size(), observations.size());

  // TODO(dmurph): Avoid getting and setting repeated times.
  for (const auto& idx : observation_indices) {
    records_
        .insert(web_observations[idx].object_store_id,
                HeapVector<Member<IDBObservation>>())
        .stored_value->value.emplace_back(observations[idx]);
  }
}

void IDBObserverChanges::Trace(blink::Visitor* visitor) {
  visitor->Trace(database_);
  visitor->Trace(transaction_);
  visitor->Trace(records_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
