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
    v8::Local<v8::Value> value = record.data().v8Value();

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
      recordMediaType.isEmpty() ? defaultMediaType : recordMediaType;
}

template <>
struct TypeConverter<mojo::WTFArray<uint8_t>, WTF::String> {
  static mojo::WTFArray<uint8_t> Convert(const WTF::String& string) {
    WTF::CString utf8String = string.utf8();
    WTF::Vector<uint8_t> array;
    array.append(utf8String.data(), utf8String.length());
    return mojo::WTFArray<uint8_t>(std::move(array));
  }
};

template <>
struct TypeConverter<mojo::WTFArray<uint8_t>, blink::DOMArrayBuffer*> {
  static mojo::WTFArray<uint8_t> Convert(blink::DOMArrayBuffer* buffer) {
    WTF::Vector<uint8_t> array;
    array.append(static_cast<uint8_t*>(buffer->data()), buffer->byteLength());
    return mojo::WTFArray<uint8_t>(std::move(array));
  }
};

template <>
struct TypeConverter<NFCRecordPtr, WTF::String> {
  static NFCRecordPtr Convert(const WTF::String& string) {
    NFCRecordPtr record = NFCRecord::New();
    record->record_type = NFCRecordType::TEXT;
    record->media_type = kPlainTextMimeType;
    record->media_type.append(kCharSetUTF8);
    record->data = mojo::WTFArray<uint8_t>::From(string).PassStorage();
    return record;
  }
};

template <>
struct TypeConverter<NFCRecordPtr, blink::DOMArrayBuffer*> {
  static NFCRecordPtr Convert(blink::DOMArrayBuffer* buffer) {
    NFCRecordPtr record = NFCRecord::New();
    record->record_type = NFCRecordType::OPAQUE_RECORD;
    record->media_type = kOpaqueMimeType;
    record->data = mojo::WTFArray<uint8_t>::From(buffer).PassStorage();
    return record;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, WTF::String> {
  static NFCMessagePtr Convert(const WTF::String& string) {
    NFCMessagePtr message = NFCMessage::New();
    message->data.append(NFCRecord::From(string));
    return message;
  }
};

template <>
struct TypeConverter<mojo::WTFArray<uint8_t>, blink::ScriptValue> {
  static mojo::WTFArray<uint8_t> Convert(
      const blink::ScriptValue& scriptValue) {
    v8::Local<v8::Value> value = scriptValue.v8Value();

    if (value->IsNumber())
      return mojo::WTFArray<uint8_t>::From(
          WTF::String::number(value.As<v8::Number>()->Value()));

    if (value->IsString()) {
      blink::V8StringResource<> stringResource = value;
      if (stringResource.prepare())
        return mojo::WTFArray<uint8_t>::From<WTF::String>(stringResource);
    }

    if (value->IsObject() && !value->IsArray() && !value->IsArrayBuffer()) {
      v8::Local<v8::String> jsonString;
      if (v8::JSON::Stringify(scriptValue.context(), value.As<v8::Object>())
              .ToLocal(&jsonString)) {
        WTF::String wtfString = blink::v8StringToWebCoreString<WTF::String>(
            jsonString, blink::DoNotExternalize);
        return mojo::WTFArray<uint8_t>::From(wtfString);
      }
    }

    if (value->IsArrayBuffer())
      return mojo::WTFArray<uint8_t>::From(
          blink::V8ArrayBuffer::toImpl(value.As<v8::Object>()));

    return nullptr;
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
        recordPtr->media_type.append(kCharSetUTF8);
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

    auto recordData = mojo::WTFArray<uint8_t>::From(record.data());
    // If JS object cannot be converted to uint8_t array, return null,
    // interrupt NFCMessage conversion algorithm and reject promise with
    // SyntaxError exception.
    if (recordData.is_null())
      return nullptr;

    recordPtr->data = recordData.PassStorage();
    return recordPtr;
  }
};

template <>
struct TypeConverter<NFCMessagePtr, blink::NFCMessage> {
  static NFCMessagePtr Convert(const blink::NFCMessage& message) {
    NFCMessagePtr messagePtr = NFCMessage::New();
    messagePtr->url = message.url();
    messagePtr->data.resize(message.data().size());
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
    message->data.append(NFCRecord::From(buffer));
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

bool isValidTextRecord(const NFCRecord& record) {
  v8::Local<v8::Value> value = record.data().v8Value();
  if (!value->IsString() &&
      !(value->IsNumber() && !std::isnan(value.As<v8::Number>()->Value())))
    return false;

  if (record.hasMediaType() &&
      !record.mediaType().startsWith(kPlainTextMimePrefix,
                                     TextCaseUnicodeInsensitive))
    return false;

  return true;
}

bool isValidURLRecord(const NFCRecord& record) {
  if (!record.data().v8Value()->IsString())
    return false;

  blink::V8StringResource<> stringResource = record.data().v8Value();
  if (!stringResource.prepare())
    return false;

  return KURL(KURL(), stringResource).isValid();
}

bool isValidJSONRecord(const NFCRecord& record) {
  v8::Local<v8::Value> value = record.data().v8Value();
  if (!value->IsObject() || value->IsArrayBuffer())
    return false;

  if (record.hasMediaType() &&
      !record.mediaType().startsWith(kJsonMimePrefix, TextCaseASCIIInsensitive))
    return false;

  return true;
}

bool isValidOpaqueRecord(const NFCRecord& record) {
  return record.data().v8Value()->IsArrayBuffer();
}

bool isValidNFCRecord(const NFCRecord& record) {
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
      return isValidTextRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::URL:
      return isValidURLRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::JSON:
      return isValidJSONRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::OPAQUE_RECORD:
      return isValidOpaqueRecord(record);
    case device::nfc::mojom::blink::NFCRecordType::EMPTY:
      return !record.hasData() && record.mediaType().isEmpty();
  }

  NOTREACHED();
  return false;
}

DOMException* isValidNFCRecordArray(const HeapVector<NFCRecord>& records) {
  // https://w3c.github.io/web-nfc/#the-push-method
  // If NFCMessage.data is empty, reject promise with SyntaxError
  if (records.isEmpty())
    return DOMException::create(SyntaxError);

  for (const auto& record : records) {
    if (!isValidNFCRecord(record))
      return DOMException::create(SyntaxError);
  }

  return nullptr;
}

DOMException* isValidNFCPushMessage(const NFCPushMessage& message) {
  if (!message.isNFCMessage() && !message.isString() &&
      !message.isArrayBuffer())
    return DOMException::create(TypeMismatchError);

  if (message.isNFCMessage()) {
    if (!message.getAsNFCMessage().hasData())
      return DOMException::create(TypeMismatchError);

    return isValidNFCRecordArray(message.getAsNFCMessage().data());
  }

  return nullptr;
}

bool setURL(const String& origin,
            device::nfc::mojom::blink::NFCMessagePtr& message) {
  KURL originURL(ParsedURLString, origin);

  if (!message->url.isEmpty() && originURL.canSetPathname()) {
    originURL.setPath(message->url);
  }

  message->url = originURL;
  return originURL.isValid();
}

String toNFCRecordType(const device::nfc::mojom::blink::NFCRecordType& type) {
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

v8::Local<v8::Value> toV8(
    ScriptState* scriptState,
    const device::nfc::mojom::blink::NFCRecordPtr& record) {
  switch (record->record_type) {
    case device::nfc::mojom::blink::NFCRecordType::TEXT:
    case device::nfc::mojom::blink::NFCRecordType::URL:
    case device::nfc::mojom::blink::NFCRecordType::JSON: {
      String stringData;
      if (!record->data.isEmpty()) {
        stringData = String::fromUTF8WithLatin1Fallback(
            static_cast<unsigned char*>(&record->data.front()),
            record->data.size());
      }

      v8::Isolate* isolate = scriptState->isolate();
      v8::Local<v8::String> string = v8String(isolate, stringData);

      // Stringified JSON must be converted back to an Object.
      if (record->record_type ==
          device::nfc::mojom::blink::NFCRecordType::JSON) {
        v8::Local<v8::Value> jsonObject;
        v8::TryCatch tryCatch(isolate);
        if (!v8Call(v8::JSON::Parse(isolate, string), jsonObject, tryCatch)) {
          return v8::Null(isolate);
        }

        return jsonObject;
      }

      return string;
    }

    case device::nfc::mojom::blink::NFCRecordType::OPAQUE_RECORD: {
      if (!record->data.isEmpty()) {
        DOMArrayBuffer* buffer = DOMArrayBuffer::create(
            static_cast<void*>(&record->data.front()), record->data.size());
        return toV8(buffer, scriptState->context()->Global(),
                    scriptState->isolate());
      }

      return v8::Null(scriptState->isolate());
    }

    case device::nfc::mojom::blink::NFCRecordType::EMPTY:
      return v8::Null(scriptState->isolate());
  }

  NOTREACHED();
  return v8::Local<v8::Value>();
}

NFCRecord toNFCRecord(ScriptState* scriptState,
                      const device::nfc::mojom::blink::NFCRecordPtr& record) {
  NFCRecord nfcRecord;
  nfcRecord.setMediaType(record->media_type);
  nfcRecord.setRecordType(toNFCRecordType(record->record_type));
  nfcRecord.setData(ScriptValue(scriptState, toV8(scriptState, record)));
  return nfcRecord;
}

NFCMessage toNFCMessage(
    ScriptState* scriptState,
    const device::nfc::mojom::blink::NFCMessagePtr& message) {
  NFCMessage nfcMessage;
  nfcMessage.setURL(message->url);
  blink::HeapVector<NFCRecord> records;
  for (size_t i = 0; i < message->data.size(); ++i)
    records.append(toNFCRecord(scriptState, message->data[i]));
  nfcMessage.setData(records);
  return nfcMessage;
}

size_t getNFCMessageSize(
    const device::nfc::mojom::blink::NFCMessagePtr& message) {
  size_t messageSize = message->url.charactersSizeInBytes();
  for (size_t i = 0; i < message->data.size(); ++i) {
    messageSize += message->data[i]->media_type.charactersSizeInBytes();
    messageSize += message->data[i]->data.size();
  }
  return messageSize;
}

}  // namespace

NFC::NFC(LocalFrame* frame)
    : PageVisibilityObserver(frame->page()),
      ContextLifecycleObserver(frame->document()),
      m_client(this) {
  ThreadState::current()->registerPreFinalizer(this);
  frame->interfaceProvider()->getInterface(mojo::GetProxy(&m_nfc));
  m_nfc.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&NFC::OnConnectionError, wrapWeakPersistent(this))));
  m_nfc->SetClient(m_client.CreateInterfacePtrAndBind());
}

NFC* NFC::create(LocalFrame* frame) {
  NFC* nfc = new NFC(frame);
  return nfc;
}

NFC::~NFC() {
  // |m_nfc| may hold persistent handle to |this| object, therefore, there
  // should be no more outstanding requests when NFC object is destructed.
  DCHECK(m_requests.isEmpty());
}

void NFC::dispose() {
  m_client.Close();
}

void NFC::contextDestroyed() {
  m_nfc.reset();
  m_requests.clear();
  m_callbacks.clear();
}

// https://w3c.github.io/web-nfc/#writing-or-pushing-content
// https://w3c.github.io/web-nfc/#dom-nfc-push
ScriptPromise NFC::push(ScriptState* scriptState,
                        const NFCPushMessage& pushMessage,
                        const NFCPushOptions& options) {
  ScriptPromise promise = rejectIfNotSupported(scriptState);
  if (!promise.isEmpty())
    return promise;

  DOMException* exception = isValidNFCPushMessage(pushMessage);
  if (exception)
    return ScriptPromise::rejectWithDOMException(scriptState, exception);

  device::nfc::mojom::blink::NFCMessagePtr message =
      device::nfc::mojom::blink::NFCMessage::From(pushMessage);
  if (!message)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SyntaxError));

  if (!setURL(
          scriptState->getExecutionContext()->getSecurityOrigin()->toString(),
          message))
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SyntaxError));

  if (getNFCMessageSize(message) >
      device::nfc::mojom::blink::NFCMessage::kMaxSize) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotSupportedError));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  m_requests.add(resolver);
  auto callback = convertToBaseCallback(WTF::bind(&NFC::OnRequestCompleted,
                                                  wrapPersistent(this),
                                                  wrapPersistent(resolver)));
  m_nfc->Push(std::move(message),
              device::nfc::mojom::blink::NFCPushOptions::From(options),
              callback);

  return resolver->promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelpush
ScriptPromise NFC::cancelPush(ScriptState* scriptState, const String& target) {
  ScriptPromise promise = rejectIfNotSupported(scriptState);
  if (!promise.isEmpty())
    return promise;

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  m_requests.add(resolver);
  auto callback = convertToBaseCallback(WTF::bind(&NFC::OnRequestCompleted,
                                                  wrapPersistent(this),
                                                  wrapPersistent(resolver)));
  m_nfc->CancelPush(mojo::toNFCPushTarget(target), callback);

  return resolver->promise();
}

// https://w3c.github.io/web-nfc/#watching-for-content
// https://w3c.github.io/web-nfc/#dom-nfc-watch
ScriptPromise NFC::watch(ScriptState* scriptState,
                         MessageCallback* callback,
                         const NFCWatchOptions& options) {
  ScriptPromise promise = rejectIfNotSupported(scriptState);
  if (!promise.isEmpty())
    return promise;

  callback->setScriptState(scriptState);
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  m_requests.add(resolver);
  auto watchCallback = convertToBaseCallback(
      WTF::bind(&NFC::OnWatchRegistered, wrapPersistent(this),
                wrapPersistent(callback), wrapPersistent(resolver)));
  m_nfc->Watch(device::nfc::mojom::blink::NFCWatchOptions::From(options),
               watchCallback);
  return resolver->promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
ScriptPromise NFC::cancelWatch(ScriptState* scriptState, long id) {
  ScriptPromise promise = rejectIfNotSupported(scriptState);
  if (!promise.isEmpty())
    return promise;

  if (id) {
    m_callbacks.remove(id);
  } else {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotFoundError));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  m_requests.add(resolver);
  m_nfc->CancelWatch(id, convertToBaseCallback(WTF::bind(
                             &NFC::OnRequestCompleted, wrapPersistent(this),
                             wrapPersistent(resolver))));
  return resolver->promise();
}

// https://w3c.github.io/web-nfc/#dom-nfc-cancelwatch
// If watchId is not provided to nfc.cancelWatch, cancel all watch operations.
ScriptPromise NFC::cancelWatch(ScriptState* scriptState) {
  ScriptPromise promise = rejectIfNotSupported(scriptState);
  if (!promise.isEmpty())
    return promise;

  m_callbacks.clear();
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  m_requests.add(resolver);
  m_nfc->CancelAllWatches(convertToBaseCallback(
      WTF::bind(&NFC::OnRequestCompleted, wrapPersistent(this),
                wrapPersistent(resolver))));
  return resolver->promise();
}

void NFC::pageVisibilityChanged() {
  // If service is not initialized, there cannot be any pending NFC activities
  if (!m_nfc)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  if (page()->visibilityState() == PageVisibilityStateVisible)
    m_nfc->ResumeNFCOperations();
  else
    m_nfc->SuspendNFCOperations();
}

void NFC::OnRequestCompleted(ScriptPromiseResolver* resolver,
                             device::nfc::mojom::blink::NFCErrorPtr error) {
  if (!m_requests.contains(resolver))
    return;

  m_requests.remove(resolver);
  if (error.is_null())
    resolver->resolve();
  else
    resolver->reject(NFCError::take(resolver, error->error_type));
}

void NFC::OnConnectionError() {
  if (!Platform::current()) {
    // TODO(rockot): Clean this up once renderer shutdown sequence is fixed.
    return;
  }

  m_nfc.reset();
  m_callbacks.clear();

  // If NFCService is not available or disappears when NFC hardware is
  // disabled, reject promise with NotSupportedError exception.
  for (ScriptPromiseResolver* resolver : m_requests)
    resolver->reject(NFCError::take(
        resolver, device::nfc::mojom::blink::NFCErrorType::NOT_SUPPORTED));

  m_requests.clear();
}

void NFC::OnWatch(const WTF::Vector<uint32_t>& ids,
                  device::nfc::mojom::blink::NFCMessagePtr message) {
  for (const auto& id : ids) {
    auto it = m_callbacks.find(id);
    if (it != m_callbacks.end()) {
      MessageCallback* callback = it->value;
      ScriptState* scriptState = callback->getScriptState();
      DCHECK(scriptState);
      ScriptState::Scope scope(scriptState);
      NFCMessage nfcMessage = toNFCMessage(scriptState, message);
      callback->handleMessage(nfcMessage);
    }
  }
}

ScriptPromise NFC::rejectIfNotSupported(ScriptState* scriptState) {
  String errorMessage;
  if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(SecurityError, errorMessage));
  }

  if (!m_nfc) {
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(NotSupportedError));
  }

  return ScriptPromise();
}

void NFC::OnWatchRegistered(MessageCallback* callback,
                            ScriptPromiseResolver* resolver,
                            uint32_t id,
                            device::nfc::mojom::blink::NFCErrorPtr error) {
  m_requests.remove(resolver);

  // Invalid id was returned.
  // https://w3c.github.io/web-nfc/#dom-nfc-watch
  // 8. If the request fails, reject promise with "NotSupportedError"
  // and abort these steps.
  if (!id) {
    resolver->reject(NFCError::take(
        resolver, device::nfc::mojom::blink::NFCErrorType::NOT_SUPPORTED));
    return;
  }

  if (error.is_null()) {
    m_callbacks.add(id, callback);
    resolver->resolve(id);
  } else {
    resolver->reject(NFCError::take(resolver, error->error_type));
  }
}

DEFINE_TRACE(NFC) {
  PageVisibilityObserver::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_requests);
  visitor->trace(m_callbacks);
}

}  // namespace blink
