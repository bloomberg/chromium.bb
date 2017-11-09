/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "platform/bindings/V8PerContextData.h"

#include <stdlib.h>
#include <memory>
#include "platform/InstanceCounters.h"
#include "platform/bindings/ConditionalFeatures.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StringExtras.h"

namespace blink {

V8PerContextData::V8PerContextData(v8::Local<v8::Context> context)
    : isolate_(context->GetIsolate()),
      wrapper_boilerplates_(isolate_),
      constructor_map_(isolate_),
      context_holder_(std::make_unique<gin::ContextHolder>(isolate_)),
      context_(isolate_, context),
      activity_logger_(nullptr) {
  context_holder_->SetContext(context);

  if (IsMainThread()) {
    InstanceCounters::IncrementCounter(
        InstanceCounters::kV8PerContextDataCounter);
  }
}

V8PerContextData::~V8PerContextData() {
  if (IsMainThread()) {
    InstanceCounters::DecrementCounter(
        InstanceCounters::kV8PerContextDataCounter);
  }
}

std::unique_ptr<V8PerContextData> V8PerContextData::Create(
    v8::Local<v8::Context> context) {
  return WTF::WrapUnique(new V8PerContextData(context));
}

V8PerContextData* V8PerContextData::From(v8::Local<v8::Context> context) {
  return ScriptState::From(context)->PerContextData();
}

v8::Local<v8::Object> V8PerContextData::CreateWrapperFromCacheSlowCase(
    const WrapperTypeInfo* type) {
  v8::Context::Scope scope(GetContext());
  v8::Local<v8::Function> interface_object = ConstructorForType(type);
  CHECK(!interface_object.IsEmpty());
  v8::Local<v8::Object> instance_template =
      V8ObjectConstructor::NewInstance(isolate_, interface_object)
          .ToLocalChecked();
  wrapper_boilerplates_.Set(type, instance_template);
  return instance_template->Clone();
}

v8::Local<v8::Function> V8PerContextData::ConstructorForTypeSlowCase(
    const WrapperTypeInfo* type) {
  v8::Local<v8::Context> current_context = GetContext();
  v8::Context::Scope scope(current_context);
  const DOMWrapperWorld& world = DOMWrapperWorld::World(current_context);
  // We shouldn't reach this point for the types that are implemented in v8 such
  // as typed arrays and hence don't have domTemplateFunction.
  DCHECK(type->dom_template_function);
  v8::Local<v8::FunctionTemplate> interface_template =
      type->domTemplate(isolate_, world);
  // Getting the function might fail if we're running out of stack or memory.
  v8::Local<v8::Function> interface_object;
  if (!interface_template->GetFunction(current_context)
           .ToLocal(&interface_object))
    return v8::Local<v8::Function>();

  if (type->parent_class) {
    v8::Local<v8::Object> prototype_template =
        ConstructorForType(type->parent_class);
    if (prototype_template.IsEmpty())
      return v8::Local<v8::Function>();
    if (!V8CallBoolean(interface_object->SetPrototype(current_context,
                                                      prototype_template)))
      return v8::Local<v8::Function>();
  }

  v8::Local<v8::Object> prototype_object;
  if (type->wrapper_type_prototype !=
      WrapperTypeInfo::kWrapperTypeNoPrototype) {
    v8::Local<v8::Value> prototype_value;
    if (!interface_object
             ->Get(current_context, V8AtomicString(isolate_, "prototype"))
             .ToLocal(&prototype_value) ||
        !prototype_value->IsObject())
      return v8::Local<v8::Function>();
    prototype_object = prototype_value.As<v8::Object>();
    if (prototype_object->InternalFieldCount() ==
            kV8PrototypeInternalFieldcount &&
        type->wrapper_type_prototype ==
            WrapperTypeInfo::kWrapperTypeObjectPrototype) {
      prototype_object->SetAlignedPointerInInternalField(
          kV8PrototypeTypeIndex, const_cast<WrapperTypeInfo*>(type));
    }
    type->PreparePrototypeAndInterfaceObject(current_context, world,
                                             prototype_object, interface_object,
                                             interface_template);
  }

  // Origin Trials
  InstallConditionalFeatures(type, ScriptState::From(current_context),
                             prototype_object, interface_object);
  constructor_map_.Set(type, interface_object);
  return interface_object;
}

v8::Local<v8::Object> V8PerContextData::PrototypeForType(
    const WrapperTypeInfo* type) {
  v8::Local<v8::Object> constructor = ConstructorForType(type);
  if (constructor.IsEmpty())
    return v8::Local<v8::Object>();
  v8::Local<v8::Value> prototype_value;
  if (!constructor->Get(GetContext(), V8AtomicString(isolate_, "prototype"))
           .ToLocal(&prototype_value) ||
      !prototype_value->IsObject())
    return v8::Local<v8::Object>();
  return prototype_value.As<v8::Object>();
}

bool V8PerContextData::GetExistingConstructorAndPrototypeForType(
    const WrapperTypeInfo* type,
    v8::Local<v8::Object>* prototype_object,
    v8::Local<v8::Function>* interface_object) {
  *interface_object = constructor_map_.Get(type);
  if (interface_object->IsEmpty()) {
    *prototype_object = v8::Local<v8::Object>();
    return false;
  }
  *prototype_object = PrototypeForType(type);
  DCHECK(!prototype_object->IsEmpty());
  return true;
}

void V8PerContextData::AddCustomElementBinding(
    std::unique_ptr<V0CustomElementBinding> binding) {
  custom_element_bindings_.push_back(std::move(binding));
}

void V8PerContextData::AddData(const char* key, Data* data) {
  data_map_.Set(key, data);
}

void V8PerContextData::ClearData(const char* key) {
  data_map_.erase(key);
}

V8PerContextData::Data* V8PerContextData::GetData(const char* key) {
  return data_map_.at(key);
}

}  // namespace blink
