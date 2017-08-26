// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8ContextSnapshot.h"

#include <array>
#include <cstring>

#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8Window.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

intptr_t* g_v8_context_snapshot_reference_table = nullptr;

// TODO(peria): This method is almost a copy of
// V8PerContext::ConstructorForTypeSlowCase(), so merge with it.
v8::Local<v8::Function> ConstructPlainType(v8::Isolate* isolate,
                                           const DOMWrapperWorld& world,
                                           v8::Local<v8::Context> context,
                                           const WrapperTypeInfo* type) {
  v8::Context::Scope scope(context);
  // We shouldn't reach this point for the types that are implemented in v8 such
  // as typed arrays and hence don't have domTemplateFunction.
  DCHECK(type->dom_template_function);
  v8::Local<v8::FunctionTemplate> interface_template =
      type->domTemplate(isolate, world);
  // Getting the function might fail if we're running out of stack or memory.
  v8::Local<v8::Function> interface_object =
      interface_template->GetFunction(context).ToLocalChecked();

  if (type->parent_class) {
    v8::Local<v8::Object> prototype_template =
        ConstructPlainType(isolate, world, context, type->parent_class);
    CHECK(interface_object->SetPrototype(context, prototype_template)
              .ToChecked());
  }

  v8::Local<v8::Value> prototype_value =
      interface_object->Get(context, V8AtomicString(isolate, "prototype"))
          .ToLocalChecked();
  CHECK(prototype_value->IsObject());
  v8::Local<v8::Object> prototype_object = prototype_value.As<v8::Object>();
  if (prototype_object->InternalFieldCount() ==
          kV8PrototypeInternalFieldcount &&
      type->wrapper_type_prototype ==
          WrapperTypeInfo::kWrapperTypeObjectPrototype) {
    prototype_object->SetAlignedPointerInInternalField(
        kV8PrototypeTypeIndex, const_cast<WrapperTypeInfo*>(type));
  }
  type->PreparePrototypeAndInterfaceObject(
      context, world, prototype_object, interface_object, interface_template);

  return interface_object;
}

// TODO(peria): This method is almost a copy of
// V8PerContext::CreateWrapperFromCacheSlowCase(), so merge with it.
v8::Local<v8::Object> CreatePlainWrapper(v8::Isolate* isolate,
                                         const DOMWrapperWorld& world,
                                         v8::Local<v8::Context> context,
                                         const WrapperTypeInfo* type) {
  CHECK(V8HTMLDocument::wrapperTypeInfo.Equals(type));

  v8::Context::Scope scope(context);
  v8::Local<v8::Function> interface_object =
      ConstructPlainType(isolate, world, context, type);
  CHECK(!interface_object.IsEmpty());
  v8::Local<v8::Object> instance_template =
      V8ObjectConstructor::NewInstance(isolate, interface_object)
          .ToLocalChecked();
  v8::Local<v8::Object> wrapper = instance_template->Clone();
  wrapper->SetAlignedPointerInInternalField(kV8DOMWrapperTypeIndex,
                                            const_cast<WrapperTypeInfo*>(type));
  return wrapper;
}

int GetSnapshotIndexForWorld(const DOMWrapperWorld& world) {
  return world.IsMainWorld() ? 0 : 1;
}

// Interface templates of those classes are stored in a snapshot without any
// runtime enabled features, so we have to install runtime enabled features on
// them after instantiation.
struct SnapshotInterface {
  const WrapperTypeInfo* wrapper_type_info;
  InstallRuntimeEnabledFeaturesOnTemplateFunction install_function;
};
SnapshotInterface g_snapshot_interfaces[] = {
    {&V8Window::wrapperTypeInfo,
     V8Window::InstallRuntimeEnabledFeaturesOnTemplate},
    {&V8HTMLDocument::wrapperTypeInfo,
     V8HTMLDocument::InstallRuntimeEnabledFeaturesOnTemplate},
    {&V8EventTarget::wrapperTypeInfo,
     V8EventTarget::InstallRuntimeEnabledFeaturesOnTemplate},
    {&V8Node::wrapperTypeInfo, V8Node::InstallRuntimeEnabledFeaturesOnTemplate},
    {&V8Document::wrapperTypeInfo,
     V8Document::InstallRuntimeEnabledFeaturesOnTemplate},
};
constexpr size_t kSnapshotInterfaceSize =
    WTF_ARRAY_LENGTH(g_snapshot_interfaces);

enum class InternalFieldType : uint8_t {
  kNone,
  kNodeType,
  kDocumentType,
  kHTMLDocumentType,
  kHTMLDocumentObject,
};

const WrapperTypeInfo* FieldTypeToWrapperTypeInfo(InternalFieldType type) {
  switch (type) {
    case InternalFieldType::kNone:
      NOTREACHED();
      break;
    case InternalFieldType::kNodeType:
      return &V8Node::wrapperTypeInfo;
    case InternalFieldType::kDocumentType:
      return &V8Document::wrapperTypeInfo;
    case InternalFieldType::kHTMLDocumentType:
      return &V8HTMLDocument::wrapperTypeInfo;
    case InternalFieldType::kHTMLDocumentObject:
      return &V8HTMLDocument::wrapperTypeInfo;
  }
  NOTREACHED();
  return nullptr;
}

struct DataForDeserializer {
  STACK_ALLOCATED();
  Member<Document> document;
};

int CountExternalReferenceEntries() {
  if (!g_v8_context_snapshot_reference_table)
    return 0;

  int count = 0;
  for (intptr_t* p = g_v8_context_snapshot_reference_table; *p; ++p)
    ++count;
  return count;
}

}  // namespace

v8::Local<v8::Context> V8ContextSnapshot::CreateContextFromSnapshot(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::ExtensionConfiguration* extension_configuration,
    v8::Local<v8::Object> global_proxy,
    Document* document) {
  if (!CanCreateContextFromSnapshot(isolate, world, document)) {
    return v8::Local<v8::Context>();
  }

  const int index = GetSnapshotIndexForWorld(world);
  DataForDeserializer data{document};
  v8::DeserializeInternalFieldsCallback callback =
      v8::DeserializeInternalFieldsCallback(&DeserializeInternalField, &data);
  v8::Local<v8::Context> context =
      v8::Context::FromSnapshot(isolate, index, callback,
                                extension_configuration, global_proxy)
          .ToLocalChecked();
  VLOG(1) << "A context is created from snapshot for "
          << (world.IsMainWorld() ? "" : "non-") << "main world";

  return context;
}

void V8ContextSnapshot::InstallRuntimeEnabledFeatures(
    v8::Local<v8::Context> context,
    Document* document) {
  ScriptState* script_state = ScriptState::From(context);
  v8::Isolate* isolate = script_state->GetIsolate();
  const DOMWrapperWorld& world = script_state->World();
  if (!CanCreateContextFromSnapshot(isolate, world, document)) {
    return;
  }

  TRACE_EVENT1("v8", "V8ContextSnapshot::InstallRuntimeEnabled", "IsMainFrame",
               world.IsMainWorld());

  v8::Local<v8::String> prototype_str = V8AtomicString(isolate, "prototype");
  V8PerContextData* data = script_state->PerContextData();

  v8::Local<v8::Object> global_proxy = context->Global();
  {
    v8::Local<v8::Object> window_wrapper =
        global_proxy->GetPrototype().As<v8::Object>();
    const WrapperTypeInfo* type = &V8Window::wrapperTypeInfo;
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8Window::install_runtime_enabled_features_function_(
        isolate, world, window_wrapper, prototype, interface);
  }
  {
    const WrapperTypeInfo* type = &V8EventTarget::wrapperTypeInfo;
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8EventTarget::InstallRuntimeEnabledFeatures(
        isolate, world, v8::Local<v8::Object>(), prototype, interface);
  }

  if (!world.IsMainWorld()) {
    return;
  }

  // The below code handles window.document on the main world.
  {
    CHECK(document);
    DCHECK(document->IsHTMLDocument());
    CHECK(document->ContainsWrapper());
    v8::Local<v8::Object> document_wrapper =
        ToV8(document, global_proxy, isolate).As<v8::Object>();
    const WrapperTypeInfo* type = &V8HTMLDocument::wrapperTypeInfo;
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8HTMLDocument::InstallRuntimeEnabledFeatures(
        isolate, world, document_wrapper, prototype, interface);
  }
  {
    const WrapperTypeInfo* type = &V8Document::wrapperTypeInfo;
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8Document::InstallRuntimeEnabledFeatures(
        isolate, world, v8::Local<v8::Object>(), prototype, interface);
  }
  {
    const WrapperTypeInfo* type = &V8Node::wrapperTypeInfo;
    v8::Local<v8::Function> interface = data->ConstructorForType(type);
    v8::Local<v8::Object> prototype = interface->Get(context, prototype_str)
                                          .ToLocalChecked()
                                          .As<v8::Object>();
    V8Node::InstallRuntimeEnabledFeatures(
        isolate, world, v8::Local<v8::Object>(), prototype, interface);
  }
}

void V8ContextSnapshot::EnsureInterfaceTemplates(v8::Isolate* isolate) {
  if (V8PerIsolateData::From(isolate)->GetV8ContextSnapshotMode() !=
      V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot) {
    return;
  }

  v8::HandleScope handle_scope(isolate);
  SnapshotInterface& snapshot_window = g_snapshot_interfaces[0];
  DCHECK(V8Window::wrapperTypeInfo.Equals(snapshot_window.wrapper_type_info));
  // Update the install function for V8Window to work for partial interfaces.
  snapshot_window.install_function =
      V8Window::install_runtime_enabled_features_on_template_function_;

  EnsureInterfaceTemplatesForWorld(isolate, DOMWrapperWorld::MainWorld());
  // Any world types other than |kMain| are acceptable for this.
  RefPtr<DOMWrapperWorld> isolated_world = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kForV8ContextSnapshotNonMain);
  EnsureInterfaceTemplatesForWorld(isolate, *isolated_world);
}

void V8ContextSnapshot::SetReferenceTable(intptr_t* table) {
  DCHECK(!g_v8_context_snapshot_reference_table);
  g_v8_context_snapshot_reference_table = table;
}

intptr_t* V8ContextSnapshot::GetReferenceTable() {
  return g_v8_context_snapshot_reference_table;
}

v8::StartupData V8ContextSnapshot::TakeSnapshot() {
  DCHECK_EQ(V8PerIsolateData::From(V8PerIsolateData::MainThreadIsolate())
                ->GetV8ContextSnapshotMode(),
            V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot);

  v8::SnapshotCreator* creator =
      V8PerIsolateData::From(V8PerIsolateData::MainThreadIsolate())
          ->GetSnapshotCreator();
  v8::Isolate* isolate = creator->GetIsolate();
  CHECK_EQ(isolate, v8::Isolate::GetCurrent());

  VLOG(1) << "External reference table has " << CountExternalReferenceEntries()
          << " entries.";

  // Disable all runtime enabled features
  RuntimeEnabledFeatures::SetStableFeaturesEnabled(false);
  RuntimeEnabledFeatures::SetExperimentalFeaturesEnabled(false);
  RuntimeEnabledFeatures::SetTestFeaturesEnabled(false);

  {
    v8::HandleScope handleScope(isolate);
    creator->SetDefaultContext(v8::Context::New(isolate));

    TakeSnapshotForWorld(creator, DOMWrapperWorld::MainWorld());
    // For non main worlds, we can use any type to create a context.
    TakeSnapshotForWorld(
        creator,
        *DOMWrapperWorld::Create(
            isolate, DOMWrapperWorld::WorldType::kForV8ContextSnapshotNonMain));
  }

  isolate->RemoveMessageListeners(V8Initializer::MessageHandlerInMainThread);

  v8::StartupData blob =
      creator->CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);

  return blob;
}

v8::StartupData V8ContextSnapshot::SerializeInternalField(
    v8::Local<v8::Object> object,
    int index,
    void*) {
  InternalFieldType field_type = InternalFieldType::kNone;
  const WrapperTypeInfo* wrapper_type = ToWrapperTypeInfo(object);
  if (kV8DOMWrapperObjectIndex == index) {
    if (blink::V8HTMLDocument::wrapperTypeInfo.Equals(wrapper_type)) {
      field_type = InternalFieldType::kHTMLDocumentObject;
    }
    DCHECK_LE(kV8DefaultWrapperInternalFieldCount,
              object->InternalFieldCount());
  } else if (kV8DOMWrapperTypeIndex == index) {
    if (blink::V8HTMLDocument::wrapperTypeInfo.Equals(wrapper_type)) {
      field_type = InternalFieldType::kHTMLDocumentType;
    } else if (blink::V8Document::wrapperTypeInfo.Equals(wrapper_type)) {
      field_type = InternalFieldType::kDocumentType;
    } else if (blink::V8Node::wrapperTypeInfo.Equals(wrapper_type)) {
      field_type = InternalFieldType::kNodeType;
    }
    DCHECK_LE(kV8PrototypeInternalFieldcount, object->InternalFieldCount());
  }
  CHECK_NE(field_type, InternalFieldType::kNone);

  int size = sizeof(InternalFieldType);
  // Allocated memory on |data| will be released in
  // v8::i::PartialSerializer::SerializeEmbedderFields().
  char* data = new char[size];
  std::memcpy(data, &field_type, size);

  return {data, size};
}

void V8ContextSnapshot::DeserializeInternalField(v8::Local<v8::Object> object,
                                                 int index,
                                                 v8::StartupData payload,
                                                 void* ptr) {
  // DeserializeInternalField() expects to be called in the main world
  // with |document| being HTMLDocument.
  CHECK_EQ(payload.raw_size, static_cast<int>(sizeof(InternalFieldType)));
  InternalFieldType type =
      *reinterpret_cast<const InternalFieldType*>(payload.data);

  const WrapperTypeInfo* wrapper_type_info = FieldTypeToWrapperTypeInfo(type);
  switch (type) {
    case InternalFieldType::kNodeType:
    case InternalFieldType::kDocumentType:
    case InternalFieldType::kHTMLDocumentType: {
      CHECK_EQ(index, kV8DOMWrapperTypeIndex);
      object->SetAlignedPointerInInternalField(
          index, const_cast<WrapperTypeInfo*>(wrapper_type_info));
      return;
    }
    case InternalFieldType::kHTMLDocumentObject: {
      // The below code handles window.document on the main world.
      CHECK_EQ(index, kV8DOMWrapperObjectIndex);
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      DataForDeserializer* data = static_cast<DataForDeserializer*>(ptr);
      ScriptWrappable* document = data->document;
      DCHECK(document);

      // Make reference from wrapper to document
      object->SetAlignedPointerInInternalField(index, document);
      // Make reference from document to wrapper
      CHECK(document->SetWrapper(isolate, wrapper_type_info, object));
      WrapperTypeInfo::WrapperCreated();
      return;
    }
    case InternalFieldType::kNone:
      NOTREACHED();
      return;
  }

  NOTREACHED();
}

bool V8ContextSnapshot::CanCreateContextFromSnapshot(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    Document* document) {
  DCHECK(document);
  if (V8PerIsolateData::From(isolate)->GetV8ContextSnapshotMode() !=
      V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot) {
    return false;
  }

  // When creating a context for the main world from snapshot, we also need a
  // HTMLDocument instance. If typeof window.document is not HTMLDocument, e.g.
  // SVGDocument or XMLDocument, we can't create contexts from the snapshot.
  return !world.IsMainWorld() || document->IsHTMLDocument();
}

void V8ContextSnapshot::EnsureInterfaceTemplatesForWorld(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world) {
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);

  // A snapshot has some interface templates in it.  The first
  // |kSnapshotInterfaceSize| templates are for the main world, and the
  // remaining templates are for isolated worlds.
  const int index_offset = world.IsMainWorld() ? 0 : kSnapshotInterfaceSize;

  for (size_t i = 0; i < kSnapshotInterfaceSize; ++i) {
    auto& snapshot_interface = g_snapshot_interfaces[i];
    const WrapperTypeInfo* wrapper_type_info =
        snapshot_interface.wrapper_type_info;
    v8::Local<v8::FunctionTemplate> interface_template =
        v8::FunctionTemplate::FromSnapshot(isolate, index_offset + i)
            .ToLocalChecked();
    snapshot_interface.install_function(isolate, world, interface_template);
    CHECK(!interface_template.IsEmpty());
    data->SetInterfaceTemplate(world, wrapper_type_info, interface_template);
  }
}

void V8ContextSnapshot::TakeSnapshotForWorld(v8::SnapshotCreator* creator,
                                             const DOMWrapperWorld& world) {
  v8::Isolate* isolate = creator->GetIsolate();
  CHECK_EQ(isolate, v8::Isolate::GetCurrent());

  // Function templates
  v8::HandleScope handleScope(isolate);
  std::array<v8::Local<v8::FunctionTemplate>, kSnapshotInterfaceSize>
      interface_templates;
  v8::Local<v8::FunctionTemplate> window_template;
  for (size_t i = 0; i < kSnapshotInterfaceSize; ++i) {
    const WrapperTypeInfo* wrapper_type_info =
        g_snapshot_interfaces[i].wrapper_type_info;
    v8::Local<v8::FunctionTemplate> interface_template =
        wrapper_type_info->domTemplate(isolate, world);
    CHECK(!interface_template.IsEmpty());
    interface_templates[i] = interface_template;
    if (V8Window::wrapperTypeInfo.Equals(wrapper_type_info)) {
      window_template = interface_template;
    }
  }
  CHECK(!window_template.IsEmpty());

  v8::Local<v8::ObjectTemplate> window_instance_template =
      window_template->InstanceTemplate();
  CHECK(!window_instance_template.IsEmpty());

  v8::Local<v8::Context> context;
  {
    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(isolate));
    context = v8::Context::New(isolate, nullptr, window_instance_template);
  }
  CHECK(!context.IsEmpty());

  // For the main world context, we need to prepare a HTMLDocument wrapper and
  // set it to window.documnt.
  if (world.IsMainWorld()) {
    v8::Context::Scope scope(context);
    v8::Local<v8::Object> document_wrapper = CreatePlainWrapper(
        isolate, world, context, &V8HTMLDocument::wrapperTypeInfo);
    int indices[] = {kV8DOMWrapperObjectIndex, kV8DOMWrapperTypeIndex};
    void* values[] = {nullptr, const_cast<WrapperTypeInfo*>(
                                   &V8HTMLDocument::wrapperTypeInfo)};
    document_wrapper->SetAlignedPointerInInternalFields(
        WTF_ARRAY_LENGTH(indices), indices, values);

    // Set the cached accessor for window.document.
    CHECK(V8PrivateProperty::GetWindowDocumentCachedAccessor(isolate).Set(
        context->Global(), document_wrapper));
  }

  for (auto& interface_template : interface_templates) {
    creator->AddTemplate(interface_template);
  }
  creator->AddContext(context, SerializeInternalField);

  V8PerIsolateData::From(isolate)->ClearPersistentsForV8ContextSnapshot();
}

}  // namespace blink
