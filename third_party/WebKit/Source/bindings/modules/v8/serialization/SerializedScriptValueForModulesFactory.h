// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SerializedScriptValueForModulesFactory_h
#define SerializedScriptValueForModulesFactory_h

#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class SerializedScriptValueForModulesFactory final
    : public SerializedScriptValueFactory {
  USING_FAST_MALLOC(SerializedScriptValueForModulesFactory);
  WTF_MAKE_NONCOPYABLE(SerializedScriptValueForModulesFactory);

 public:
  SerializedScriptValueForModulesFactory() : SerializedScriptValueFactory() {}

 protected:
  PassRefPtr<SerializedScriptValue> Create(
      v8::Isolate*,
      v8::Local<v8::Value>,
      const SerializedScriptValue::SerializeOptions&,
      ExceptionState&) override;

  v8::Local<v8::Value> Deserialize(
      RefPtr<SerializedScriptValue>,
      v8::Isolate*,
      const SerializedScriptValue::DeserializeOptions&) override;

  v8::Local<v8::Value> Deserialize(
      UnpackedSerializedScriptValue*,
      v8::Isolate*,
      const SerializedScriptValue::DeserializeOptions&) override;
};

}  // namespace blink

#endif  // SerializedScriptValueForModulesFactory_h
