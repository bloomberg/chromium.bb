/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "bindings/core/v8/SerializedScriptValue.h"

#include <memory>
#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/Transferables.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8OffscreenCanvas.h"
#include "bindings/core/v8/V8SharedArrayBuffer.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/frame/ImageBitmap.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/ByteOrder.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuffer.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Serialize(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializeOptions& options,
    ExceptionState& exception) {
  return SerializedScriptValueFactory::Instance().Create(isolate, value,
                                                         options, exception);
}

PassRefPtr<SerializedScriptValue>
SerializedScriptValue::SerializeAndSwallowExceptions(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  DummyExceptionStateForTesting exception_state;
  RefPtr<SerializedScriptValue> serialized =
      Serialize(isolate, value, SerializeOptions(), exception_state);
  if (exception_state.HadException())
    return NullValue();
  return serialized.Release();
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Create() {
  return AdoptRef(new SerializedScriptValue);
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Create(
    const String& data) {
  return AdoptRef(new SerializedScriptValue(data));
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Create(
    const char* data,
    size_t length) {
  if (!data)
    return Create();

  // Decode wire data from big endian to host byte order.
  DCHECK(!(length % sizeof(UChar)));
  size_t string_length = length / sizeof(UChar);
  StringBuffer<UChar> buffer(string_length);
  const UChar* src = reinterpret_cast<const UChar*>(data);
  UChar* dst = buffer.Characters();
  for (size_t i = 0; i < string_length; i++)
    dst[i] = ntohs(src[i]);

  return AdoptRef(new SerializedScriptValue(String::Adopt(buffer)));
}

SerializedScriptValue::SerializedScriptValue()
    : has_registered_external_allocation_(false),
      transferables_need_external_allocation_registration_(false) {}

SerializedScriptValue::SerializedScriptValue(const String& wire_data)
    : has_registered_external_allocation_(false),
      transferables_need_external_allocation_registration_(false) {
  size_t byte_length = wire_data.length() * 2;
  data_buffer_.reset(static_cast<uint8_t*>(WTF::Partitions::BufferMalloc(
      byte_length, "SerializedScriptValue buffer")));
  data_buffer_size_ = byte_length;
  wire_data.CopyTo(reinterpret_cast<UChar*>(data_buffer_.get()), 0,
                   wire_data.length());
}

SerializedScriptValue::~SerializedScriptValue() {
  // If the allocated memory was not registered before, then this class is
  // likely used in a context other than Worker's onmessage environment and the
  // presence of current v8 context is not guaranteed. Avoid calling v8 then.
  if (has_registered_external_allocation_) {
    ASSERT(v8::Isolate::GetCurrent());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int64_t>(DataLengthInBytes()));
  }
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::NullValue() {
  // UChar rather than uint8_t here to get host endian behavior.
  static const UChar kNullData[] = {0xff09, 0x3000};
  return Create(reinterpret_cast<const char*>(kNullData), sizeof(kNullData));
}

String SerializedScriptValue::ToWireString() const {
  // Add the padding '\0', but don't put it in |m_dataBuffer|.
  // This requires direct use of uninitialized strings, though.
  UChar* destination;
  size_t string_size_bytes = (data_buffer_size_ + 1) & ~1;
  String wire_string =
      String::CreateUninitialized(string_size_bytes / 2, destination);
  memcpy(destination, data_buffer_.get(), data_buffer_size_);
  if (string_size_bytes > data_buffer_size_)
    reinterpret_cast<char*>(destination)[string_size_bytes - 1] = '\0';
  return wire_string;
}

// Convert serialized string to big endian wire data.
void SerializedScriptValue::ToWireBytes(Vector<char>& result) const {
  DCHECK(result.IsEmpty());

  size_t wire_size_bytes = (data_buffer_size_ + 1) & ~1;
  result.Resize(wire_size_bytes);

  const UChar* src = reinterpret_cast<UChar*>(data_buffer_.get());
  UChar* dst = reinterpret_cast<UChar*>(result.Data());
  for (size_t i = 0; i < data_buffer_size_ / 2; i++)
    dst[i] = htons(src[i]);

  // This is equivalent to swapping the byte order of the two bytes (x, 0),
  // depending on endianness.
  if (data_buffer_size_ % 2)
    dst[wire_size_bytes / 2 - 1] = data_buffer_[data_buffer_size_ - 1] << 8;
}

static void AccumulateArrayBuffersForAllWorlds(
    v8::Isolate* isolate,
    DOMArrayBuffer* object,
    Vector<v8::Local<v8::ArrayBuffer>, 4>& buffers) {
  Vector<RefPtr<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  for (const auto& world : worlds) {
    v8::Local<v8::Object> wrapper = world->DomDataStore().Get(object, isolate);
    if (!wrapper.IsEmpty())
      buffers.push_back(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
  }
}

std::unique_ptr<SerializedScriptValue::ImageBitmapContentsArray>
SerializedScriptValue::TransferImageBitmapContents(
    v8::Isolate* isolate,
    const ImageBitmapArray& image_bitmaps,
    ExceptionState& exception_state) {
  if (!image_bitmaps.size())
    return nullptr;

  for (size_t i = 0; i < image_bitmaps.size(); ++i) {
    if (image_bitmaps[i]->IsNeutered()) {
      exception_state.ThrowDOMException(
          kDataCloneError, "ImageBitmap at index " + String::Number(i) +
                               " is already detached.");
      return nullptr;
    }
  }

  std::unique_ptr<ImageBitmapContentsArray> contents =
      WTF::WrapUnique(new ImageBitmapContentsArray);
  HeapHashSet<Member<ImageBitmap>> visited;
  for (size_t i = 0; i < image_bitmaps.size(); ++i) {
    if (visited.Contains(image_bitmaps[i]))
      continue;
    visited.insert(image_bitmaps[i]);
    contents->push_back(image_bitmaps[i]->Transfer());
  }
  return contents;
}

void SerializedScriptValue::TransferImageBitmaps(
    v8::Isolate* isolate,
    const ImageBitmapArray& image_bitmaps,
    ExceptionState& exception_state) {
  std::unique_ptr<ImageBitmapContentsArray> contents =
      TransferImageBitmapContents(isolate, image_bitmaps, exception_state);
  image_bitmap_contents_array_ = std::move(contents);
}

void SerializedScriptValue::TransferOffscreenCanvas(
    v8::Isolate* isolate,
    const OffscreenCanvasArray& offscreen_canvases,
    ExceptionState& exception_state) {
  if (!offscreen_canvases.size())
    return;

  HeapHashSet<Member<OffscreenCanvas>> visited;
  for (size_t i = 0; i < offscreen_canvases.size(); i++) {
    if (visited.Contains(offscreen_canvases[i].Get()))
      continue;
    if (offscreen_canvases[i]->IsNeutered()) {
      exception_state.ThrowDOMException(
          kDataCloneError, "OffscreenCanvas at index " + String::Number(i) +
                               " is already detached.");
      return;
    }
    if (offscreen_canvases[i]->RenderingContext()) {
      exception_state.ThrowDOMException(
          kDataCloneError, "OffscreenCanvas at index " + String::Number(i) +
                               " has an associated context.");
      return;
    }
    visited.insert(offscreen_canvases[i].Get());
    offscreen_canvases[i].Get()->SetNeutered();
  }
}

void SerializedScriptValue::TransferArrayBuffers(
    v8::Isolate* isolate,
    const ArrayBufferArray& array_buffers,
    ExceptionState& exception_state) {
  array_buffer_contents_array_ =
      TransferArrayBufferContents(isolate, array_buffers, exception_state);
}

v8::Local<v8::Value> SerializedScriptValue::Deserialize(
    v8::Isolate* isolate,
    const DeserializeOptions& options) {
  return SerializedScriptValueFactory::Instance().Deserialize(this, isolate,
                                                              options);
}

bool SerializedScriptValue::ExtractTransferables(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    int argument_index,
    Transferables& transferables,
    ExceptionState& exception_state) {
  if (value.IsEmpty() || value->IsUndefined())
    return true;

  uint32_t length = 0;
  if (value->IsArray()) {
    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);
    length = array->Length();
  } else if (!ToV8Sequence(value, length, isolate, exception_state)) {
    if (!exception_state.HadException())
      exception_state.ThrowTypeError(
          ExceptionMessages::NotAnArrayTypeArgumentOrValue(argument_index + 1));
    return false;
  }

  v8::Local<v8::Object> transferable_array = v8::Local<v8::Object>::Cast(value);

  // Validate the passed array of transferables.
  for (unsigned i = 0; i < length; ++i) {
    v8::Local<v8::Value> transferable_object;
    if (!transferable_array->Get(isolate->GetCurrentContext(), i)
             .ToLocal(&transferable_object))
      return false;
    // Validation of non-null objects, per HTML5 spec 10.3.3.
    if (IsUndefinedOrNull(transferable_object)) {
      exception_state.ThrowTypeError(
          "Value at index " + String::Number(i) + " is an untransferable " +
          (transferable_object->IsUndefined() ? "'undefined'" : "'null'") +
          " value.");
      return false;
    }
    // Validation of Objects implementing an interface, per WebIDL spec 4.1.15.
    if (V8MessagePort::hasInstance(transferable_object, isolate)) {
      MessagePort* port = V8MessagePort::toImpl(
          v8::Local<v8::Object>::Cast(transferable_object));
      // Check for duplicate MessagePorts.
      if (transferables.message_ports.Contains(port)) {
        exception_state.ThrowDOMException(
            kDataCloneError, "Message port at index " + String::Number(i) +
                                 " is a duplicate of an earlier port.");
        return false;
      }
      transferables.message_ports.push_back(port);
    } else if (transferable_object->IsArrayBuffer()) {
      DOMArrayBuffer* array_buffer = V8ArrayBuffer::toImpl(
          v8::Local<v8::Object>::Cast(transferable_object));
      if (transferables.array_buffers.Contains(array_buffer)) {
        exception_state.ThrowDOMException(
            kDataCloneError, "ArrayBuffer at index " + String::Number(i) +
                                 " is a duplicate of an earlier ArrayBuffer.");
        return false;
      }
      transferables.array_buffers.push_back(array_buffer);
    } else if (transferable_object->IsSharedArrayBuffer()) {
      DOMSharedArrayBuffer* shared_array_buffer = V8SharedArrayBuffer::toImpl(
          v8::Local<v8::Object>::Cast(transferable_object));
      if (transferables.array_buffers.Contains(shared_array_buffer)) {
        exception_state.ThrowDOMException(
            kDataCloneError,
            "SharedArrayBuffer at index " + String::Number(i) +
                " is a duplicate of an earlier SharedArrayBuffer.");
        return false;
      }
      transferables.array_buffers.push_back(shared_array_buffer);
    } else if (V8ImageBitmap::hasInstance(transferable_object, isolate)) {
      ImageBitmap* image_bitmap = V8ImageBitmap::toImpl(
          v8::Local<v8::Object>::Cast(transferable_object));
      if (transferables.image_bitmaps.Contains(image_bitmap)) {
        exception_state.ThrowDOMException(
            kDataCloneError, "ImageBitmap at index " + String::Number(i) +
                                 " is a duplicate of an earlier ImageBitmap.");
        return false;
      }
      transferables.image_bitmaps.push_back(image_bitmap);
    } else if (V8OffscreenCanvas::hasInstance(transferable_object, isolate)) {
      OffscreenCanvas* offscreen_canvas = V8OffscreenCanvas::toImpl(
          v8::Local<v8::Object>::Cast(transferable_object));
      if (transferables.offscreen_canvases.Contains(offscreen_canvas)) {
        exception_state.ThrowDOMException(
            kDataCloneError,
            "OffscreenCanvas at index " + String::Number(i) +
                " is a duplicate of an earlier OffscreenCanvas.");
        return false;
      }
      transferables.offscreen_canvases.push_back(offscreen_canvas);
    } else {
      exception_state.ThrowTypeError("Value at index " + String::Number(i) +
                                     " does not have a transferable type.");
      return false;
    }
  }
  return true;
}

std::unique_ptr<SerializedScriptValue::ArrayBufferContentsArray>
SerializedScriptValue::TransferArrayBufferContents(
    v8::Isolate* isolate,
    const ArrayBufferArray& array_buffers,
    ExceptionState& exception_state) {
  if (!array_buffers.size())
    return nullptr;

  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMArrayBufferBase* array_buffer = *it;
    if (array_buffer->IsNeutered()) {
      size_t index = std::distance(array_buffers.begin(), it);
      exception_state.ThrowDOMException(
          kDataCloneError, "ArrayBuffer at index " + String::Number(index) +
                               " is already neutered.");
      return nullptr;
    }
  }

  std::unique_ptr<ArrayBufferContentsArray> contents =
      WTF::WrapUnique(new ArrayBufferContentsArray(array_buffers.size()));

  HeapHashSet<Member<DOMArrayBufferBase>> visited;
  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMArrayBufferBase* array_buffer = *it;
    if (visited.Contains(array_buffer))
      continue;
    visited.insert(array_buffer);

    size_t index = std::distance(array_buffers.begin(), it);
    if (array_buffer->IsShared()) {
      if (!array_buffer->ShareContentsWith(contents->at(index))) {
        exception_state.ThrowDOMException(kDataCloneError,
                                          "SharedArrayBuffer at index " +
                                              String::Number(index) +
                                              " could not be transferred.");
        return nullptr;
      }
    } else {
      Vector<v8::Local<v8::ArrayBuffer>, 4> buffer_handles;
      v8::HandleScope handle_scope(isolate);
      AccumulateArrayBuffersForAllWorlds(
          isolate, static_cast<DOMArrayBuffer*>(it->Get()), buffer_handles);
      bool is_neuterable = true;
      for (const auto& buffer_handle : buffer_handles)
        is_neuterable &= buffer_handle->IsNeuterable();

      DOMArrayBufferBase* to_transfer = array_buffer;
      if (!is_neuterable) {
        to_transfer =
            DOMArrayBuffer::Create(array_buffer->Buffer()->Data(),
                                   array_buffer->Buffer()->ByteLength());
      }
      if (!to_transfer->Transfer(contents->at(index))) {
        exception_state.ThrowDOMException(
            kDataCloneError, "ArrayBuffer at index " + String::Number(index) +
                                 " could not be transferred.");
        return nullptr;
      }

      if (is_neuterable) {
        for (const auto& buffer_handle : buffer_handles)
          buffer_handle->Neuter();
      }
    }
  }
  return contents;
}

void SerializedScriptValue::
    UnregisterMemoryAllocatedWithCurrentScriptContext() {
  if (has_registered_external_allocation_) {
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int64_t>(DataLengthInBytes()));
    has_registered_external_allocation_ = false;
  }

  // TODO: if other transferables start accounting for their external
  // allocations with V8, extend this with corresponding cases.
  if (array_buffer_contents_array_ &&
      !transferables_need_external_allocation_registration_) {
    for (auto& buffer : *array_buffer_contents_array_)
      buffer.UnregisterExternalAllocationWithCurrentContext();
    transferables_need_external_allocation_registration_ = true;
  }
}

void SerializedScriptValue::RegisterMemoryAllocatedWithCurrentScriptContext() {
  if (has_registered_external_allocation_)
    return;

  has_registered_external_allocation_ = true;
  int64_t diff = static_cast<int64_t>(DataLengthInBytes());
  DCHECK_GE(diff, 0);
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(diff);

  // Only (re)register allocation cost for transferables if this
  // SerializedScriptValue has explicitly unregistered them before.
  if (array_buffer_contents_array_ &&
      transferables_need_external_allocation_registration_) {
    for (auto& buffer : *array_buffer_contents_array_)
      buffer.RegisterExternalAllocationWithCurrentContext();
  }
}

}  // namespace blink
