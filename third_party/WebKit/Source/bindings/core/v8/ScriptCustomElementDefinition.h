// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptCustomElementDefinition_h
#define ScriptCustomElementDefinition_h

#include "core/CoreExport.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "v8.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementRegistry;

class CORE_EXPORT ScriptCustomElementDefinition final
    : public CustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinition);

 public:
  static ScriptCustomElementDefinition* ForConstructor(
      ScriptState*,
      CustomElementRegistry*,
      const v8::Local<v8::Value>& constructor);

  static ScriptCustomElementDefinition* Create(
      ScriptState*,
      CustomElementRegistry*,
      const CustomElementDescriptor&,
      CustomElementDefinition::Id,
      const v8::Local<v8::Object>& constructor,
      const v8::Local<v8::Function>& connected_callback,
      const v8::Local<v8::Function>& disconnected_callback,
      const v8::Local<v8::Function>& adopted_callback,
      const v8::Local<v8::Function>& attribute_changed_callback,
      HashSet<AtomicString>&& observed_attributes);

  virtual ~ScriptCustomElementDefinition() = default;

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  v8::Local<v8::Object> Constructor() const;

  HTMLElement* CreateElementSync(Document&, const QualifiedName&) override;

  bool HasConnectedCallback() const override;
  bool HasDisconnectedCallback() const override;
  bool HasAdoptedCallback() const override;

  void RunConnectedCallback(Element*) override;
  void RunDisconnectedCallback(Element*) override;
  void RunAdoptedCallback(Element*,
                          Document* old_owner,
                          Document* new_owner) override;
  void RunAttributeChangedCallback(Element*,
                                   const QualifiedName&,
                                   const AtomicString& old_value,
                                   const AtomicString& new_value) override;

 private:
  ScriptCustomElementDefinition(
      ScriptState*,
      const CustomElementDescriptor&,
      const v8::Local<v8::Object>& constructor,
      const v8::Local<v8::Function>& connected_callback,
      const v8::Local<v8::Function>& disconnected_callback,
      const v8::Local<v8::Function>& adopted_callback,
      const v8::Local<v8::Function>& attribute_changed_callback,
      HashSet<AtomicString>&& observed_attributes);

  // Implementations of |CustomElementDefinition|
  ScriptValue GetConstructorForScript() final;
  bool RunConstructor(Element*) override;

  // Calls the constructor. The script scope, etc. must already be set up.
  Element* CallConstructor();

  void RunCallback(v8::Local<v8::Function>,
                   Element*,
                   int argc = 0,
                   v8::Local<v8::Value> argv[] = nullptr);

  HTMLElement* HandleCreateElementSyncException(Document&,
                                                const QualifiedName& tag_name,
                                                v8::Isolate*,
                                                ExceptionState&);

  RefPtr<ScriptState> script_state_;
  TraceWrapperV8Reference<v8::Object> constructor_;
  TraceWrapperV8Reference<v8::Function> connected_callback_;
  TraceWrapperV8Reference<v8::Function> disconnected_callback_;
  TraceWrapperV8Reference<v8::Function> adopted_callback_;
  TraceWrapperV8Reference<v8::Function> attribute_changed_callback_;
};

}  // namespace blink

#endif  // ScriptCustomElementDefinition_h
