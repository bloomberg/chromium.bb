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
  // TODO(jbroman): This should just be:
  // using V8ScriptValueDeserializer::V8ScriptValueDeserializer;
  // Unfortunately, MSVC 2015 emits C2248, claiming that it cannot access its
  // own private members. Until it's gone, we write the constructors by hand.
  V8ScriptValueDeserializerForModules(RefPtr<ScriptState> script_state,
                                      UnpackedSerializedScriptValue* unpacked,
                                      const Options& options = Options())
      : V8ScriptValueDeserializer(std::move(script_state), unpacked, options) {}
  V8ScriptValueDeserializerForModules(RefPtr<ScriptState> script_state,
                                      RefPtr<SerializedScriptValue> value,
                                      const Options& options = Options())
      : V8ScriptValueDeserializer(std::move(script_state),
                                  std::move(value),
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
