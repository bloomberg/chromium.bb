// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/nfc/NFC.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8StringResource.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/nfc/NFCError.h"
#include "modules/nfc/NFCMessage.h"
#include "modules/nfc/NFCPushOptions.h"
#include "modules/nfc/NFCWatchOptions.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace {
const char kJsonMimePrefix[] = "application/";
const char kJsonMimeType[] = "application/json";
const char kOpaqueMimeType[] = "application/octet-stream";
const char kPlainTextMimeType[] = "text/plain";
const char kPlainTextMimePrefix[] = "text/";
const char kCharSetUTF8[] = ";charset=UTF-8";
}  // anonymous namespace

// Mojo type converters
namespace mojo {

using device::nfc::mojom::blink::NFCMessage;
using device::nfc::mojom::blink::NFCMessagePtr;
using device::nfc::mojom::blink::NFCRecord;
using device::nfc::mojom::blink::NFCRecordPtr;
using device::nfc::mojom::blink::NFCRecordType;
using device::nfc::mojom::blink::NFCRecordTypeFilter;
using device::nfc::mojom::blink::NFCPushOptions;
using device::nfc::mojom::blink::NFCPushOptionsPtr;
using device::nfc::mojom::blink::NFCPushTarget;
using device::nfc::mojom::blink::NFCWatchMode;
using device::nfc::mojom::blink::NFCWatchOptions;
using device::nfc::mojom::blink::NFCWatchOptionsPtr;

NFCPushTarget toNFCPushTarget(const WTF::String& target) {
  if (target == "tag")
    return NFCPushTarget::TAG;

  if (target == "peer")
    return NFCPushTarget::PEER;

  return NFCPushTarget::ANY;
}

NFCRecordType toNFCRecordType(const WTF::String& recordType) {
  if (recordType == "empty")
    return NFCRecordType::EMPTY;

  if (recordType == "text")
    return NFCRecordType::TEXT;

  if (recordType == "url")
    return NFCRecordType::URL;

  if (recordType == "json")
    return NFCRecordType::JSON;

  if (recordType == "opaque")
    return NFCRecordType::OPAQUE_RECORD;

  NOTREACHED();
  return NFCRecordType::EMPTY;
}

NFCWatchMode toNFCWatchMode(const WTF::String& watchMode) {
  if (watchMode == "web-nfc-only")
    return NFCWatchMode::WEBNFC_ONLY;

  if (watchMode == "any")
    return NFCWatchMode::ANY;

  NOTREACHED();
  return NFCWatchMode::WEBNFC_ONLY;
}

// https://w3c.github.io/web-nfc/#creating-web-nfc-message Step 2.1
// If NFCRecord type is not provided, deduce NFCRecord type from JS data type:
// String or Number => 'text' record
// ArrayBuffer => 'opaque' record
// JSON serializable Object => 'json' record
NFCRecordType deduceRecordTypeFromDataType(const blink::NFCRecord& record) {
  if (record.hasData()) {
    v8::Local<v8::Value> value = record.data().V8Value();

    if (value->IsString() ||
        (value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value()))) {
      return NFCRecordType::TEXT;
    }

    if (value->IsObject() && !value->IsArrayBuffer()) {
      return NFCRecordType::JSON;
    }

    if (value->IsArrayBuffer()) {
      return NFCRecordType::OPAQUE_RECORD;
    }
  }

  return NFCRecordType::EMPTY;
}

void setMediaType(NFCRecordPtr& recordPtr,
                  const WTF::String& recordMediaType,
                  const WTF::String& defaultMediaType) {
  recordPtr->media_type =
      recordMediaType.IsEmpty() ? defaultMediaType : recordMediaType;
}

template <>
struct TypeConverter<WTF::Vector<uint8_t>, WTF::String> {
  static WTF::Vector<uint8_t> Convert(const WTF::String& string) {
    WTF::CString utf8String = string.Utf8();
    WTF::Vector<uint8_t> array;
    array.Append(utf8String.Data(), utf8String.length());
    return array;
  }
};

template <>
struct TypeConverter<WTF::Vector<uint8_t>, blink::DOMArrayBuffer*> {
  static WTF::Vector<uint8_t> Convert(blink::DOMArrayBuffer* buffer) {
    WTF::Vector<uint8_t> array;
    array.Append(static_cast<uint8_t*>(buffer->Data()), buffer->ByteLength());
    return array;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, WTF::String> {
  static NFCRecordPtr Convert(const WTF::String& string) {
    NFCRecordPtr record = NFCRecord::New();
    record->record_type = NFCRecordType::TEXT;
    record->media_type = kPlainTextMimeType;
    record->media_type.Append(kCharSetUTF8);
    record->data = mojo::ConvertTo<WTF::Vector<uint8_t>>(string);
    return record;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::DOMArrayBuffer*> {
  static NFCRecordPtr Convert(blink::DOMArrayBuffer* buffer) {
    NFCRecordPtr record = NFCRecord::New();
    record->record_type = NFCRecordType::OPAQUE_RECORD;
    record->media_type = kOpaqueMimeType;
    record->data = mojo::ConvertTo<WTF::Vector<uint8_t>>(buffer);
    return record;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, WTF::String> {
  static NFCMessagePtr Convert(const WTF::String& string) {
    NFCMessagePtr message = NFCMessage::New();
    message->data.push_back(NFCRecord::From(string));
    return message;
  }
};

template <>
struct TypeConverter<WTF::Optional<WTF::Vector<uint8_t>>, blink::ScriptValue> {
  static WTF::Optional<WTF::Vector<uint8_t>> Convert(
      const blink::ScriptValue& scriptValue) {
    v8::Local<v8::Value> value = scriptValue.V8Value();

    if (value->IsNumber())
      return mojo::ConvertTo<WTF::Vector<uint8_t>>(
          WTF::String::Number(value.As<v8::Number>()->Value()));

    if (value->IsString()) {
      blink::V8StringResource<> stringResource = value;
      if (stringResource.Prepare()) {
        return mojo::ConvertTo<WTF::Vector<uint8_t>>(
            WTF::String(stringResource));
      }
    }

    if (value->IsObject() && !value->IsArray() && !value->IsArrayBuffer()) {
      v8::Local<v8::String> jsonString;
      if (v8::JSON::Stringify(scriptValue.GetContext(), value.As<v8::Object>())
              .ToLocal(&jsonString)) {
        WTF::String wtfString = blink::V8StringToWebCoreString<WTF::String>(
            jsonString, blink::kDoNotExternalize);
        return mojo::ConvertTo<WTF::Vector<uint8_t>>(wtfString);
      }
    }

    if (value->IsArrayBuffer())
      return mojo::ConvertTo<WTF::Vector<uint8_t>>(
          blink::V8ArrayBuffer::toImpl(value.As<v8::Object>()));

    return WTF::kNullopt;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::NFCRecord> {
  static NFCRecordPtr Convert(const blink::NFCRecord& record) {
    NFCRecordPtr recordPtr = NFCRecord::New();

    if (record.hasRecordType())
      recordPtr->record_type = toNFCRecordType(record.recordType());
    else
      recordPtr->record_type = deduceRecordTypeFromDataType(record);

    // If record type is "empty", no need to set media type or data.
    // https://w3c.github.io/web-nfc/#creating-web-nfc-message
    if (recordPtr->record_type == NFCRecordType::EMPTY)
      return recordPtr;

    switch (recordPtr->record_type) {
      case NFCRecordType::TEXT:
      case NFCRecordType::URL:
        setMediaType(recordPtr, record.mediaType(), kPlainTextMimeType);
        recordPtr->media_type.Append(kCharSetUTF8);
        break;
      case NFCRecordType::JSON:
        setMediaType(recordPtr, record.mediaType(), kJsonMimeType);
        break;
      case NFCRecordType::OPAQUE_RECORD:
        setMediaType(recordPtr, record.mediaType(), kOpaqueMimeType);
        break;
      default:
        NOTREACHED();
        break;
    }

    auto recordData =
        mojo::ConvertTo<WTF::Optional<WTF::Vector<uint8_t>>>(record.data());
    // If JS object cannot be converted to uint8_t array, return null,
    // interrupt NFCMessage conversion algorithm and reject promise with
    // SyntaxError exception.
    if (!recordData)
      return nullptr;

    recordPtr->data = recordData.value();
    return recordPtr;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCMessage> {
  static NFCMessagePtr Convert(const blink::NFCMessage& message) {
    NFCMessagePtr messagePtr = NFCMessage::New();
    messagePtr->url = message.url();
    messagePtr->data.Resize(message.data().size());
    for (size_t i = 0; i < message.data().size(); ++i) {
      NFCRecordPtr record = NFCRecord::From(message.data()[i]);
      if (record.is_null())
        return nullptr;

      messagePtr->data[i] = std::move(record);
    }
    return messagePtr;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::DOMArrayBuffer*> {
  static NFCMessagePtr Convert(blink::DOMArrayBuffer* buffer) {
    NFCMessagePtr message = NFCMessage::New();
    message->data.push_back(NFCRecord::From(buffer));
    return message;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCPushMessage> {
  static NFCMessagePtr Convert(const blink::NFCPushMessage& message) {
    if (message.isString())
      return NFCMessage::From(message.getAsString());

    if (message.isNFCMessage())
      return NFCMessage::From(message.getAsNFCMessage());

    if (message.isArrayBuffer())
      return NFCMessage::From(message.getAsArrayBuffer());

    NOTREACHED();
    return nullptr;
  }
};

template <>
struct TypeConverter<NFCPushOptionsPtr, blink::NFCPushOptions> {
  static NFCPushOptionsPtr Convert(const blink::NFCPushOptions& pushOptions) {
    // https://w3c.github.io/web-nfc/#the-nfcpushoptions-dictionary
    // Default values for NFCPushOptions dictionary are:
    // target = 'any', timeout = Infinity, ignoreRead = true
    NFCPushOptionsPtr pushOptionsPtr = NFCPushOptions::New();

    if (pushOptions.hasTarget())
      pushOptionsPtr->target = toNFCPushTarget(pushOptions.target());
    else
      pushOptionsPtr->target = NFCPushTarget::ANY;

    if (pushOptions.hasTimeout())
      pushOptionsPtr->timeout = pushOptions.timeout();
    else
      pushOptionsPtr->timeout = std::numeric_limits<double>::infinity();

    if (pushOptions.hasIgnoreRead())
      pushOptionsPtr->ignore_read = pushOptions.ignoreRead();
    else
      pushOptionsPtr->ignore_read = true;

    return pushOptionsPtr;
  }
};

template <>
struct TypeConverter<NFCWatchOptionsPtr, blink::NFCWatchOptions> {
  static NFCWatchOptionsPtr Convert(
      const blink::NFCWatchOptions& watchOptions) {
    // https://w3c.github.io/web-nfc/#the-nfcwatchoptions-dictionary
    // Default values for NFCWatchOptions dictionary are:
    // url = "", recordType = null, mediaType = "", mode = "web-nfc-only"
    NFCWatchOptionsPtr watchOptionsPtr = NFCWatchOptions::New();
    watchOptionsPtr->url = watchOptions.url();
    watchOptionsPtr->media_type = watchOptions.mediaType();

    if (watchOptions.hasMode())
      watchOptionsPtr->mode = toNFCWatchMode(watchOptions.mode());
    else
      watchOptionsPtr->mode = NFCWatchMode::WEBNFC_ONLY;

    if (watchOptions.hasRecordType()) {
      watchOptionsPtr->record_filter = NFCRecordTypeFilter::New();
      watchOptionsPtr->record_filter->record_type =
          toNFCRecordType(watchOptions.recordType());
    }

    return watchOptionsPtr;
  }
};

}  // namespace mojo

namespace blink {
namespace {

bool IsValidTextRecord(const NFCRecord& record) {
  v8::Local<v8::Value> value = record.data().V8Value();
  if (!value->IsString() &&
      !(value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value())))
    return false;

  if (record.hasMediaType() &&
      !record.mediaType().StartsWith(kPlainTextMimePrefix,
                                     kTextCaseUnicodeInsensitive))
    return false;

  return true;
}

bool IsValidURLRecord(const NFCRecord& record) {
  if (!record.data().V8Value()->IsString())
    return false;

  blink::V8StringResource<> string_resource = record.data().V8Value();
  if (!string_resource.Prepare())
    return false;

  return KURL(KURL(), string_resource).IsValid();
}

bool IsValidJSONRecord(const NFCRecord& record) {
  v8::Local<v8::Value> value = record.data().V8Value();
  if (!value->IsObject() || value->IsArrayBuffer())
    return false;

  if (record.hasMediaType() && !record.mediaType().StartsWith(
                                   kJsonMimePrefix, kTextCaseASCIIInsensitive))
    return false;

  return true;
}

bool IsValidOpaqueRecord(const NFCRecord& record) {
  return record.data().V8Value()->IsArrayBuffer();
}

bool IsValidNFCRecord(const NFCRecord& record) {
  device::nfc::mojom::blink::NFCRecordType type;
  if (record.hasRecordType()) {
    type = mojo::toNFCRecordType(record.recordType());
  } else {
    type = mojo::deduceRecordTypeFromDataType(record);

    // https://w3c.github.io/web-nfc/#creating-web-nfc-message
    // If NFCRecord.recordType is not set and record type cannot be deduced
    // from NFCRecord.data, reject promise with SyntaxError.
    if (type == device::nfc::mojom::blink::NFCRecordType::EMPTY)
      return false;
  }

  // Non-empty records must have data.
  if (!record.hasData() &&
      (type != device::nfc::mojom::blink::NFCRecordType::EMPTY)) {
    return false;
  }

  switch (type) {
    case device::nfc::mojom::blink::NFCRecordType::TEXT:
      return IsValidTextRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::URL:
      return IsValidURLRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::JSON:
      return IsValidJSONRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::OPAQUE_RECORD:
      return IsValidOpaqueRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::EMPTY:
      return !record.hasData() && record.mediaType().IsEmpty();
  }

  NOTREACHED();
  return false;
}

DOMException* IsValidNFCRecordArray(const HeapVector<NFCRecord>& records) {
  // https://w3c.github.io/web-nfc/#the-push-method
  // If NFCMessage.data is empty, reject promise with SyntaxError
  if (records.IsEmpty())
    return DOMException::Create(kSyntaxError);

  for (const auto& record : records) {
    if (!IsValidNFCRecord(record))
      return DOMException::Create(kSyntaxError);
  }

  return nullptr;
}

DOMException* IsValidNFCPushMessage(const NFCPushMessage& message) {
  if (!message.isNFCMessage() && !message.isString() &&
      !message.isArrayBuffer())
    return DOMException::Create(kTypeMismatchError);

  if (message.isNFCMessage()) {
    if (!message.getAsNFCMessage().hasData())
      return DOMException::Create(kTypeMismatchError);

    return IsValidNFCRecordArray(message.getAsNFCMessage().data());
  }

  return nullptr;
}

bool SetURL(const String& origin,
            device::nfc::mojom::blink::NFCMessagePtr& message) {
  KURL origin_url(kParsedURLString, origin);

  if (!message->url.IsEmpty() && origin_url.CanSetPathname()) {
    origin_url.SetPath(message->url);
  }

  message->url = origin_url;
  return origin_url.IsValid();
}

String ToNFCRecordType(const device::nfc::mojom::blink::NFCRecordType& type) {
  switch (type) {
    case device::nfc::mojom::blink::NFCRecordType::TEXT:
      return "text";
    case device::nfc::mojom::blink::NFCRecordType::URL:
      return "url";
    case device::nfc::mojom::blink::NFCRecordType::JSON:
      return "json";
    case device::nfc::mojom::blink::NFCRecordType::OPAQUE_RECORD:
      return "opaque";
    case device::nfc::mojom::blink::NFCRecordType::EMPTY:
      return "empty";
  }

  NOTREACHED();
  return String();
}

v8::Local<v8::Value> ToV8(
    ScriptState* script_state,
    const device::nfc::mojom::blink::NFCRecordPtr& record) {
  switch (record->record_type) {
    case device::nfc::mojom::blink::NFCRecordType::TEXT:
    case device::nfc::mojom::blink::NFCRecordType::URL:
    case device::nfc::mojom::blink::NFCRecordType::JSON: {
      String string_data;
      if (!record->data.IsEmpty()) {
        string_data = String::FromUTF8WithLatin1Fallback(
            static_cast<unsigned char*>(&record->data.front()),
            record->data.size());
      }

      v8::Isolate* isolate = script_state->GetIsolate();
      v8::Local<v8::String> string = V8String(isolate, string_data);

      // Stringified JSON must be converted back to an Object.
      if (record->record_type ==
          device::nfc::mojom::blink::NFCRecordType::JSON) {
        v8::Local<v8::Value> json_object;
        v8::TryCatch try_catch(isolate);
        if (!V8Call(v8::JSON::Parse(isolate, string), json_object, try_catch)) {
          return v8::Null(isolate);
        }

        return json_object;
      }

      return string;
    }

    case device::nfc::mojom::blink::NFCRecordType::OPAQUE_RECORD: {
      if (!record->data.IsEmpty()) {
        DOMArrayBuffer* buffer = DOMArrayBuffer::Create(
            static_cast<void*>(&record->data.front()), record->data.size());
        return ToV8(buffer, script_state->GetContext()->Global(),
                    script_state->GetIsolate());
      }

      return v8::Null(script_state->GetIsolate());
    }

    case device::nfc::mojom::blink::NFCRecordType::EMPTY:
      return v8::Null(script_state->GetIsolate());
  }

  NOTREACHED();
  return v8::Local<v8::Value>();
}

NFCRecord ToNFCRecord(ScriptState* script_state,
                      const device::nfc::mojom::blink::NFCRecordPtr& record) {
  NFCRecord nfc_record;
  nfc_record.setMediaType(record->media_type);
  nfc_record.setRecordType(ToNFCRecordType(record->record_type));
  nfc_record.setData(ScriptValue(script_state, ToV8(script_state, record)));
  return nfc_record;
}

NFCMessage ToNFCMessage(
    ScriptState* script_state,
    const device::nfc::mojom::blink::NFCMessagePtr& message) {
  NFCMessage nfc_message;
  nfc_message.setURL(message->url);
  blink::HeapVector<NFCRecord> records;
  for (size_t i = 0; i < message->data.size(); ++i)
    records.push_back(ToNFCRecord(script_state, message->data[i]));
  nfc_message.setData(records);
  return nfc_message;
}

size_t GetNFCMessageSize(
    const device::nfc::mojom::blink::NFCMessagePtr& message) {
  size_t message_size = message->url.CharactersSizeInBytes();
  for (size_t i = 0; i < message->data.size(); ++i) {
    message_size += message->data[i]->media_type.CharactersSizeInBytes();
    message_size += message->data[i]->data.size();
  }
  return message_size;
}

}  // namespace

NFC::NFC(LocalFrame* frame)
    : PageVisibilityObserver(frame->GetPage()),
      ContextLifecycleObserver(frame->GetDocument()),
      client_(this) {
  frame->GetInterfaceProvider()->GetInterface(mojo::MakeRequest(&nfc_));
  nfc_.set_connection_error_handler(ConvertToBaseCallback(
      WTF::Bind(&NFC::OnConnectionError, WrapWeakPersistent(this))));
  nfc_->SetClient(client_.CreateInterfacePtrAndBind());
}

NFC* NFC::Create(LocalFrame* frame) {
  NFC* nfc = new NFC(frame);
  return nfc;
}

NFC::~NFC() {
  // |m_nfc| may hold persistent handle to |this| object, therefore, there
  // should be no more outstanding requests when NFC object is destructed.
  DCHECK(requests_.IsEmpty());
}

void NFC::Dispose() {
  client_.Close();
}

void NFC::ContextDestroyed(ExecutionContext*) {
  nfc_.reset();
  requests_.Clear();
  callbacks_.Clear();
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
// https://w3c.github.io/web-nfc/#dom-nfc-push
ScriptPromise NFC::push(ScriptState* script_state,
                        const NFCPushMessage& push_message,
                        const NFCPushOptions& options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  DOMException* exception = IsValidNFCPushMessage(push_message);
  if (exception)
    return ScriptPromise::RejectWithDOMException(script_state, exception);

  device::nfc::mojom::blink::NFCMessagePtr message =
      device::nfc::mojom::blink::NFCMessage::From(push_message);
  if (!message)
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSyntaxError));

  if (!SetURL(
          ExecutionContext::From(script_state)->GetSecurityOrigin()->ToString(),
          message))
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSyntaxError));

  if (GetNFCMessageSize(message) >
      device::nfc::mojom::blink::NFCMessage::kMaxSize) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  auto callback = ConvertToBaseCallback(WTF::Bind(&NFC::OnRequestCompleted,
                                                  WrapPersistent(this),
                                                  WrapPersistent(resolver)));
  nfc_->Push(std::move(message),
             device::nfc::mojom::blink::NFCPushOptions::From(options),
             callback);

  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelpush
ScriptPromise NFC::cancelPush(ScriptState* script_state, const String& target) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  auto callback = ConvertToBaseCallback(WTF::Bind(&NFC::OnRequestCompleted,
                                                  WrapPersistent(this),
                                                  WrapPersistent(resolver)));
  nfc_->CancelPush(mojo::toNFCPushTarget(target), callback);

  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#watching-for-content
// https://w3c.github.io/web-nfc/#dom-nfc-watch
ScriptPromise NFC::watch(ScriptState* script_state,
                         MessageCallback* callback,
                         const NFCWatchOptions& options) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  callback->SetScriptState(script_state);
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  auto watch_callback = ConvertToBaseCallback(
      WTF::Bind(&NFC::OnWatchRegistered, WrapPersistent(this),
                WrapPersistent(callback), WrapPersistent(resolver)));
  nfc_->Watch(device::nfc::mojom::blink::NFCWatchOptions::From(options),
              watch_callback);
  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
ScriptPromise NFC::cancelWatch(ScriptState* script_state, long id) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  if (id) {
    callbacks_.erase(id);
  } else {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotFoundError));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  nfc_->CancelWatch(id, ConvertToBaseCallback(WTF::Bind(
                            &NFC::OnRequestCompleted, WrapPersistent(this),
                            WrapPersistent(resolver))));
  return resolver->Promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
// If watchId is not provided to nfc.cancelWatch, cancel all watch operations.
ScriptPromise NFC::cancelWatch(ScriptState* script_state) {
  ScriptPromise promise = RejectIfNotSupported(script_state);
  if (!promise.IsEmpty())
    return promise;

  callbacks_.Clear();
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  requests_.insert(resolver);
  nfc_->CancelAllWatches(ConvertToBaseCallback(
      WTF::Bind(&NFC::OnRequestCompleted, WrapPersistent(this),
                WrapPersistent(resolver))));
  return resolver->Promise();
}

void NFC::PageVisibilityChanged() {
  // If service is not initialized, there cannot be any pending NFC activities
  if (!nfc_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (GetPage()->VisibilityState() == kPageVisibilityStateVisible)
    nfc_->ResumeNFCOperations();
  else
    nfc_->SuspendNFCOperations();
}

void NFC::OnRequestCompleted(ScriptPromiseResolver* resolver,
                             device::nfc::mojom::blink::NFCErrorPtr error) {
  if (!requests_.Contains(resolver))
    return;

  requests_.erase(resolver);
  if (error.is_null())
    resolver->Resolve();
  else
    resolver->Reject(NFCError::Take(resolver, error->error_type));
}

void NFC::OnConnectionError() {
  nfc_.reset();
  callbacks_.Clear();

  // If NFCService is not available or disappears when NFC hardware is
  // disabled, reject promise with NotSupportedError exception.
  for (ScriptPromiseResolver* resolver : requests_)
    resolver->Reject(NFCError::Take(
        resolver, device::nfc::mojom::blink::NFCErrorType::NOT_SUPPORTED));

  requests_.Clear();
}

void NFC::OnWatch(const WTF::Vector<uint32_t>& ids,
                  device::nfc::mojom::blink::NFCMessagePtr message) {
  for (const auto& id : ids) {
    auto it = callbacks_.Find(id);
    if (it != callbacks_.end()) {
      MessageCallback* callback = it->value;
      ScriptState* script_state = callback->GetScriptState();
      DCHECK(script_state);
      ScriptState::Scope scope(script_state);
      NFCMessage nfc_message = ToNFCMessage(script_state, message);
      callback->handleMessage(nfc_message);
    }
  }
}

ScriptPromise NFC::RejectIfNotSupported(ScriptState* script_state) {
  String error_message;
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!context->IsSecureContext(error_message)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kSecurityError, error_message));
  }

  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (!ToDocument(context)->domWindow()->GetFrame() ||
      !ToDocument(context)->GetFrame()->IsMainFrame()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kSecurityError,
                             "Must be in a top-level browsing context."));
  }

  if (!nfc_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError));
  }

  return ScriptPromise();
}

void NFC::OnWatchRegistered(MessageCallback* callback,
                            ScriptPromiseResolver* resolver,
                            uint32_t id,
                            device::nfc::mojom::blink::NFCErrorPtr error) {
  requests_.erase(resolver);

  // Invalid id was returned.
  // https://w3c.github.io/web-nfc/#dom-nfc-watch
  // 8. If the request fails, reject promise with "NotSupportedError"
  // and abort these steps.
  if (!id) {
    resolver->Reject(NFCError::Take(
        resolver, device::nfc::mojom::blink::NFCErrorType::NOT_SUPPORTED));
    return;
  }

  if (error.is_null()) {
    callbacks_.insert(id, callback);
    resolver->Resolve(id);
  } else {
    resolver->Reject(NFCError::Take(resolver, error->error_type));
  }
}

DEFINE_TRACE(NFC) {
  PageVisibilityObserver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(requests_);
  visitor->Trace(callbacks_);
}

}  // namespace blink
