// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBValueWrapping.h"

#include <memory>
#include <utility>

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/serialization/SerializationTag.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/blob/BlobData.h"
#include "platform/wtf/text/StringView.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

// V8 values are stored on disk by IndexedDB using the format implemented in
// SerializedScriptValue (SSV). The wrapping detection logic in
// IDBValueUnwrapper::IsWrapped() must be able to distinguish between SSV byte
// sequences produced and byte sequences expressing the fact that an IDBValue
// has been wrapped and requires post-processing.
//
// The detection logic takes advantage of the highly regular structure around
// SerializedScriptValue. A version 17 byte sequence always starts with the
// following four bytes:
//
// 1) 0xFF - kVersionTag
// 2) 0x11 - Blink wrapper version, 17
// 3) 0xFF - kVersionTag
// 4) 0x0D - V8 serialization version, currently 13, doesn't matter
//
// It follows that SSV will never produce byte sequences starting with 0xFF,
// 0x11, and any value except for 0xFF. If the SSV format changes, the version
// will have to be bumped.

// The SSV format version whose encoding hole is (ab)used for wrapping.
const static uint8_t kRequiresProcessingSSVPseudoVersion = 17;

// Identifies IndexedDB values that were wrapped in Blobs. The wrapper has the
// following format:
//
// 1) 0xFF - kVersionTag
// 2) 0x11 - kRequiresProcessingSSVPseudoVersion
// 3) 0x01 - kBlobWrappedValue
// 4) varint - Blob size
// 5) varint - the offset of the SSV-wrapping Blob in the IDBValue list of Blobs
//             (should always be the last Blob)
const static uint8_t kBlobWrappedValue = 1;

}  // namespace

IDBValueWrapper::IDBValueWrapper(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    SerializedScriptValue::SerializeOptions::WasmSerializationPolicy
        wasm_policy,
    ExceptionState& exception_state) {
  SerializedScriptValue::SerializeOptions options;
  options.blob_info = &blob_info_;
  options.for_storage = SerializedScriptValue::kForStorage;
  options.wasm_policy = wasm_policy;

  serialized_value_ = SerializedScriptValue::Serialize(isolate, value, options,
                                                       exception_state);
  if (serialized_value_)
    original_data_length_ = serialized_value_->DataLengthInBytes();
#if DCHECK_IS_ON()
  if (exception_state.HadException())
    had_exception_ = true;
#endif  // DCHECK_IS_ON()
}

// Explicit destructor in the .cpp file, to move the dependency on the
// BlobDataHandle definition away from the header file.
IDBValueWrapper::~IDBValueWrapper() {}

void IDBValueWrapper::Clone(ScriptState* script_state, ScriptValue* clone) {
#if DCHECK_IS_ON()
  DCHECK(!had_exception_) << __FUNCTION__
                          << " called on wrapper with serialization exception";
  DCHECK(!wrap_called_) << "Clone() called after WrapIfBiggerThan()";
#endif  // DCHECK_IS_ON()

  bool read_wasm_from_stream = true;
  // It is safe to unconditionally enable WASM module decoding because the
  // relevant checks were already performed in SerializedScriptValue::Serialize,
  // called by the IDBValueWrapper constructor.
  *clone = DeserializeScriptValue(script_state, serialized_value_.get(),
                                  &blob_info_, read_wasm_from_stream);
}

void IDBValueWrapper::WriteVarint(unsigned value, Vector<char>& output) {
  // Writes an unsigned integer as a base-128 varint.
  // The number is written, 7 bits at a time, from the least significant to
  // the most significant 7 bits. Each byte, except the last, has the MSB set.
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  do {
    output.push_back((value & 0x7F) | 0x80);
    value >>= 7;
  } while (value);
  output.back() &= 0x7F;
}

bool IDBValueWrapper::WrapIfBiggerThan(unsigned max_bytes) {
#if DCHECK_IS_ON()
  DCHECK(!had_exception_) << __FUNCTION__
                          << " called on wrapper with serialization exception";
  DCHECK(!wrap_called_) << __FUNCTION__ << " called twice on the same wrapper";
  wrap_called_ = true;
#endif  // DCHECK_IS_ON()

  StringView wire_data = serialized_value_->GetWireData();
  DCHECK(wire_data.Is8Bit());
  unsigned wire_data_size = wire_data.length();
  if (wire_data_size <= max_bytes) {
    wire_bytes_.ReserveInitialCapacity(wire_data_size);
    wire_bytes_.Append(wire_data.Characters8(), wire_data_size);
    return false;
  }

  // TODO(pwnall): The MIME type should probably be an atomic string.
  String mime_type(kWrapMimeType);
  // TODO(crbug.com/721516): Use WebBlobRegistry::CreateBuilder instead of
  //                         Blob::Create to avoid a buffer copy.
  std::unique_ptr<BlobData> wrapper_blob_data = BlobData::Create();
  wrapper_blob_data->SetContentType(String(kWrapMimeType));
  wrapper_blob_data->AppendBytes(wire_data.Characters8(), wire_data_size);
  wrapper_handle_ =
      BlobDataHandle::Create(std::move(wrapper_blob_data), wire_data_size);
  blob_info_.emplace_back(wrapper_handle_->Uuid(), wrapper_handle_->GetType(),
                          wire_data_size);

  wire_bytes_.clear();
  wire_bytes_.push_back(kVersionTag);
  wire_bytes_.push_back(kRequiresProcessingSSVPseudoVersion);
  wire_bytes_.push_back(kBlobWrappedValue);
  IDBValueWrapper::WriteVarint(wrapper_handle_->size(), wire_bytes_);
  IDBValueWrapper::WriteVarint(serialized_value_->BlobDataHandles().size(),
                               wire_bytes_);
  return true;
}

void IDBValueWrapper::ExtractBlobDataHandles(
    Vector<scoped_refptr<BlobDataHandle>>* blob_data_handles) {
  for (const auto& kvp : serialized_value_->BlobDataHandles())
    blob_data_handles->push_back(kvp.value);
  if (wrapper_handle_)
    blob_data_handles->push_back(std::move(wrapper_handle_));
}

scoped_refptr<SharedBuffer> IDBValueWrapper::ExtractWireBytes() {
#if DCHECK_IS_ON()
  DCHECK(!had_exception_) << __FUNCTION__
                          << " called on wrapper with serialization exception";
#endif  // DCHECK_IS_ON()

  return SharedBuffer::AdoptVector(wire_bytes_);
}

IDBValueUnwrapper::IDBValueUnwrapper() {
  Reset();
}

bool IDBValueUnwrapper::IsWrapped(IDBValue* value) {
  DCHECK(value);

  uint8_t header[3];
  if (!value->data_ || !value->data_->GetBytes(header, sizeof(header)))
    return false;

  return header[0] == kVersionTag &&
         header[1] == kRequiresProcessingSSVPseudoVersion &&
         header[2] == kBlobWrappedValue;
}

bool IDBValueUnwrapper::IsWrapped(
    const Vector<scoped_refptr<IDBValue>>& values) {
  for (const auto& value : values) {
    if (IsWrapped(value.get()))
      return true;
  }
  return false;
}

scoped_refptr<IDBValue> IDBValueUnwrapper::Unwrap(
    IDBValue* wrapped_value,
    scoped_refptr<SharedBuffer>&& wrapper_blob_content) {
  DCHECK(wrapped_value);
  DCHECK(wrapped_value->data_);
  DCHECK_GT(wrapped_value->blob_info_->size(), 0U);
  DCHECK_EQ(wrapped_value->blob_info_->size(),
            wrapped_value->blob_data_->size());

  // Create an IDBValue with the same blob information, minus the last blob.
  unsigned blob_count = wrapped_value->BlobInfo()->size() - 1;
  std::unique_ptr<Vector<scoped_refptr<BlobDataHandle>>> blob_data =
      std::make_unique<Vector<scoped_refptr<BlobDataHandle>>>();
  blob_data->ReserveCapacity(blob_count);
  std::unique_ptr<Vector<WebBlobInfo>> blob_info =
      std::make_unique<Vector<WebBlobInfo>>();
  blob_info->ReserveCapacity(blob_count);

  for (unsigned i = 0; i < blob_count; ++i) {
    blob_data->push_back((*wrapped_value->blob_data_)[i]);
    blob_info->push_back((*wrapped_value->blob_info_)[i]);
  }

  return IDBValue::Create(std::move(wrapper_blob_content), std::move(blob_data),
                          std::move(blob_info), wrapped_value->PrimaryKey(),
                          wrapped_value->KeyPath());
}

bool IDBValueUnwrapper::Parse(IDBValue* value) {
  // Fast path that avoids unnecessary dynamic allocations.
  if (!IDBValueUnwrapper::IsWrapped(value))
    return false;

  const uint8_t* data = reinterpret_cast<const uint8_t*>(value->data_->Data());
  end_ = data + value->data_->size();
  current_ = data + 3;

  if (!ReadVarint(blob_size_))
    return Reset();

  unsigned blob_offset;
  if (!ReadVarint(blob_offset))
    return Reset();

  size_t value_blob_count = value->blob_data_->size();
  if (!value_blob_count || blob_offset != value_blob_count - 1)
    return Reset();

  blob_handle_ = value->blob_data_->back();
  if (blob_handle_->size() != blob_size_)
    return Reset();

  return true;
}

scoped_refptr<BlobDataHandle> IDBValueUnwrapper::WrapperBlobHandle() {
  DCHECK(blob_handle_);

  return std::move(blob_handle_);
}

bool IDBValueUnwrapper::ReadVarint(unsigned& value) {
  value = 0;
  unsigned shift = 0;
  bool has_another_byte;
  do {
    if (current_ >= end_)
      return false;

    if (shift >= sizeof(unsigned) * 8)
      return false;
    uint8_t byte = *current_;
    ++current_;
    value |= static_cast<unsigned>(byte & 0x7F) << shift;
    shift += 7;

    has_another_byte = byte & 0x80;
  } while (has_another_byte);
  return true;
}

bool IDBValueUnwrapper::Reset() {
#if DCHECK_IS_ON()
  blob_handle_ = nullptr;
  current_ = nullptr;
  end_ = nullptr;
#endif  // DCHECK_IS_ON()
  return false;
}

}  // namespace blink
