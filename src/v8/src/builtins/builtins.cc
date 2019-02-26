// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"

#include "src/api-inl.h"
#include "src/assembler-inl.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/callable.h"
#include "src/code-tracer.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/objects/fixed-array.h"
#include "src/snapshot/embedded-data.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Address* args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

namespace {

// TODO(jgruber): Pack in CallDescriptors::Key.
struct BuiltinMetadata {
  const char* name;
  Builtins::Kind kind;
  union {
    Address cpp_entry;       // For CPP and API builtins.
    int8_t parameter_count;  // For TFJ builtins.
  } kind_specific_data;
};

// clang-format off
#define DECL_CPP(Name, ...) { #Name, Builtins::CPP, \
                              { FUNCTION_ADDR(Builtin_##Name) }},
#define DECL_API(Name, ...) { #Name, Builtins::API, \
                              { FUNCTION_ADDR(Builtin_##Name) }},
#ifdef V8_TARGET_BIG_ENDIAN
#define DECL_TFJ(Name, Count, ...) { #Name, Builtins::TFJ, \
  { static_cast<Address>(static_cast<uintptr_t>(           \
                              Count) << (kBitsPerByte * (kPointerSize - 1))) }},
#else
#define DECL_TFJ(Name, Count, ...) { #Name, Builtins::TFJ, \
                              { static_cast<Address>(Count) }},
#endif
#define DECL_TFC(Name, ...) { #Name, Builtins::TFC, {} },
#define DECL_TFS(Name, ...) { #Name, Builtins::TFS, {} },
#define DECL_TFH(Name, ...) { #Name, Builtins::TFH, {} },
#define DECL_BCH(Name, ...) { #Name, Builtins::BCH, {} },
#define DECL_ASM(Name, ...) { #Name, Builtins::ASM, {} },
const BuiltinMetadata builtin_metadata[] = {
  BUILTIN_LIST(DECL_CPP, DECL_API, DECL_TFJ, DECL_TFC, DECL_TFS, DECL_TFH,
               DECL_BCH, DECL_ASM)
};
#undef DECL_CPP
#undef DECL_API
#undef DECL_TFJ
#undef DECL_TFC
#undef DECL_TFS
#undef DECL_TFH
#undef DECL_BCH
#undef DECL_ASM
// clang-format on

}  // namespace

BailoutId Builtins::GetContinuationBailoutId(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ || Builtins::KindOf(name) == TFC);
  return BailoutId(BailoutId::kFirstBuiltinContinuationId + name);
}

Builtins::Name Builtins::GetBuiltinFromBailoutId(BailoutId id) {
  int builtin_index = id.ToInt() - BailoutId::kFirstBuiltinContinuationId;
  DCHECK(Builtins::KindOf(builtin_index) == TFJ ||
         Builtins::KindOf(builtin_index) == TFC);
  return static_cast<Name>(builtin_index);
}

void Builtins::TearDown() { initialized_ = false; }

const char* Builtins::Lookup(Address pc) {
  // Off-heap pc's can be looked up through binary search.
  if (FLAG_embedded_builtins) {
    Code maybe_builtin = InstructionStream::TryLookupCode(isolate_, pc);
    if (!maybe_builtin.is_null()) return name(maybe_builtin->builtin_index());
  }

  // May be called during initialization (disassembler).
  if (initialized_) {
    for (int i = 0; i < builtin_count; i++) {
      if (isolate_->heap()->builtin(i)->contains(pc)) return name(i);
    }
  }
  return nullptr;
}

Handle<Code> Builtins::NewFunctionContext(ScopeType scope_type) {
  switch (scope_type) {
    case ScopeType::EVAL_SCOPE:
      return builtin_handle(kFastNewFunctionContextEval);
    case ScopeType::FUNCTION_SCOPE:
      return builtin_handle(kFastNewFunctionContextFunction);
    default:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

Handle<Code> Builtins::NonPrimitiveToPrimitive(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return builtin_handle(kNonPrimitiveToPrimitive_Default);
    case ToPrimitiveHint::kNumber:
      return builtin_handle(kNonPrimitiveToPrimitive_Number);
    case ToPrimitiveHint::kString:
      return builtin_handle(kNonPrimitiveToPrimitive_String);
  }
  UNREACHABLE();
}

Handle<Code> Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint) {
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      return builtin_handle(kOrdinaryToPrimitive_Number);
    case OrdinaryToPrimitiveHint::kString:
      return builtin_handle(kOrdinaryToPrimitive_String);
  }
  UNREACHABLE();
}

void Builtins::set_builtin(int index, Code builtin) {
  isolate_->heap()->set_builtin(index, builtin);
}

Code Builtins::builtin(int index) { return isolate_->heap()->builtin(index); }

Handle<Code> Builtins::builtin_handle(int index) {
  DCHECK(IsBuiltinId(index));
  return Handle<Code>(
      reinterpret_cast<Address*>(isolate_->heap()->builtin_address(index)));
}

// static
int Builtins::GetStackParameterCount(Name name) {
  DCHECK(Builtins::KindOf(name) == TFJ);
  return builtin_metadata[name].kind_specific_data.parameter_count;
}

// static
Callable Builtins::CallableFor(Isolate* isolate, Name name) {
  Handle<Code> code = isolate->builtins()->builtin_handle(name);
  CallDescriptors::Key key;
  switch (name) {
// This macro is deliberately crafted so as to emit very little code,
// in order to keep binary size of this function under control.
#define CASE_OTHER(Name, ...)                          \
  case k##Name: {                                      \
    key = Builtin_##Name##_InterfaceDescriptor::key(); \
    break;                                             \
  }
    BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, CASE_OTHER,
                 CASE_OTHER, CASE_OTHER, IGNORE_BUILTIN, IGNORE_BUILTIN)
#undef CASE_OTHER
    default:
      Builtins::Kind kind = Builtins::KindOf(name);
      DCHECK_NE(BCH, kind);
      if (kind == TFJ || kind == CPP) {
        return Callable(code, JSTrampolineDescriptor{});
      }
      UNREACHABLE();
  }
  CallInterfaceDescriptor descriptor(key);
  return Callable(code, descriptor);
}

// static
const char* Builtins::name(int index) {
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].name;
}

void Builtins::PrintBuiltinCode() {
  DCHECK(FLAG_print_builtin_code);
#ifdef ENABLE_DISASSEMBLER
  for (int i = 0; i < builtin_count; i++) {
    const char* builtin_name = name(i);
    Handle<Code> code = builtin_handle(i);
    if (PassesFilter(CStrVector(builtin_name),
                     CStrVector(FLAG_print_builtin_code_filter))) {
      CodeTracer::Scope trace_scope(isolate_->GetCodeTracer());
      OFStream os(trace_scope.file());
      code->Disassemble(builtin_name, os);
      os << "\n";
    }
  }
#endif
}

void Builtins::PrintBuiltinSize() {
  DCHECK(FLAG_print_builtin_size);
  for (int i = 0; i < builtin_count; i++) {
    const char* builtin_name = name(i);
    const char* kind = KindNameOf(i);
    Code code = builtin(i);
    PrintF(stdout, "%s Builtin, %s, %d\n", kind, builtin_name,
           code->InstructionSize());
  }
}

// static
Address Builtins::CppEntryOf(int index) {
  DCHECK(Builtins::HasCppImplementation(index));
  return builtin_metadata[index].kind_specific_data.cpp_entry;
}

// static
bool Builtins::IsBuiltin(const Code code) {
  return Builtins::IsBuiltinId(code->builtin_index());
}

bool Builtins::IsBuiltinHandle(Handle<HeapObject> maybe_code,
                               int* index) const {
  Heap* heap = isolate_->heap();
  Address handle_location = maybe_code.address();
  Address start = heap->builtin_address(0);
  Address end = heap->builtin_address(Builtins::builtin_count);
  if (handle_location >= end) return false;
  if (handle_location < start) return false;
  *index = static_cast<int>(handle_location - start) >> kPointerSizeLog2;
  DCHECK(Builtins::IsBuiltinId(*index));
  return true;
}

// static
bool Builtins::IsIsolateIndependentBuiltin(const Code code) {
  if (FLAG_embedded_builtins) {
    const int builtin_index = code->builtin_index();
    return Builtins::IsBuiltinId(builtin_index) &&
           Builtins::IsIsolateIndependent(builtin_index);
  } else {
    return false;
  }
}

// static
bool Builtins::IsWasmRuntimeStub(int index) {
  DCHECK(IsBuiltinId(index));
  switch (index) {
#define CASE_TRAP(Name) case kThrowWasm##Name:
#define CASE(Name) case k##Name:
    WASM_RUNTIME_STUB_LIST(CASE, CASE_TRAP)
#undef CASE_TRAP
#undef CASE
    return true;
    default:
      return false;
  }
  UNREACHABLE();
}

namespace {

class OffHeapTrampolineGenerator {
 public:
  explicit OffHeapTrampolineGenerator(Isolate* isolate)
      : isolate_(isolate),
        masm_(isolate, buffer, kBufferSize, CodeObjectRequired::kYes) {}

  CodeDesc Generate(Address off_heap_entry) {
    // Generate replacement code that simply tail-calls the off-heap code.
    DCHECK(!masm_.has_frame());
    {
      FrameScope scope(&masm_, StackFrame::NONE);
      masm_.JumpToInstructionStream(off_heap_entry);
    }

    CodeDesc desc;
    masm_.GetCode(isolate_, &desc);
    return desc;
  }

  Handle<HeapObject> CodeObject() { return masm_.CodeObject(); }

 private:
  Isolate* isolate_;
  // Enough to fit the single jmp.
  static constexpr size_t kBufferSize = 256;
  byte buffer[kBufferSize];
  MacroAssembler masm_;
};

}  // namespace

// static
Handle<Code> Builtins::GenerateOffHeapTrampolineFor(Isolate* isolate,
                                                    Address off_heap_entry) {
  DCHECK_NOT_NULL(isolate->embedded_blob());
  DCHECK_NE(0, isolate->embedded_blob_size());

  OffHeapTrampolineGenerator generator(isolate);
  CodeDesc desc = generator.Generate(off_heap_entry);

  return isolate->factory()->NewCode(desc, Code::BUILTIN,
                                     generator.CodeObject());
}

// static
Handle<ByteArray> Builtins::GenerateOffHeapTrampolineRelocInfo(
    Isolate* isolate) {
  OffHeapTrampolineGenerator generator(isolate);
  // Generate a jump to a dummy address as we're not actually interested in the
  // generated instruction stream.
  CodeDesc desc = generator.Generate(kNullAddress);

  Handle<ByteArray> reloc_info =
      isolate->factory()->NewByteArray(desc.reloc_size, TENURED_READ_ONLY);
  Code::CopyRelocInfoToByteArray(*reloc_info, desc);

  return reloc_info;
}

// static
Builtins::Kind Builtins::KindOf(int index) {
  DCHECK(IsBuiltinId(index));
  return builtin_metadata[index].kind;
}

// static
const char* Builtins::KindNameOf(int index) {
  Kind kind = Builtins::KindOf(index);
  // clang-format off
  switch (kind) {
    case CPP: return "CPP";
    case API: return "API";
    case TFJ: return "TFJ";
    case TFC: return "TFC";
    case TFS: return "TFS";
    case TFH: return "TFH";
    case BCH: return "BCH";
    case ASM: return "ASM";
  }
  // clang-format on
  UNREACHABLE();
}

// static
bool Builtins::IsCpp(int index) { return Builtins::KindOf(index) == CPP; }

// static
bool Builtins::HasCppImplementation(int index) {
  Kind kind = Builtins::KindOf(index);
  return (kind == CPP || kind == API);
}

// static
bool Builtins::AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                    Handle<JSObject> target_global_proxy) {
  if (FLAG_allow_unsafe_function_constructor) return true;
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  Handle<Context> responsible_context = impl->LastEnteredOrMicrotaskContext();
  // TODO(jochen): Remove this.
  if (responsible_context.is_null()) {
    return true;
  }
  if (*responsible_context == target->context()) return true;
  return isolate->MayAccess(responsible_context, target_global_proxy);
}

Builtins::Name ExampleBuiltinForTorqueFunctionPointerType(
    size_t function_pointer_type_id) {
  switch (function_pointer_type_id) {
#define FUNCTION_POINTER_ID_CASE(id, name) \
  case id:                                 \
    return Builtins::k##name;
    TORQUE_FUNCTION_POINTER_TYPE_TO_BUILTIN_MAP(FUNCTION_POINTER_ID_CASE)
#undef FUNCTION_POINTER_ID_CASE
    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
