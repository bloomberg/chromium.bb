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

#include "bindings/core/v8/serialization/SerializedScriptValue.h"

#include <algorithm>
#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/IDLTypes.h"
#include "bindings/core/v8/NativeValueTraitsImpl.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8OffscreenCanvas.h"
#include "bindings/core/v8/V8SharedArrayBuffer.h"
#include "bindings/core/v8/serialization/SerializationTag.h"
#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"
#include "bindings/core/v8/serialization/Transferables.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMSharedArrayBuffer.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "platform/SharedBuffer.h"
#include "platform/bindings/DOMDataStore.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptState.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/ByteOrder.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/dtoa/utils.h"
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
  return serialized;
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Create() {
  return AdoptRef(new SerializedScriptValue);
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Create(
    const String& data) {
  return AdoptRef(new SerializedScriptValue(data));
}

// Versions 16 and below (prior to April 2017) used ntohs() to byte-swap SSV
// data when converting it to the wire format. This was a historical accient.
//
// As IndexedDB stores SSVs to disk indefinitely, we still need to keep around
// the code needed to deserialize the old format.
inline static bool IsByteSwappedWiredData(const char* data, size_t length) {
  // TODO(pwnall): Return false early if we're on big-endian hardware. Chromium
  // doesn't currently support big-endian hardware, and there's no header
  // exposing endianness to Blink yet. ARCH_CPU_LITTLE_ENDIAN seems promising,
  // but Blink is not currently allowed to include files from build/.

  // The first SSV version without byte-swapping has two envelopes (Blink, V8),
  // each of which is at least 2 bytes long.
  if (length < 4)
    return true;

  // This code handles the following cases:
  //
  // v0 (byte-swapped)    - [d,    t,    ...], t = tag byte, d = first data byte
  // v1-16 (byte-swapped) - [v,    0xFF, ...], v = version (1 <= v <= 16)
  // v17+                 - [0xFF, v,    ...], v = first byte of version varint

  if (static_cast<uint8_t>(data[0]) == kVersionTag) {
    // The only case where byte-swapped data can have 0xFF in byte zero is
    // version 0. This can only happen if byte one is a tag (supported in
    // version 0) that takes in extra data, and the first byte of extra data is
    // 0xFF. There are 13 such tags, listed below. These tags cannot be used as
    // version numbers in the Blink-side SSV envelope.
    //
    //  35 - 0x23 - # - ImageDataTag
    //  64 - 0x40 - @ - SparseArrayTag
    //  68 - 0x44 - D - DateTag
    //  73 - 0x49 - I - Int32Tag
    //  78 - 0x4E - N - NumberTag
    //  82 - 0x52 - R - RegExpTag
    //  83 - 0x53 - S - StringTag
    //  85 - 0x55 - U - Uint32Tag
    //  91 - 0x5B - [ - ArrayTag
    //  98 - 0x62 - b - BlobTag
    // 102 - 0x66 - f - FileTag
    // 108 - 0x6C - l - FileListTag
    // 123 - 0x7B - { - ObjectTag
    //
    // Why we care about version 0:
    //
    // IndexedDB stores values using the SSV format. Currently, IndexedDB does
    // not do any sort of migration, so a value written with a SSV version will
    // be stored with that version until it is removed via an update or delete.
    //
    // IndexedDB was shipped in Chrome 11, which was released on April 27, 2011.
    // SSV version 1 was added in WebKit r91698, which was shipped in Chrome 14,
    // which was released on September 16, 2011.
    static_assert(
        SerializedScriptValue::kWireFormatVersion != 35 &&
            SerializedScriptValue::kWireFormatVersion != 64 &&
            SerializedScriptValue::kWireFormatVersion != 68 &&
            SerializedScriptValue::kWireFormatVersion != 73 &&
            SerializedScriptValue::kWireFormatVersion != 78 &&
            SerializedScriptValue::kWireFormatVersion != 82 &&
            SerializedScriptValue::kWireFormatVersion != 83 &&
            SerializedScriptValue::kWireFormatVersion != 85 &&
            SerializedScriptValue::kWireFormatVersion != 91 &&
            SerializedScriptValue::kWireFormatVersion != 98 &&
            SerializedScriptValue::kWireFormatVersion != 102 &&
            SerializedScriptValue::kWireFormatVersion != 108 &&
            SerializedScriptValue::kWireFormatVersion != 123,
        "Using a burned version will prevent us from reading SSV version 0");

    // Fast path until the Blink-side SSV envelope reaches version 35.
    if (SerializedScriptValue::kWireFormatVersion < 35) {
      if (static_cast<uint8_t>(data[1]) < 35)
        return false;

      // TODO(pwnall): Add UMA metric here.
      return true;
    }

    // Slower path that would kick in after version 35, assuming we don't remove
    // support for SSV version 0 by then.
    static constexpr uint8_t version0Tags[] = {35, 64, 68, 73,  78,  82, 83,
                                               85, 91, 98, 102, 108, 123};
    return std::find(std::begin(version0Tags), std::end(version0Tags),
                     data[1]) != std::end(version0Tags);
  }

  if (static_cast<uint8_t>(data[1]) == kVersionTag) {
    // The last SSV format that used byte-swapping was version 16. The version
    // number is stored (before byte-swapping) after a serialization tag, which
    // is 0xFF.
    return static_cast<uint8_t>(data[0]) != kVersionTag;
  }

  // If kVersionTag isn't in any of the first two bytes, this is SSV version 0,
  // which was byte-swapped.
  return true;
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::Create(
    const char* data,
    size_t length) {
  if (!data)
    return Create();

  DCHECK(!(length % sizeof(UChar)));
  const UChar* src = reinterpret_cast<const UChar*>(data);
  size_t string_length = length / sizeof(UChar);

  if (IsByteSwappedWiredData(data, length)) {
    // Decode wire data from big endian to host byte order.
    StringBuffer<UChar> buffer(string_length);
    UChar* dst = buffer.Characters();
    for (size_t i = 0; i < string_length; ++i)
      dst[i] = ntohs(src[i]);

    return AdoptRef(new SerializedScriptValue(String::Adopt(buffer)));
  }

  return AdoptRef(new SerializedScriptValue(String(src, string_length)));
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
    DCHECK(v8::Isolate::GetCurrent());
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int64_t>(DataLengthInBytes()));
  }
}

PassRefPtr<SerializedScriptValue> SerializedScriptValue::NullValue() {
  // The format here may fall a bit out of date, because we support
  // deserializing SSVs written by old browser versions.
  static const uint8_t kNullData[] = {0xFF, 17, 0xFF, 13, '0', 0x00};
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

void SerializedScriptValue::ToWireBytes(Vector<char>& result) const {
  DCHECK(result.IsEmpty());

  size_t result_size = (data_buffer_size_ + 1) & ~1;
  result.resize(result_size);
  memcpy(result.data(), data_buffer_.get(), data_buffer_size_);

  if (result_size > data_buffer_size_) {
    DCHECK_EQ(result_size, data_buffer_size_ + 1);
    result[data_buffer_size_] = 0;
  }
}

SerializedScriptValue::ImageBitmapContentsArray
SerializedScriptValue::TransferImageBitmapContents(
    v8::Isolate* isolate,
    const ImageBitmapArray& image_bitmaps,
    ExceptionState& exception_state) {
  ImageBitmapContentsArray contents;

  if (!image_bitmaps.size())
    return contents;

  for (size_t i = 0; i < image_bitmaps.size(); ++i) {
    if (image_bitmaps[i]->IsNeutered()) {
      exception_state.ThrowDOMException(
          kDataCloneError, "ImageBitmap at index " + String::Number(i) +
                               " is already detached.");
      return contents;
    }
  }

  HeapHashSet<Member<ImageBitmap>> visited;
  for (size_t i = 0; i < image_bitmaps.size(); ++i) {
    if (visited.Contains(image_bitmaps[i]))
      continue;
    visited.insert(image_bitmaps[i]);
    contents.push_back(image_bitmaps[i]->Transfer());
  }
  return contents;
}

void SerializedScriptValue::TransferImageBitmaps(
    v8::Isolate* isolate,
    const ImageBitmapArray& image_bitmaps,
    ExceptionState& exception_state) {
  image_bitmap_contents_array_ =
      TransferImageBitmapContents(isolate, image_bitmaps, exception_state);
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

  Vector<v8::Local<v8::Value>> transferable_array =
      NativeValueTraits<IDLSequence<v8::Local<v8::Value>>>::NativeValue(
          isolate, value, exception_state);
  if (exception_state.HadException())
    return false;

  // Validate the passed array of transferables.
  uint32_t i = 0;
  for (const auto& transferable_object : transferable_array) {
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
    i++;
  }
  return true;
}

ArrayBufferArray SerializedScriptValue::ExtractNonSharedArrayBuffers(
    Transferables& transferables) {
  ArrayBufferArray& array_buffers = transferables.array_buffers;
  ArrayBufferArray result;
  // Partition array_buffers into [shared..., non_shared...], maintaining
  // relative ordering of elements with the same predicate value.
  auto non_shared_begin =
      std::stable_partition(array_buffers.begin(), array_buffers.end(),
                            [](Member<DOMArrayBufferBase>& array_buffer) {
                              return array_buffer->IsShared();
                            });
  // Copy the non-shared array buffers into result, and remove them from
  // array_buffers.
  result.AppendRange(non_shared_begin, array_buffers.end());
  array_buffers.erase(non_shared_begin - array_buffers.begin(),
                      array_buffers.end() - non_shared_begin);
  return result;
}

SerializedScriptValue::ArrayBufferContentsArray
SerializedScriptValue::TransferArrayBufferContents(
    v8::Isolate* isolate,
    const ArrayBufferArray& array_buffers,
    ExceptionState& exception_state) {
  ArrayBufferContentsArray contents;

  if (!array_buffers.size())
    return ArrayBufferContentsArray();

  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMArrayBufferBase* array_buffer = *it;
    if (array_buffer->IsNeutered()) {
      size_t index = std::distance(array_buffers.begin(), it);
      exception_state.ThrowDOMException(
          kDataCloneError, "ArrayBuffer at index " + String::Number(index) +
                               " is already neutered.");
      return ArrayBufferContentsArray();
    }
  }

  contents.Grow(array_buffers.size());
  HeapHashSet<Member<DOMArrayBufferBase>> visited;
  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMArrayBufferBase* array_buffer_base = *it;
    if (visited.Contains(array_buffer_base))
      continue;
    visited.insert(array_buffer_base);

    size_t index = std::distance(array_buffers.begin(), it);
    if (array_buffer_base->IsShared()) {
      DOMSharedArrayBuffer* shared_array_buffer =
          static_cast<DOMSharedArrayBuffer*>(array_buffer_base);
      if (!shared_array_buffer->ShareContentsWith(contents.at(index))) {
        exception_state.ThrowDOMException(kDataCloneError,
                                          "SharedArrayBuffer at index " +
                                              String::Number(index) +
                                              " could not be transferred.");
        return ArrayBufferContentsArray();
      }
    } else {
      DOMArrayBuffer* array_buffer =
          static_cast<DOMArrayBuffer*>(array_buffer_base);

      if (!array_buffer->Transfer(isolate, contents.at(index))) {
        exception_state.ThrowDOMException(
            kDataCloneError, "ArrayBuffer at index " + String::Number(index) +
                                 " could not be transferred.");
        return ArrayBufferContentsArray();
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
  if (!transferables_need_external_allocation_registration_) {
    for (auto& buffer : array_buffer_contents_array_)
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
  if (transferables_need_external_allocation_registration_) {
    for (auto& buffer : array_buffer_contents_array_)
      buffer.RegisterExternalAllocationWithCurrentContext();
  }
}

void SerializedScriptValue::ReceiveTransfer() {
  if (received_)
    return;
  received_.emplace();

  received_->array_buffers.Grow(array_buffer_contents_array_.size());
  std::transform(array_buffer_contents_array_.begin(),
                 array_buffer_contents_array_.end(),
                 received_->array_buffers.begin(),
                 [](WTF::ArrayBufferContents& contents) -> DOMArrayBufferBase* {
                   if (contents.IsShared())
                     return DOMSharedArrayBuffer::Create(contents);
                   return DOMArrayBuffer::Create(contents);
                 });
  array_buffer_contents_array_.clear();

  received_->image_bitmaps.Grow(image_bitmap_contents_array_.size());
  std::transform(image_bitmap_contents_array_.begin(),
                 image_bitmap_contents_array_.end(),
                 received_->image_bitmaps.begin(),
                 [](RefPtr<StaticBitmapImage>& contents) {
                   return ImageBitmap::Create(std::move(contents));
                 });
  image_bitmap_contents_array_.clear();
}

}  // namespace blink
