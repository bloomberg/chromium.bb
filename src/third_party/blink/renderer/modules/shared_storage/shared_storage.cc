// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_run_operation_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_set_method_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_shared_storage_url_with_metadata.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

// Use the native v8::ValueSerializer here as opposed to using
// blink::V8ScriptValueSerializer. It's capable of serializing objects of
// primitive types. It's TBD whether we want to support any other non-primitive
// types supported by blink::V8ScriptValueSerializer.
bool Serialize(ScriptState* script_state,
               const SharedStorageRunOperationMethodOptions* options,
               ExceptionState& exception_state,
               Vector<uint8_t>& output) {
  DCHECK(output.IsEmpty());

  if (!options->hasData())
    return true;

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::ValueSerializer serializer(isolate);

  v8::TryCatch try_catch(isolate);

  bool wrote_value;
  if (!serializer
           .WriteValue(script_state->GetContext(), options->data().V8Value())
           .To(&wrote_value)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return false;
  }

  DCHECK(wrote_value);

  std::pair<uint8_t*, size_t> buffer = serializer.Release();

  output.ReserveInitialCapacity(SafeCast<wtf_size_t>(buffer.second));
  output.Append(buffer.first, static_cast<wtf_size_t>(buffer.second));
  DCHECK_EQ(output.size(), buffer.second);

  free(buffer.first);

  return true;
}

void OnVoidOperationFinished(ScriptPromiseResolver* resolver,
                             SharedStorage* shared_storage,
                             bool success,
                             const String& error_message) {
  DCHECK(resolver);
  ScriptState* script_state = resolver->GetScriptState();

  if (!success) {
    ScriptState::Scope scope(script_state);
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kOperationError,
        error_message));
    return;
  }

  resolver->Resolve();
}

// TODO(crbug.com/1335504): Consider moving this function to
// third_party/blink/common/fenced_frame/fenced_frame_utils.cc.
bool IsValidFencedFrameReportingURL(const KURL& url) {
  if (!url.IsValid())
    return false;
  return url.ProtocolIs("https");
}

bool StringFromV8(v8::Isolate* isolate, v8::Local<v8::Value> val, String* out) {
  DCHECK(out);

  if (!val->IsString())
    return false;

  v8::Local<v8::String> str = v8::Local<v8::String>::Cast(val);
  wtf_size_t length = str->Utf8Length(isolate);
  LChar* buffer;
  *out = String::CreateUninitialized(length, buffer);

  str->WriteUtf8(isolate, reinterpret_cast<char*>(buffer), length, nullptr,
                 v8::String::NO_NULL_TERMINATION);

  return true;
}

}  // namespace

SharedStorage::SharedStorage() = default;
SharedStorage::~SharedStorage() = default;

void SharedStorage::Trace(Visitor* visitor) const {
  visitor->Trace(shared_storage_worklet_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise SharedStorage::set(ScriptState* script_state,
                                 const String& key,
                                 const String& value,
                                 ExceptionState& exception_state) {
  return set(script_state, key, value, SharedStorageSetMethodOptions::Create(),
             exception_state);
}

ScriptPromise SharedStorage::set(ScriptState* script_state,
                                 const String& key,
                                 const String& value,
                                 const SharedStorageSetMethodOptions* options,
                                 ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValidSharedStorageKeyStringLength(key.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"key\" parameter is not valid."));
    return promise;
  }

  if (!IsValidSharedStorageValueStringLength(value.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"value\" parameter is not valid."));
    return promise;
  }

  bool ignore_if_present =
      options->hasIgnoreIfPresent() && options->ignoreIfPresent();
  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageSet(
          key, value, ignore_if_present,
          WTF::Bind(&OnVoidOperationFinished, WrapPersistent(resolver),
                    WrapPersistent(this)));

  return promise;
}

ScriptPromise SharedStorage::append(ScriptState* script_state,
                                    const String& key,
                                    const String& value,
                                    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValidSharedStorageKeyStringLength(key.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"key\" parameter is not valid."));
    return promise;
  }

  if (!IsValidSharedStorageValueStringLength(value.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"value\" parameter is not valid."));
    return promise;
  }

  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageAppend(
          key, value,
          WTF::Bind(&OnVoidOperationFinished, WrapPersistent(resolver),
                    WrapPersistent(this)));

  return promise;
}

ScriptPromise SharedStorage::Delete(ScriptState* script_state,
                                    const String& key,
                                    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValidSharedStorageKeyStringLength(key.length())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"key\" parameter is not valid."));
    return promise;
  }

  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageDelete(
          key, WTF::Bind(&OnVoidOperationFinished, WrapPersistent(resolver),
                         WrapPersistent(this)));

  return promise;
}

ScriptPromise SharedStorage::clear(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetSharedStorageDocumentService(execution_context)
      ->SharedStorageClear(WTF::Bind(&OnVoidOperationFinished,
                                     WrapPersistent(resolver),
                                     WrapPersistent(this)));

  return promise;
}

ScriptPromise SharedStorage::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    ExceptionState& exception_state) {
  return selectURL(script_state, name, urls,
                   SharedStorageRunOperationMethodOptions::Create(),
                   exception_state);
}

ScriptPromise SharedStorage::selectURL(
    ScriptState* script_state,
    const String& name,
    HeapVector<Member<SharedStorageUrlWithMetadata>> urls,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required.");
    return ScriptPromise();
  }

  LocalFrame* frame = To<LocalDOMWindow>(execution_context)->GetFrame();
  DCHECK(frame);

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (frame->IsInFencedFrameTree()) {
    // https://github.com/pythagoraskitty/shared-storage/blob/main/README.md#url-selection
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kInvalidAccessError,
        "sharedStorage.selectURL() is not allowed in fenced frame."));
    return promise;
  }

  if (!IsValidSharedStorageURLsArrayLength(urls.size())) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), DOMExceptionCode::kDataError,
        "Length of the \"urls\" parameter is not valid."));
    return promise;
  }

  v8::Local<v8::Context> v8_context =
      script_state->GetIsolate()->GetCurrentContext();

  Vector<mojom::blink::SharedStorageUrlWithMetadataPtr> converted_urls;
  converted_urls.ReserveInitialCapacity(urls.size());

  wtf_size_t index = 0;
  for (const auto& url_with_metadata : urls) {
    DCHECK(url_with_metadata->hasUrl());

    KURL converted_url =
        execution_context->CompleteURL(url_with_metadata->url());

    // TODO(crbug.com/1318970): Use `IsValidFencedFrameURL()` or equivalent
    // logic here.
    if (!converted_url.IsValid()) {
      resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kDataError,
          "The url \"" + url_with_metadata->url() + "\" is invalid."));
      return promise;
    }

    HashMap<String, KURL> converted_reporting_metadata;

    if (url_with_metadata->hasReportingMetadata()) {
      DCHECK(url_with_metadata->reportingMetadata().V8Value()->IsObject());

      v8::Local<v8::Object> obj =
          url_with_metadata->reportingMetadata().V8Value().As<v8::Object>();

      v8::MaybeLocal<v8::Array> maybe_fields =
          obj->GetOwnPropertyNames(v8_context);
      v8::Local<v8::Array> fields;
      if (!maybe_fields.ToLocal(&fields) || fields->Length() == 0) {
        resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
            script_state->GetIsolate(), DOMExceptionCode::kDataError,
            "selectURL could not get reportingMetadata object attributes"));
        return promise;
      }

      converted_reporting_metadata.ReserveCapacityForSize(fields->Length());

      for (wtf_size_t idx = 0; idx < fields->Length(); idx++) {
        v8::Local<v8::Value> report_event =
            fields->Get(v8_context, idx).ToLocalChecked();
        String report_event_string;
        if (!StringFromV8(script_state->GetIsolate(), report_event,
                          &report_event_string)) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kDataError,
              "selectURL reportingMetadata object attributes must be "
              "strings"));
          return promise;
        }

        v8::Local<v8::Value> report_url =
            obj->Get(v8_context, report_event).ToLocalChecked();
        String report_url_string;
        if (!StringFromV8(script_state->GetIsolate(), report_url,
                          &report_url_string)) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kDataError,
              "selectURL reportingMetadata object attributes must be "
              "strings"));
          return promise;
        }

        KURL converted_report_url =
            execution_context->CompleteURL(report_url_string);

        if (!IsValidFencedFrameReportingURL(converted_report_url)) {
          resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kDataError,
              "The metadata for the url at index " +
                  String::NumberToStringECMAScript(index) +
                  " has an invalid or non-HTTPS report_url parameter \"" +
                  report_url_string + "\"."));
          return promise;
        }

        converted_reporting_metadata.Set(report_event_string,
                                         converted_report_url);
      }
    }

    converted_urls.push_back(mojom::blink::SharedStorageUrlWithMetadata::New(
        converted_url, std::move(converted_reporting_metadata)));
    index++;
  }

  Vector<uint8_t> serialized_data;
  if (!Serialize(script_state, options, exception_state, serialized_data))
    return promise;

  GetSharedStorageDocumentService(execution_context)
      ->RunURLSelectionOperationOnWorklet(
          name, std::move(converted_urls), std::move(serialized_data),
          WTF::Bind(
              [](ScriptPromiseResolver* resolver, SharedStorage* shared_storage,
                 bool success, const String& error_message,
                 const KURL& opaque_url) {
                DCHECK(resolver);
                ScriptState* script_state = resolver->GetScriptState();

                if (!success) {
                  ScriptState::Scope scope(script_state);
                  resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
                      script_state->GetIsolate(),
                      DOMExceptionCode::kOperationError, error_message));
                  return;
                }

                resolver->Resolve(opaque_url);
              },
              WrapPersistent(resolver), WrapPersistent(this)));

  return promise;
}

ScriptPromise SharedStorage::run(ScriptState* script_state,
                                 const String& name,
                                 ExceptionState& exception_state) {
  return run(script_state, name,
             SharedStorageRunOperationMethodOptions::Create(), exception_state);
}

ScriptPromise SharedStorage::run(
    ScriptState* script_state,
    const String& name,
    const SharedStorageRunOperationMethodOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  CHECK(execution_context->IsWindow());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "A browsing context is required.");
    return ScriptPromise();
  }

  Vector<uint8_t> serialized_data;
  if (!Serialize(script_state, options, exception_state, serialized_data))
    return ScriptPromise();

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetSharedStorageDocumentService(execution_context)
      ->RunOperationOnWorklet(
          name, std::move(serialized_data),
          WTF::Bind(&OnVoidOperationFinished, WrapPersistent(resolver),
                    WrapPersistent(this)));

  return promise;
}

SharedStorageWorklet* SharedStorage::worklet(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  if (shared_storage_worklet_)
    return shared_storage_worklet_.Get();

  shared_storage_worklet_ = MakeGarbageCollected<SharedStorageWorklet>(this);

  return shared_storage_worklet_.Get();
}

mojom::blink::SharedStorageDocumentService*
SharedStorage::GetSharedStorageDocumentService(
    ExecutionContext* execution_context) {
  CHECK(execution_context->IsWindow());
  if (!shared_storage_document_service_.is_bound()) {
    LocalFrame* frame = To<LocalDOMWindow>(execution_context)->GetFrame();
    DCHECK(frame);

    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        shared_storage_document_service_.BindNewEndpointAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return shared_storage_document_service_.get();
}

}  // namespace blink
