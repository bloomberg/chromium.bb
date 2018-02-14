/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/bindings/V8PerIsolateData.h"

#include <memory>

#include "platform/bindings/DOMDataStore.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/bindings/V8Binding.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/bindings/V8ValueCache.h"
#include "platform/wtf/LeakAnnotations.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/web/WebKit.h"
#include "v8/include/v8.h"

namespace blink {

// Wrapper function defined in WebKit.h
v8::Isolate* MainThreadIsolate() {
  return V8PerIsolateData::MainThreadIsolate();
}

static V8PerIsolateData* g_main_thread_per_isolate_data = nullptr;

static void BeforeCallEnteredCallback(v8::Isolate* isolate) {
  CHECK(!ScriptForbiddenScope::IsScriptForbidden());
}

static void MicrotasksCompletedCallback(v8::Isolate* isolate) {
  V8PerIsolateData::From(isolate)->RunEndOfScopeTasks();
}

V8PerIsolateData::V8PerIsolateData(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    V8ContextSnapshotMode v8_context_snapshot_mode)
    : v8_context_snapshot_mode_(v8_context_snapshot_mode),
      isolate_holder_(task_runner,
                      gin::IsolateHolder::kSingleThread,
                      IsMainThread() ? gin::IsolateHolder::kDisallowAtomicsWait
                                     : gin::IsolateHolder::kAllowAtomicsWait),
      interface_template_map_for_v8_context_snapshot_(GetIsolate()),
      string_cache_(WTF::WrapUnique(new StringCache(GetIsolate()))),
      private_property_(V8PrivateProperty::Create()),
      constructor_mode_(ConstructorMode::kCreateNewObject),
      use_counter_disabled_(false),
      is_handling_recursion_level_error_(false),
      is_reporting_exception_(false),
      runtime_call_stats_(base::DefaultTickClock::GetInstance()) {
  // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
  GetIsolate()->Enter();
  GetIsolate()->AddBeforeCallEnteredCallback(&BeforeCallEnteredCallback);
  GetIsolate()->AddMicrotasksCompletedCallback(&MicrotasksCompletedCallback);
  if (IsMainThread())
    g_main_thread_per_isolate_data = this;
}

// This constructor is used for creating a V8 context snapshot. It must run on
// the main thread.
V8PerIsolateData::V8PerIsolateData()
    : v8_context_snapshot_mode_(V8ContextSnapshotMode::kTakeSnapshot),
      isolate_holder_(Platform::Current()->MainThread()->GetTaskRunner(),
                      gin::IsolateHolder::kSingleThread,
                      gin::IsolateHolder::kAllowAtomicsWait,
                      gin::IsolateHolder::IsolateCreationMode::kCreateSnapshot),
      interface_template_map_for_v8_context_snapshot_(GetIsolate()),
      string_cache_(WTF::WrapUnique(new StringCache(GetIsolate()))),
      private_property_(V8PrivateProperty::Create()),
      constructor_mode_(ConstructorMode::kCreateNewObject),
      use_counter_disabled_(false),
      is_handling_recursion_level_error_(false),
      is_reporting_exception_(false),
      runtime_call_stats_(base::DefaultTickClock::GetInstance()) {
  CHECK(IsMainThread());

  // SnapshotCreator enters the isolate, so we don't call Isolate::Enter() here.
  g_main_thread_per_isolate_data = this;
}

V8PerIsolateData::~V8PerIsolateData() = default;

v8::Isolate* V8PerIsolateData::MainThreadIsolate() {
  DCHECK(g_main_thread_per_isolate_data);
  return g_main_thread_per_isolate_data->GetIsolate();
}

v8::Isolate* V8PerIsolateData::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    V8ContextSnapshotMode context_mode) {
  V8PerIsolateData* data = nullptr;
  if (context_mode == V8ContextSnapshotMode::kTakeSnapshot) {
    data = new V8PerIsolateData();
  } else {
    data = new V8PerIsolateData(task_runner, context_mode);
  }
  DCHECK(data);

  v8::Isolate* isolate = data->GetIsolate();
  isolate->SetData(gin::kEmbedderBlink, data);
  return isolate;
}

void V8PerIsolateData::EnableIdleTasks(
    v8::Isolate* isolate,
    std::unique_ptr<gin::V8IdleTaskRunner> task_runner) {
  From(isolate)->isolate_holder_.EnableIdleTasks(std::move(task_runner));
}

// willBeDestroyed() clear things that should be cleared before
// ThreadState::detach() gets called.
void V8PerIsolateData::WillBeDestroyed(v8::Isolate* isolate) {
  V8PerIsolateData* data = From(isolate);

  data->thread_debugger_.reset();
  // Clear any data that may have handles into the heap,
  // prior to calling ThreadState::detach().
  data->ClearEndOfScopeTasks();

  data->active_script_wrappables_.Clear();
}

// destroy() clear things that should be cleared after ThreadState::detach()
// gets called but before the Isolate exits.
void V8PerIsolateData::Destroy(v8::Isolate* isolate) {
  isolate->RemoveBeforeCallEnteredCallback(&BeforeCallEnteredCallback);
  isolate->RemoveMicrotasksCompletedCallback(&MicrotasksCompletedCallback);
  V8PerIsolateData* data = From(isolate);

  // Clear everything before exiting the Isolate.
  if (data->script_regexp_script_state_)
    data->script_regexp_script_state_->DisposePerContextData();
  data->private_property_.reset();
  data->string_cache_->Dispose();
  data->string_cache_.reset();
  data->interface_template_map_for_non_main_world_.clear();
  data->interface_template_map_for_main_world_.clear();
  data->operation_template_map_for_non_main_world_.clear();
  data->operation_template_map_for_main_world_.clear();
  if (IsMainThread())
    g_main_thread_per_isolate_data = nullptr;

  // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
  isolate->Exit();
  delete data;
}

V8PerIsolateData::V8FunctionTemplateMap&
V8PerIsolateData::SelectInterfaceTemplateMap(const DOMWrapperWorld& world) {
  return world.IsMainWorld() ? interface_template_map_for_main_world_
                             : interface_template_map_for_non_main_world_;
}

V8PerIsolateData::V8FunctionTemplateMap&
V8PerIsolateData::SelectOperationTemplateMap(const DOMWrapperWorld& world) {
  return world.IsMainWorld() ? operation_template_map_for_main_world_
                             : operation_template_map_for_non_main_world_;
}

v8::Local<v8::FunctionTemplate> V8PerIsolateData::FindOrCreateOperationTemplate(
    const DOMWrapperWorld& world,
    const void* key,
    v8::FunctionCallback callback,
    v8::Local<v8::Value> data,
    v8::Local<v8::Signature> signature,
    int length) {
  auto& map = SelectOperationTemplateMap(world);
  auto result = map.find(key);
  if (result != map.end())
    return result->value.Get(GetIsolate());

  v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(
      GetIsolate(), callback, data, signature, length);
  templ->RemovePrototype();
  map.insert(key, v8::Eternal<v8::FunctionTemplate>(GetIsolate(), templ));
  return templ;
}

v8::Local<v8::FunctionTemplate> V8PerIsolateData::FindInterfaceTemplate(
    const DOMWrapperWorld& world,
    const void* key) {
  if (GetV8ContextSnapshotMode() == V8ContextSnapshotMode::kTakeSnapshot) {
    const WrapperTypeInfo* type = reinterpret_cast<const WrapperTypeInfo*>(key);
    return interface_template_map_for_v8_context_snapshot_.Get(type);
  }

  auto& map = SelectInterfaceTemplateMap(world);
  auto result = map.find(key);
  if (result != map.end())
    return result->value.Get(GetIsolate());
  return v8::Local<v8::FunctionTemplate>();
}

void V8PerIsolateData::SetInterfaceTemplate(
    const DOMWrapperWorld& world,
    const void* key,
    v8::Local<v8::FunctionTemplate> value) {
  if (GetV8ContextSnapshotMode() == V8ContextSnapshotMode::kTakeSnapshot) {
    auto& map = interface_template_map_for_v8_context_snapshot_;
    const WrapperTypeInfo* type = reinterpret_cast<const WrapperTypeInfo*>(key);
    map.Set(type, value);
  } else {
    auto& map = SelectInterfaceTemplateMap(world);
    map.insert(key, v8::Eternal<v8::FunctionTemplate>(GetIsolate(), value));
  }
}

void V8PerIsolateData::ClearPersistentsForV8ContextSnapshot() {
  interface_template_map_for_v8_context_snapshot_.Clear();
  private_property_.reset();
}

const v8::Eternal<v8::Name>* V8PerIsolateData::FindOrCreateEternalNameCache(
    const void* lookup_key,
    const char* const names[],
    size_t count) {
  auto it = eternal_name_cache_.find(lookup_key);
  const Vector<v8::Eternal<v8::Name>>* vector = nullptr;
  if (UNLIKELY(it == eternal_name_cache_.end())) {
    v8::Isolate* isolate = this->GetIsolate();
    Vector<v8::Eternal<v8::Name>> new_vector(count);
    std::transform(
        names, names + count, new_vector.begin(), [isolate](const char* name) {
          return v8::Eternal<v8::Name>(isolate, V8AtomicString(isolate, name));
        });
    vector = &eternal_name_cache_.Set(lookup_key, std::move(new_vector))
                  .stored_value->value;
  } else {
    vector = &it->value;
  }
  DCHECK_EQ(vector->size(), count);
  return vector->data();
}

v8::Local<v8::Context> V8PerIsolateData::EnsureScriptRegexpContext() {
  if (!script_regexp_script_state_) {
    LEAK_SANITIZER_DISABLED_SCOPE;
    v8::Local<v8::Context> context(v8::Context::New(GetIsolate()));
    script_regexp_script_state_ = ScriptState::Create(
        context, DOMWrapperWorld::Create(GetIsolate(),
                                         DOMWrapperWorld::WorldType::kRegExp));
  }
  return script_regexp_script_state_->GetContext();
}

void V8PerIsolateData::ClearScriptRegexpContext() {
  if (script_regexp_script_state_)
    script_regexp_script_state_->DisposePerContextData();
  script_regexp_script_state_ = nullptr;
}

bool V8PerIsolateData::HasInstance(
    const WrapperTypeInfo* untrusted_wrapper_type_info,
    v8::Local<v8::Value> value) {
  RUNTIME_CALL_TIMER_SCOPE(GetIsolate(),
                           RuntimeCallStats::CounterId::kHasInstance);
  return HasInstance(untrusted_wrapper_type_info, value,
                     interface_template_map_for_main_world_) ||
         HasInstance(untrusted_wrapper_type_info, value,
                     interface_template_map_for_non_main_world_);
}

bool V8PerIsolateData::HasInstance(
    const WrapperTypeInfo* untrusted_wrapper_type_info,
    v8::Local<v8::Value> value,
    V8FunctionTemplateMap& map) {
  auto result = map.find(untrusted_wrapper_type_info);
  if (result == map.end())
    return false;
  v8::Local<v8::FunctionTemplate> templ = result->value.Get(GetIsolate());
  return templ->HasInstance(value);
}

v8::Local<v8::Object> V8PerIsolateData::FindInstanceInPrototypeChain(
    const WrapperTypeInfo* info,
    v8::Local<v8::Value> value) {
  v8::Local<v8::Object> wrapper = FindInstanceInPrototypeChain(
      info, value, interface_template_map_for_main_world_);
  if (!wrapper.IsEmpty())
    return wrapper;
  return FindInstanceInPrototypeChain(
      info, value, interface_template_map_for_non_main_world_);
}

v8::Local<v8::Object> V8PerIsolateData::FindInstanceInPrototypeChain(
    const WrapperTypeInfo* info,
    v8::Local<v8::Value> value,
    V8FunctionTemplateMap& map) {
  if (value.IsEmpty() || !value->IsObject())
    return v8::Local<v8::Object>();
  auto result = map.find(info);
  if (result == map.end())
    return v8::Local<v8::Object>();
  v8::Local<v8::FunctionTemplate> templ = result->value.Get(GetIsolate());
  return v8::Local<v8::Object>::Cast(value)->FindInstanceInPrototypeChain(
      templ);
}

void V8PerIsolateData::AddEndOfScopeTask(base::OnceClosure task) {
  end_of_scope_tasks_.push_back(std::move(task));
}

void V8PerIsolateData::RunEndOfScopeTasks() {
  Vector<base::OnceClosure> tasks;
  tasks.swap(end_of_scope_tasks_);
  for (auto& task : tasks)
    std::move(task).Run();
  DCHECK(end_of_scope_tasks_.IsEmpty());
}

void V8PerIsolateData::ClearEndOfScopeTasks() {
  end_of_scope_tasks_.clear();
}

void V8PerIsolateData::SetThreadDebugger(
    std::unique_ptr<V8PerIsolateData::Data> thread_debugger) {
  DCHECK(!thread_debugger_);
  thread_debugger_ = std::move(thread_debugger);
}

V8PerIsolateData::Data* V8PerIsolateData::ThreadDebugger() {
  return thread_debugger_.get();
}

void V8PerIsolateData::AddActiveScriptWrappable(
    ActiveScriptWrappableBase* wrappable) {
  if (!active_script_wrappables_)
    active_script_wrappables_ = new ActiveScriptWrappableSet();

  active_script_wrappables_->insert(wrappable);
}

void V8PerIsolateData::TemporaryScriptWrappableVisitorScope::
    SwapWithV8PerIsolateDataVisitor(
        std::unique_ptr<ScriptWrappableMarkingVisitor>& visitor) {
  ScriptWrappableMarkingVisitor* current = CurrentVisitor();
  if (current)
    ScriptWrappableMarkingVisitor::PerformCleanup(isolate_);

  V8PerIsolateData::From(isolate_)->script_wrappable_visitor_.swap(
      saved_visitor_);
  isolate_->SetEmbedderHeapTracer(CurrentVisitor());
}

}  // namespace blink
