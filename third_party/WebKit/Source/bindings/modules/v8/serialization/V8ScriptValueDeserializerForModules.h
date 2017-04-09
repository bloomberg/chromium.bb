// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ScriptValueDeserializerForModules_h
#define V8ScriptValueDeserializerForModules_h

#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "modules/ModulesExport.h"

namespace blink {

class CryptoKey;

// Extends V8ScriptValueSerializer with support for modules/ types.
class MODULES_EXPORT V8ScriptValueDeserializerForModules final
    : public V8ScriptValueDeserializer {
 public:
  explicit V8ScriptValueDeserializerForModules(
      RefPtr<ScriptState> script_state,
      RefPtr<SerializedScriptValue> serialized_script_value,
      const Options& options = Options())
      : V8ScriptValueDeserializer(script_state,
                                  serialized_script_value,
                                  options) {}

 protected:
  ScriptWrappable* ReadDOMObject(SerializationTag) override;

 private:
  bool ReadOneByte(uint8_t* byte) {
    const void* data;
    if (!ReadRawBytes(1, &data))
      return false;
    *byte = *reinterpret_cast<const uint8_t*>(data);
    return true;
  }
  CryptoKey* ReadCryptoKey();
};

}  // namespace blink

#endif  // V8ScriptValueDeserializerForModules_h
