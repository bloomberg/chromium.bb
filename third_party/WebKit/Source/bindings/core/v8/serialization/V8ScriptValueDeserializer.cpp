// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"

#include "bindings/core/v8/ToV8ForCore.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/geometry/DOMMatrix.h"
#include "core/geometry/DOMMatrixReadOnly.h"
#include "core/geometry/DOMPoint.h"
#include "core/geometry/DOMPointInit.h"
#include "core/geometry/DOMPointReadOnly.h"
#include "core/geometry/DOMQuad.h"
#include "core/geometry/DOMRect.h"
#include "core/geometry/DOMRectReadOnly.h"
#include "core/html/ImageData.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/DateMath.h"
#include "public/platform/WebBlobInfo.h"

namespace blink {

namespace {

// The "Blink-side" serialization version, which defines how Blink will behave
// during the serialization process. The serialization format has two
// "envelopes": an outer one controlled by Blink and an inner one by V8.
//
// They are formatted as follows:
// [version tag] [Blink version] [version tag] [v8 version] ...
//
// Before version 16, there was only a single envelope and the version number
// for both parts was always equal.
//
// See also V8ScriptValueDeserializer.cpp.
const uint32_t kMinVersionForSeparateEnvelope = 16;

// Returns the number of bytes consumed reading the Blink version envelope, and
// sets |*version| to the version. If no Blink envelope was detected, zero is
// returned.
size_t ReadVersionEnvelope(SerializedScriptValue* serialized_script_value,
                           uint32_t* out_version) {
  const uint8_t* raw_data = serialized_script_value->Data();
  const size_t length = serialized_script_value->DataLengthInBytes();
  if (!length || raw_data[0] != kVersionTag)
    return 0;

  // Read a 32-bit unsigned integer from varint encoding.
  uint32_t version = 0;
  size_t i = 1;
  unsigned shift = 0;
  bool has_another_byte;
  do {
    if (i >= length)
      return 0;
    uint8_t byte = raw_data[i];
    if (LIKELY(shift < 32)) {
      version |= static_cast<uint32_t>(byte & 0x7f) << shift;
      shift += 7;
    }
    has_another_byte = byte & 0x80;
    i++;
  } while (has_another_byte);

  // If the version in the envelope is too low, this was not a Blink version
  // envelope.
  if (version < kMinVersionForSeparateEnvelope)
    return 0;

  // Otherwise, we did read the envelope. Hurray!
  *out_version = version;
  return i;
}

}  // namespace

V8ScriptValueDeserializer::V8ScriptValueDeserializer(
    RefPtr<ScriptState> script_state,
    RefPtr<SerializedScriptValue> serialized_script_value,
    const Options& options)
    : script_state_(std::move(script_state)),
      serialized_script_value_(std::move(serialized_script_value)),
      deserializer_(script_state_->GetIsolate(),
                    serialized_script_value_->Data(),
                    serialized_script_value_->DataLengthInBytes(),
                    this),
      transferred_message_ports_(options.message_ports),
      blob_info_array_(options.blob_info) {
  deserializer_.SetSupportsLegacyWireFormat(true);
  deserializer_.SetExpectInlineWasm(options.read_wasm_from_stream);
}

v8::Local<v8::Value> V8ScriptValueDeserializer::Deserialize() {
#if DCHECK_IS_ON()
  DCHECK(!deserialize_invoked_);
  deserialize_invoked_ = true;
#endif

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::EscapableHandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = script_state_->GetContext();

  size_t version_envelope_size =
      ReadVersionEnvelope(serialized_script_value_.Get(), &version_);
  if (version_envelope_size) {
    const void* blink_envelope;
    bool read_envelope = ReadRawBytes(version_envelope_size, &blink_envelope);
    DCHECK(read_envelope);
    DCHECK_GE(version_, kMinVersionForSeparateEnvelope);
  } else {
    DCHECK_EQ(version_, 0u);
  }

  bool read_header;
  if (!deserializer_.ReadHeader(context).To(&read_header))
    return v8::Null(isolate);
  DCHECK(read_header);

  // If there was no Blink envelope earlier, Blink shares the wire format
  // version from the V8 header.
  if (!version_)
    version_ = deserializer_.GetWireFormatVersion();

  // Prepare to transfer the provided transferables.
  Transfer();

  v8::Local<v8::Value> value;
  if (!deserializer_.ReadValue(context).ToLocal(&value))
    return v8::Null(isolate);
  return scope.Escape(value);
}

void V8ScriptValueDeserializer::Transfer() {
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Object> creation_context = context->Global();

  // Receive the transfer, making the received objects available.
  serialized_script_value_->ReceiveTransfer();

  // Transfer array buffers.
  const auto& array_buffers = serialized_script_value_->ReceivedArrayBuffers();
  for (unsigned i = 0; i < array_buffers.size(); i++) {
    DOMArrayBufferBase* array_buffer = array_buffers.at(i);
    v8::Local<v8::Value> wrapper =
        ToV8(array_buffer, creation_context, isolate);
    if (array_buffer->IsShared()) {
      DCHECK(wrapper->IsSharedArrayBuffer());
      deserializer_.TransferSharedArrayBuffer(
          i, v8::Local<v8::SharedArrayBuffer>::Cast(wrapper));
    } else {
      DCHECK(wrapper->IsArrayBuffer());
      deserializer_.TransferArrayBuffer(
          i, v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    }
  }
}

bool V8ScriptValueDeserializer::ReadUTF8String(String* string) {
  uint32_t utf8_length = 0;
  const void* utf8_data = nullptr;
  if (!ReadUint32(&utf8_length) || !ReadRawBytes(utf8_length, &utf8_data))
    return false;
  *string =
      String::FromUTF8(reinterpret_cast<const LChar*>(utf8_data), utf8_length);
  return true;
}

ScriptWrappable* V8ScriptValueDeserializer::ReadDOMObject(
    SerializationTag tag) {
  switch (tag) {
    case kBlobTag: {
      if (Version() < 3)
        return nullptr;
      String uuid, type;
      uint64_t size;
      if (!ReadUTF8String(&uuid) || !ReadUTF8String(&type) ||
          !ReadUint64(&size))
        return nullptr;
      return Blob::Create(GetOrCreateBlobDataHandle(uuid, type, size));
    }
    case kBlobIndexTag: {
      if (Version() < 6 || !blob_info_array_)
        return nullptr;
      uint32_t index = 0;
      if (!ReadUint32(&index) || index >= blob_info_array_->size())
        return nullptr;
      const WebBlobInfo& info = (*blob_info_array_)[index];
      return Blob::Create(
          GetOrCreateBlobDataHandle(info.Uuid(), info.GetType(), info.size()));
    }
    case kFileTag:
      return ReadFile();
    case kFileIndexTag:
      return ReadFileIndex();
    case kFileListTag: {
      // This does not presently deduplicate a File object and its entry in a
      // FileList, which is non-standard behavior.
      uint32_t length;
      if (!ReadUint32(&length))
        return nullptr;
      FileList* file_list = FileList::Create();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = ReadFile())
          file_list->Append(file);
        else
          return nullptr;
      }
      return file_list;
    }
    case kFileListIndexTag: {
      // This does not presently deduplicate a File object and its entry in a
      // FileList, which is non-standard behavior.
      uint32_t length;
      if (!ReadUint32(&length))
        return nullptr;
      FileList* file_list = FileList::Create();
      for (uint32_t i = 0; i < length; i++) {
        if (File* file = ReadFileIndex())
          file_list->Append(file);
        else
          return nullptr;
      }
      return file_list;
    }
    case kImageBitmapTag: {
      uint32_t origin_clean = 0, is_premultiplied = 0, width = 0, height = 0,
               pixel_length = 0;
      const void* pixels = nullptr;
      if (!ReadUint32(&origin_clean) || origin_clean > 1 ||
          !ReadUint32(&is_premultiplied) || is_premultiplied > 1 ||
          !ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&pixel_length) || !ReadRawBytes(pixel_length, &pixels))
        return nullptr;
      CheckedNumeric<uint32_t> computed_pixel_length = width;
      computed_pixel_length *= height;
      computed_pixel_length *= 4;
      if (!computed_pixel_length.IsValid() ||
          computed_pixel_length.ValueOrDie() != pixel_length)
        return nullptr;
      return ImageBitmap::Create(pixels, width, height, is_premultiplied,
                                 origin_clean);
    }
    case kImageBitmapTransferTag: {
      uint32_t index = 0;
      const auto& transferred_image_bitmaps =
          serialized_script_value_->ReceivedImageBitmaps();
      if (!ReadUint32(&index) || index >= transferred_image_bitmaps.size())
        return nullptr;
      return transferred_image_bitmaps[index].Get();
    }
    case kImageDataTag: {
      uint32_t width = 0, height = 0, pixel_length = 0;
      const void* pixels = nullptr;
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&pixel_length) || !ReadRawBytes(pixel_length, &pixels))
        return nullptr;
      CheckedNumeric<uint32_t> computed_pixel_length = width;
      computed_pixel_length *= height;
      computed_pixel_length *= 4;
      if (!computed_pixel_length.IsValid() ||
          computed_pixel_length.ValueOrDie() != pixel_length)
        return nullptr;
      ImageData* image_data = ImageData::Create(IntSize(width, height));
      if (!image_data)
        return nullptr;
      DOMUint8ClampedArray* pixel_array = image_data->data();
      DCHECK_EQ(pixel_array->length(), pixel_length);
      memcpy(pixel_array->Data(), pixels, pixel_length);
      return image_data;
    }
    case kDOMPointTag: {
      double x = 0, y = 0, z = 0, w = 1;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
          !ReadDouble(&w))
        return nullptr;
      return DOMPoint::Create(x, y, z, w);
    }
    case kDOMPointReadOnlyTag: {
      double x = 0, y = 0, z = 0, w = 1;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
          !ReadDouble(&w))
        return nullptr;
      return DOMPointReadOnly::Create(x, y, z, w);
    }
    case kDOMRectTag: {
      double x = 0, y = 0, width = 0, height = 0;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&width) ||
          !ReadDouble(&height))
        return nullptr;
      return DOMRect::Create(x, y, width, height);
    }
    case kDOMRectReadOnlyTag: {
      double x = 0, y = 0, width = 0, height = 0;
      if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&width) ||
          !ReadDouble(&height))
        return nullptr;
      return DOMRectReadOnly::Create(x, y, width, height);
    }
    case kDOMQuadTag: {
      DOMPointInit pointInits[4];
      for (DOMPointInit& init : pointInits) {
        double x = 0, y = 0, z = 0, w = 0;
        if (!ReadDouble(&x) || !ReadDouble(&y) || !ReadDouble(&z) ||
            !ReadDouble(&w))
          return nullptr;
        init.setX(x);
        init.setY(y);
        init.setZ(z);
        init.setW(w);
      }
      return DOMQuad::Create(pointInits[0], pointInits[1], pointInits[2],
                             pointInits[3]);
    }
    case kDOMMatrix2DTag: {
      double values[6];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrix::CreateForSerialization(values,
                                               WTF_ARRAY_LENGTH(values));
    }
    case kDOMMatrix2DReadOnlyTag: {
      double values[6];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrixReadOnly::CreateForSerialization(
          values, WTF_ARRAY_LENGTH(values));
    }
    case kDOMMatrixTag: {
      double values[16];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrix::CreateForSerialization(values,
                                               WTF_ARRAY_LENGTH(values));
    }
    case kDOMMatrixReadOnlyTag: {
      double values[16];
      for (double& d : values) {
        if (!ReadDouble(&d))
          return nullptr;
      }
      return DOMMatrixReadOnly::CreateForSerialization(
          values, WTF_ARRAY_LENGTH(values));
    }
    case kMessagePortTag: {
      uint32_t index = 0;
      if (!ReadUint32(&index) || !transferred_message_ports_ ||
          index >= transferred_message_ports_->size())
        return nullptr;
      return (*transferred_message_ports_)[index].Get();
    }
    case kOffscreenCanvasTransferTag: {
      uint32_t width = 0, height = 0, canvas_id = 0, client_id = 0, sink_id = 0;
      if (!ReadUint32(&width) || !ReadUint32(&height) ||
          !ReadUint32(&canvas_id) || !ReadUint32(&client_id) ||
          !ReadUint32(&sink_id))
        return nullptr;
      OffscreenCanvas* canvas = OffscreenCanvas::Create(width, height);
      canvas->SetPlaceholderCanvasId(canvas_id);
      canvas->SetFrameSinkId(client_id, sink_id);
      return canvas;
    }
    default:
      break;
  }
  return nullptr;
}

File* V8ScriptValueDeserializer::ReadFile() {
  if (Version() < 3)
    return nullptr;
  String path, name, relative_path, uuid, type;
  uint32_t has_snapshot = 0;
  uint64_t size = 0;
  double last_modified_ms = 0;
  if (!ReadUTF8String(&path) || (Version() >= 4 && !ReadUTF8String(&name)) ||
      (Version() >= 4 && !ReadUTF8String(&relative_path)) ||
      !ReadUTF8String(&uuid) || !ReadUTF8String(&type) ||
      (Version() >= 4 && !ReadUint32(&has_snapshot)))
    return nullptr;
  if (has_snapshot) {
    if (!ReadUint64(&size) || !ReadDouble(&last_modified_ms))
      return nullptr;
    if (Version() < 8)
      last_modified_ms *= kMsPerSecond;
  }
  uint32_t is_user_visible = 1;
  if (Version() >= 7 && !ReadUint32(&is_user_visible))
    return nullptr;
  const File::UserVisibility user_visibility =
      is_user_visible ? File::kIsUserVisible : File::kIsNotUserVisible;
  const uint64_t kSizeForDataHandle = static_cast<uint64_t>(-1);
  return File::CreateFromSerialization(
      path, name, relative_path, user_visibility, has_snapshot, size,
      last_modified_ms,
      GetOrCreateBlobDataHandle(uuid, type, kSizeForDataHandle));
}

File* V8ScriptValueDeserializer::ReadFileIndex() {
  if (Version() < 6 || !blob_info_array_)
    return nullptr;
  uint32_t index;
  if (!ReadUint32(&index) || index >= blob_info_array_->size())
    return nullptr;
  const WebBlobInfo& info = (*blob_info_array_)[index];
  // FIXME: transition WebBlobInfo.lastModified to be milliseconds-based also.
  double last_modified_ms = info.LastModified() * kMsPerSecond;
  return File::CreateFromIndexedSerialization(
      info.FilePath(), info.FileName(), info.size(), last_modified_ms,
      GetOrCreateBlobDataHandle(info.Uuid(), info.GetType(), info.size()));
}

RefPtr<BlobDataHandle> V8ScriptValueDeserializer::GetOrCreateBlobDataHandle(
    const String& uuid,
    const String& type,
    uint64_t size) {
  // The containing ssv may have a BDH for this uuid if this ssv is just being
  // passed from main to worker thread (for example). We use those values when
  // creating the new blob instead of cons'ing up a new BDH.
  //
  // FIXME: Maybe we should require that it work that way where the ssv must
  // have a BDH for any blobs it comes across during deserialization. Would
  // require callers to explicitly populate the collection of BDH's for blobs to
  // work, which would encourage lifetimes to be considered when passing ssv's
  // around cross process. At present, we get 'lucky' in some cases because the
  // blob in the src process happens to still exist at the time the dest process
  // is deserializing.
  // For example in sharedWorker.postMessage(...).
  BlobDataHandleMap& handles = serialized_script_value_->BlobDataHandles();
  BlobDataHandleMap::const_iterator it = handles.find(uuid);
  if (it != handles.end())
    return it->value;
  return BlobDataHandle::Create(uuid, type, size);
}

v8::MaybeLocal<v8::Object> V8ScriptValueDeserializer::ReadHostObject(
    v8::Isolate* isolate) {
  DCHECK_EQ(isolate, script_state_->GetIsolate());
  ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                 nullptr, nullptr);
  ScriptWrappable* wrappable = nullptr;
  SerializationTag tag = kVersionTag;
  if (ReadTag(&tag))
    wrappable = ReadDOMObject(tag);
  if (!wrappable) {
    exception_state.ThrowDOMException(kDataCloneError,
                                      "Unable to deserialize cloned data.");
    return v8::MaybeLocal<v8::Object>();
  }
  v8::Local<v8::Object> creation_context =
      script_state_->GetContext()->Global();
  v8::Local<v8::Value> wrapper = ToV8(wrappable, creation_context, isolate);
  DCHECK(wrapper->IsObject());
  return wrapper.As<v8::Object>();
}

v8::MaybeLocal<v8::WasmCompiledModule>
V8ScriptValueDeserializer::GetWasmModuleFromId(v8::Isolate* isolate,
                                               uint32_t id) {
  if (id < serialized_script_value_->WasmModules().size()) {
    return v8::WasmCompiledModule::FromTransferrableModule(
        isolate, serialized_script_value_->WasmModules()[id]);
  }
  CHECK(serialized_script_value_->WasmModules().IsEmpty());
  return v8::MaybeLocal<v8::WasmCompiledModule>();
}

}  // namespace blink
