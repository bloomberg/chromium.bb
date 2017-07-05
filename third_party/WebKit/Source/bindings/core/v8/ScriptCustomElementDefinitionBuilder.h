// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptCustomElementDefinitionBuilder_h
#define ScriptCustomElementDefinitionBuilder_h

#include "core/CoreExport.h"
#include "core/html/custom/CustomElementDefinitionBuilder.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringView.h"
#include "v8.h"

namespace blink {

class CustomElementRegistry;
class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT ScriptCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinitionBuilder);

 public:
  ScriptCustomElementDefinitionBuilder(
      ScriptState*,
      CustomElementRegistry*,
      const ScriptValue& constructor_script_value,
      ExceptionState&);
  ~ScriptCustomElementDefinitionBuilder() = default;

  bool CheckConstructorIntrinsics() override;
  bool CheckConstructorNotRegistered() override;
  bool CheckPrototype() override;
  bool RememberOriginalProperties() override;
  CustomElementDefinition* Build(const CustomElementDescriptor&,
                                 CustomElementDefinition::Id) override;

 private:
  static ScriptCustomElementDefinitionBuilder* stack_;

  RefPtr<ScriptState> script_state_;
  Member<CustomElementRegistry> registry_;
  v8::Local<v8::Value> constructor_value_;
  v8::Local<v8::Object> constructor_;
  v8::Local<v8::Object> prototype_;
  v8::Local<v8::Function> connected_callback_;
  v8::Local<v8::Function> disconnected_callback_;
  v8::Local<v8::Function> adopted_callback_;
  v8::Local<v8::Function> attribute_changed_callback_;
  HashSet<AtomicString> observed_attributes_;
  ExceptionState& exception_state_;

  bool ValueForName(v8::Isolate*,
                    v8::Local<v8::Context>&,
                    const v8::TryCatch&,
                    const v8::Local<v8::Object>&,
                    const StringView&,
                    v8::Local<v8::Value>&) const;
  bool CallableForName(v8::Isolate*,
                       v8::Local<v8::Context>&,
                       const v8::TryCatch&,
                       const StringView&,
                       v8::Local<v8::Function>&) const;
  bool RetrieveObservedAttributes(v8::Isolate*,
                                  v8::Local<v8::Context>&,
                                  const v8::TryCatch&);
};

}  // namespace blink

#endif  // ScriptCustomElementDefinitionBuilder_h
