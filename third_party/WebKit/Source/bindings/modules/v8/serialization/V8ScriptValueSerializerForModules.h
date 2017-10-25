// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ScriptValueSerializerForModules_h
#define V8ScriptValueSerializerForModules_h

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"
#include "modules/ModulesExport.h"
#include "public/platform/WebCryptoAlgorithm.h"

namespace blink {

class WebCryptoKey;

// Extends V8ScriptValueSerializer with support for modules/ types.
class MODULES_EXPORT V8ScriptValueSerializerForModules final
    : public V8ScriptValueSerializer {
 public:
  explicit V8ScriptValueSerializerForModules(
      scoped_refptr<ScriptState> script_state,
      const SerializedScriptValue::SerializeOptions& options)
      : V8ScriptValueSerializer(script_state, options) {}

 protected:
  bool WriteDOMObject(ScriptWrappable*, ExceptionState&) override;

 private:
  void WriteOneByte(uint8_t byte) { WriteRawBytes(&byte, 1); }
  bool WriteCryptoKey(const WebCryptoKey&, ExceptionState&);
};

}  // namespace blink

#endif  // V8ScriptValueSerializerForModules_h
