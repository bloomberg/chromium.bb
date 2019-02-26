// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/conversions.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/frame-constants.h"
#include "src/heap/factory.h"
#include "src/message-template.h"
#include "src/objects-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/trap-handler/trap-handler.h"
#include "src/v8memory.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

namespace {

WasmInstanceObject* GetWasmInstanceOnStackTop(Isolate* isolate) {
  StackFrameIterator it(isolate, isolate->thread_local_top());
  // On top: C entry stub.
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Next: the wasm compiled frame.
  DCHECK(it.frame()->is_wasm_compiled());
  WasmCompiledFrame* frame = WasmCompiledFrame::cast(it.frame());
  return frame->wasm_instance();
}

Context GetNativeContextFromWasmInstanceOnStackTop(Isolate* isolate) {
  return GetWasmInstanceOnStackTop(isolate)->native_context();
}

class ClearThreadInWasmScope {
 public:
  ClearThreadInWasmScope() {
    DCHECK_EQ(trap_handler::IsTrapHandlerEnabled(),
              trap_handler::IsThreadInWasm());
    trap_handler::ClearThreadInWasm();
  }
  ~ClearThreadInWasmScope() {
    DCHECK(!trap_handler::IsThreadInWasm());
    trap_handler::SetThreadInWasm();
  }
};

}  // namespace

RUNTIME_FUNCTION(Runtime_WasmMemoryGrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  // {delta_pages} is checked to be a positive smi in the WasmMemoryGrow builtin
  // which calls this runtime function.
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 1);

  // This runtime function is always being called from wasm code.
  ClearThreadInWasmScope flag_scope;

  int ret = WasmMemoryObject::Grow(
      isolate, handle(instance->memory_object(), isolate), delta_pages);
  // The WasmMemoryGrow builtin which calls this runtime function expects us to
  // always return a Smi.
  return Smi::FromInt(ret);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmError) {
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  ClearThreadInWasmScope clear_wasm_flag;

  HandleScope scope(isolate);
  Handle<Object> error_obj = isolate->factory()->NewWasmRuntimeError(
      MessageTemplateFromInt(message_id));
  return isolate->Throw(*error_obj);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmStackOverflow) {
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  return isolate->StackOverflow();
}

RUNTIME_FUNCTION(Runtime_WasmThrowTypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapTypeError));
}

RUNTIME_FUNCTION(Runtime_WasmThrowCreate) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(HeapObject, tag_raw, 0);
  CONVERT_SMI_ARG_CHECKED(size, 1);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> tag(tag_raw, isolate);
  Handle<Object> exception = isolate->factory()->NewWasmRuntimeError(
      MessageTemplate::kWasmExceptionError);
  CHECK(
      !JSReceiver::SetProperty(isolate, exception,
                               isolate->factory()->wasm_exception_tag_symbol(),
                               tag, LanguageMode::kStrict)
           .is_null());
  Handle<FixedArray> values = isolate->factory()->NewFixedArray(size);
  CHECK(!JSReceiver::SetProperty(
             isolate, exception,
             isolate->factory()->wasm_exception_values_symbol(), values,
             LanguageMode::kStrict)
             .is_null());
  return *exception;
}

RUNTIME_FUNCTION(Runtime_WasmExceptionGetTag) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(Object, except_obj_raw, 0);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> except_obj(except_obj_raw, isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj), isolate);
    Handle<Object> tag;
    if (JSReceiver::GetProperty(isolate, exception,
                                isolate->factory()->wasm_exception_tag_symbol())
            .ToHandle(&tag)) {
      return *tag;
    }
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmExceptionGetValues) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(Object, except_obj_raw, 0);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> except_obj(except_obj_raw, isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj), isolate);
    Handle<Object> values;
    if (JSReceiver::GetProperty(
            isolate, exception,
            isolate->factory()->wasm_exception_values_symbol())
            .ToHandle(&values)) {
      DCHECK(values->IsFixedArray());
      return *values;
    }
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmRunInterpreter) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_NUMBER_CHECKED(int32_t, func_index, Int32, args[0]);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg_buffer_obj, 1);

  // The arg buffer is the raw pointer to the caller's stack. It looks like a
  // Smi (lowest bit not set, as checked by IsSmi), but is no valid Smi. We just
  // cast it back to the raw pointer.
  CHECK(!arg_buffer_obj->IsHeapObject());
  CHECK(arg_buffer_obj->IsSmi());
  Address arg_buffer = reinterpret_cast<Address>(*arg_buffer_obj);

  ClearThreadInWasmScope wasm_flag;

  // Find the frame pointer and instance of the interpreter frame on the stack.
  Handle<WasmInstanceObject> instance;
  Address frame_pointer = 0;
  {
    StackFrameIterator it(isolate, isolate->thread_local_top());
    // On top: C entry stub.
    DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
    it.Advance();
    // Next: the wasm interpreter entry.
    DCHECK_EQ(StackFrame::WASM_INTERPRETER_ENTRY, it.frame()->type());
    instance = handle(
        WasmInterpreterEntryFrame::cast(it.frame())->wasm_instance(), isolate);
    frame_pointer = it.frame()->fp();
  }

  // Set the current isolate's context.
  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());

  // Run the function in the interpreter. Note that neither the {WasmDebugInfo}
  // nor the {InterpreterHandle} have to exist, because interpretation might
  // have been triggered by another Isolate sharing the same WasmEngine.
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  bool success = WasmDebugInfo::RunInterpreter(
      isolate, debug_info, frame_pointer, func_index, arg_buffer);

  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmStackGuard) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  DCHECK(!trap_handler::IsTrapHandlerEnabled() ||
         trap_handler::IsThreadInWasm());

  ClearThreadInWasmScope wasm_flag;

  // Check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) return isolate->StackOverflow();

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_WasmCompileLazy) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_SMI_ARG_CHECKED(func_index, 1);

  ClearThreadInWasmScope wasm_flag;

#ifdef DEBUG
  StackFrameIterator it(isolate, isolate->thread_local_top());
  // On top: C entry stub.
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Next: the wasm lazy compile frame.
  DCHECK_EQ(StackFrame::WASM_COMPILE_LAZY, it.frame()->type());
  DCHECK_EQ(*instance, WasmCompileLazyFrame::cast(it.frame())->wasm_instance());
#endif

  Address entrypoint = wasm::CompileLazy(
      isolate, instance->module_object()->native_module(), func_index);
  return reinterpret_cast<Object*>(entrypoint);
}

// Should be called from within a handle scope
Handle<JSArrayBuffer> getSharedArrayBuffer(Handle<WasmInstanceObject> instance,
                                           Isolate* isolate, uint32_t address) {
  DCHECK(instance->has_memory_object());
  Handle<JSArrayBuffer> array_buffer(instance->memory_object()->array_buffer(),
                                     isolate);

  // Validation should have failed if the memory was not shared.
  DCHECK(array_buffer->is_shared());

  // Should have trapped if address was OOB
  DCHECK_LT(address, array_buffer->byte_length());
  return array_buffer;
}

RUNTIME_FUNCTION(Runtime_WasmAtomicWake) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, address, Uint32, args[1]);
  CONVERT_NUMBER_CHECKED(uint32_t, count, Uint32, args[2]);
  Handle<JSArrayBuffer> array_buffer =
      getSharedArrayBuffer(instance, isolate, address);
  return FutexEmulation::Wake(array_buffer, address, count);
}

RUNTIME_FUNCTION(Runtime_WasmI32AtomicWait) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, address, Uint32, args[1]);
  CONVERT_NUMBER_CHECKED(int32_t, expected_value, Int32, args[2]);
  CONVERT_DOUBLE_ARG_CHECKED(timeout, 3);
  timeout = timeout < 0 ? V8_INFINITY
                        : timeout / (base::Time::kNanosecondsPerMicrosecond *
                                     base::Time::kMicrosecondsPerMillisecond);
  Handle<JSArrayBuffer> array_buffer =
      getSharedArrayBuffer(instance, isolate, address);
  return FutexEmulation::Wait(isolate, array_buffer, address, expected_value,
                              timeout);
}

}  // namespace internal
}  // namespace v8
