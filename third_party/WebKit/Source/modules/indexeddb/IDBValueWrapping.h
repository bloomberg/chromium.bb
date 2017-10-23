// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBValueWrapping_h
#define IDBValueWrapping_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "modules/ModulesExport.h"
#include "platform/SharedBuffer.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebBlobInfo.h"
#include "v8/include/v8.h"

namespace blink {

class Blob;
class BlobDataHandle;
class ExceptionState;
class IDBValue;
class ScriptState;
class ScriptValue;
class SerializedScriptValue;
class SharedBuffer;

// Logic for serializing V8 values for storage in IndexedDB.
//
// V8 values are stored on disk using the format implemented in
// SerializedScriptValue (SSV), which is essentialy a byte array plus an array
// of attached Blobs. For "normal" (not too large) V8 values, the SSV output's
// byte array is stored directly in IndexedDB's backing store, together with
// references to the attached Blobs.
//
// "Large" V8 values are wrapped in Blobs, in order to avoid operating the
// backing store in a sub-optimal region. Specifically, the byte array in the
// SSV output is replaced with a "wrapped value" marker, and stored inside a
// Blob that is tacked to the end of the SSV's Blob array. IndexedDB's backing
// store receives the "wrapped value" marker and the references to the Blobs,
// while the large byte array in the SSV output is handled by the Blob storage
// system.
//
// In summary:
// "normal" v8::Value -> SSV -> IDBValue (stores SSV output) -> LevelDB
// "large" v8::Value -> SSV -> IDBValue (stores SSV output) ->
//     Blob (stores SSV output) + IDBValue (stores Blob reference) -> LevelDB
//
// Full picture that accounts for Blob attachments:
// "normal" v8::Value -> SSV (byte array, Blob attachments) ->
//     IDBValue (bytes: SSV byte array, blobs: SSV Blob attachments) -> LevelDB
// "large" v8::Value -> SSV (byte array, Blob attachments) ->
//     IDBValue (bytes: "wrapped value" marker,
//               blobs: SSV Blob attachments + [wrapper Blob(SSV byte array)] ->
//     LevelDB
class MODULES_EXPORT IDBValueWrapper {
  STACK_ALLOCATED();

 public:
  // Wrapper for an IndexedDB value.
  //
  // The serialization process can throw an exception. The caller is responsible
  // for checking exception_state.
  IDBValueWrapper(
      v8::Isolate*,
      v8::Local<v8::Value>,
      SerializedScriptValue::SerializeOptions::WasmSerializationPolicy,
      ExceptionState&);
  ~IDBValueWrapper();

  // Creates a clone of the serialized value.
  //
  // This method is used to fulfill the IndexedDB specification requirement that
  // a value's key and index keys are extracted from a structured clone of the
  // value, which avoids the issue of side-effects in custom getters.
  //
  // This method cannot be called after WrapIfBiggerThan().
  void Clone(ScriptState*, ScriptValue* clone);

  // Conditionally wraps the serialized value's byte array into a Blob.
  //
  // The byte array is wrapped if its size exceeds max_bytes. In production, the
  // max_bytes threshold is currently always kWrapThreshold.
  //
  // This method must be called before ExtractWireBytes() and cannot be called
  // after ExtractWireBytes().
  bool WrapIfBiggerThan(unsigned max_bytes);

  // Obtains the BlobDataHandles from the serialized value's Blob array.
  //
  // This method must be called at most once, and must be called after
  // WrapIfBiggerThan().
  void ExtractBlobDataHandles(
      Vector<scoped_refptr<BlobDataHandle>>* blob_data_handles);

  // Obtains the byte array for the serialized value.
  //
  // This method must be called at most once, and must be called after
  // WrapIfBiggerThan().
  scoped_refptr<SharedBuffer> ExtractWireBytes();

  // Obtains WebBlobInfos for the serialized value's Blob array.
  //
  // This method must be called at most once, and must be called after
  // WrapIfBiggerThan().
  inline Vector<WebBlobInfo>& WrappedBlobInfo() {
#if DCHECK_IS_ON()
    DCHECK(!had_exception_)
        << "WrapBlobInfo() called on wrapper with serialization exception";
#endif  // DCHECK_IS_ON()
    return blob_info_;
  }

  size_t DataLengthBeforeWrapInBytes() { return original_data_length_; }

  // Default threshold for WrapIfBiggerThan().
  //
  // This should be tuned to achieve a compromise between short-term IndexedDB
  // throughput and long-term I/O load and memory usage. LevelDB, the underlying
  // storage for IndexedDB, was not designed with large values in mind. At the
  // very least, large values will slow down compaction, causing occasional I/O
  // spikes.
  //
  // TODO(crbug.com/756754): 128MB is the maximum IPC size, so this threshold
  // effectively disables wrapping. Set the threshold back to 64 * 1024 after
  // the Blob leak issue is fixed.
  static constexpr unsigned kWrapThreshold = 128 * 1024 * 1024;

  // MIME type used for Blobs that wrap IDBValues.
  static constexpr const char* kWrapMimeType =
      "application/vnd.blink-idb-value-wrapper";

 private:
  // Used to serialize the wrapped value.
  static void WriteVarint(unsigned value, Vector<char>& output);

  scoped_refptr<SerializedScriptValue> serialized_value_;
  scoped_refptr<BlobDataHandle> wrapper_handle_;
  Vector<WebBlobInfo> blob_info_;
  Vector<char> wire_bytes_;
  size_t original_data_length_ = 0;
#if DCHECK_IS_ON()
  bool had_exception_ = false;
  bool wrap_called_ = false;
#endif  // DCHECK_IS_ON()
};

// State and logic for unwrapping large IndexedDB values from Blobs.
//
// See IDBValueWrapper for an explanation of the wrapping concept.
//
// Once created, an IDBValueUnwrapper instance can be used to unwrap multiple
// Blobs. For each Blob to be unwrapped, the caller should first call Parse().
// If the method succeeds, the IDBValueUnwrapper will store the parse state,
// which can be obtained using WrapperBlobSize() and WrapperBlobHandle().
class MODULES_EXPORT IDBValueUnwrapper {
  STACK_ALLOCATED();

 public:
  IDBValueUnwrapper();

  // True if the IDBValue's data was wrapped in a Blob.
  static bool IsWrapped(IDBValue*);

  // True if at least one of the IDBValues' data was wrapped in a Blob.
  static bool IsWrapped(const Vector<scoped_refptr<IDBValue>>&);

  // Pieces together an unwrapped IDBValue from a wrapped value and Blob data.
  static scoped_refptr<IDBValue> Unwrap(
      IDBValue* wrapped_value,
      scoped_refptr<SharedBuffer>&& wrapper_blob_content);

  // Parses the wrapper Blob information from a wrapped IDBValue.
  //
  // Returns true for success, and false for failure. Failure can mean that the
  // given value was not a wrapped IDBValue, or that the value bytes were
  // corrupted.
  bool Parse(IDBValue*);

  // Returns the size of the Blob obtained by the last Unwrap() call.
  //
  // Should only be called after a successful result from Unwrap().
  inline unsigned WrapperBlobSize() const {
    DCHECK(end_);
    return blob_size_;
  }

  // Returns a handle to the Blob obtained by the last Unwrap() call.
  //
  // Should only be called exactly once after a successful result from Unwrap().
  scoped_refptr<BlobDataHandle> WrapperBlobHandle();

 private:
  // Used to deserialize the wrapped value.
  bool ReadVarint(unsigned& value);

  // Resets the parsing state.
  bool Reset();

  // Deserialization cursor in the SharedBuffer of the IDBValue being unwrapped.
  const uint8_t* current_;

  // Smallest invalid position_ value.
  const uint8_t* end_;

  // The size of the Blob holding the data for the last unwrapped IDBValue.
  unsigned blob_size_;

  // Handle to the Blob holding the data for the last unwrapped IDBValue.
  scoped_refptr<BlobDataHandle> blob_handle_;
};

}  // namespace blink

#endif  // IDBValueWrapping_h
