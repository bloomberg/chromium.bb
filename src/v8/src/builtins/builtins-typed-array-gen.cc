// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-typed-array-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/builtins/growable-fixed-array-gen.h"
#include "src/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "torque-generated/builtins-typed-array-createtypedarray-from-dsl-gen.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;

// This is needed for gc_mole which will compile this file without the full set
// of GN defined macros.
#ifndef V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP
#define V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP 64
#endif

// -----------------------------------------------------------------------------
// ES6 section 22.2 TypedArray Objects

TNode<Map> TypedArrayBuiltinsAssembler::LoadMapForType(
    TNode<JSTypedArray> array) {
  TVARIABLE(Map, var_typed_map);
  TNode<Map> array_map = LoadMap(array);
  TNode<Int32T> elements_kind = LoadMapElementsKind(array_map);
  ReadOnlyRoots roots(isolate());

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind kind, int size, int typed_array_fun_index) {
        Handle<Map> map(roots.MapForFixedTypedArray(kind), isolate());
        var_typed_map = HeapConstant(map);
      });

  return var_typed_map.value();
}

// Setup the TypedArray which is under construction.
//  - Set the length.
//  - Set the byte_offset.
//  - Set the byte_length.
//  - Set EmbedderFields to 0.
void TypedArrayBuiltinsAssembler::SetupTypedArray(TNode<JSTypedArray> holder,
                                                  TNode<Smi> length,
                                                  TNode<UintPtrT> byte_offset,
                                                  TNode<UintPtrT> byte_length) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(length));
  StoreObjectField(holder, JSTypedArray::kLengthOffset, length);
  StoreObjectFieldNoWriteBarrier(holder, JSArrayBufferView::kByteOffsetOffset,
                                 byte_offset,
                                 MachineType::PointerRepresentation());
  StoreObjectFieldNoWriteBarrier(holder, JSArrayBufferView::kByteLengthOffset,
                                 byte_length,
                                 MachineType::PointerRepresentation());
  for (int offset = JSTypedArray::kHeaderSize;
       offset < JSTypedArray::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectField(holder, offset, SmiConstant(0));
  }
}

// Allocate a new ArrayBuffer and initialize it with empty properties and
// elements.
TNode<JSArrayBuffer> TypedArrayBuiltinsAssembler::AllocateEmptyOnHeapBuffer(
    TNode<Context> context, TNode<JSTypedArray> holder,
    TNode<UintPtrT> byte_length) {
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<Map> map =
      CAST(LoadContextElement(native_context, Context::ARRAY_BUFFER_MAP_INDEX));
  TNode<FixedArray> empty_fixed_array =
      CAST(LoadRoot(RootIndex::kEmptyFixedArray));

  TNode<JSArrayBuffer> buffer = UncheckedCast<JSArrayBuffer>(
      Allocate(JSArrayBuffer::kSizeWithEmbedderFields));
  StoreMapNoWriteBarrier(buffer, map);
  StoreObjectFieldNoWriteBarrier(buffer, JSArray::kPropertiesOrHashOffset,
                                 empty_fixed_array);
  StoreObjectFieldNoWriteBarrier(buffer, JSArray::kElementsOffset,
                                 empty_fixed_array);
  // Setup the ArrayBuffer.
  //  - Set BitField to 0.
  //  - Set IsExternal and IsDetachable bits of BitFieldSlot.
  //  - Set the byte_length field to byte_length.
  //  - Set backing_store to null/Smi(0).
  //  - Set all embedder fields to Smi(0).
  if (FIELD_SIZE(JSArrayBuffer::kOptionalPaddingOffset) != 0) {
    DCHECK_EQ(4, FIELD_SIZE(JSArrayBuffer::kOptionalPaddingOffset));
    StoreObjectFieldNoWriteBarrier(
        buffer, JSArrayBuffer::kOptionalPaddingOffset, Int32Constant(0),
        MachineRepresentation::kWord32);
  }
  int32_t bitfield_value = (1 << JSArrayBuffer::IsExternalBit::kShift) |
                           (1 << JSArrayBuffer::IsDetachableBit::kShift);
  StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBitFieldOffset,
                                 Int32Constant(bitfield_value),
                                 MachineRepresentation::kWord32);

  StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kByteLengthOffset,
                                 byte_length,
                                 MachineType::PointerRepresentation());
  StoreObjectFieldNoWriteBarrier(buffer, JSArrayBuffer::kBackingStoreOffset,
                                 IntPtrConstant(0),
                                 MachineType::PointerRepresentation());
  for (int offset = JSArrayBuffer::kHeaderSize;
       offset < JSArrayBuffer::kSizeWithEmbedderFields; offset += kTaggedSize) {
    StoreObjectFieldNoWriteBarrier(buffer, offset, SmiConstant(0));
  }

  StoreObjectField(holder, JSArrayBufferView::kBufferOffset, buffer);
  return buffer;
}

TNode<FixedTypedArrayBase> TypedArrayBuiltinsAssembler::AllocateOnHeapElements(
    TNode<Map> map, TNode<IntPtrT> total_size, TNode<Number> length) {
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(total_size, IntPtrConstant(0)));

  // Allocate a FixedTypedArray and set the length, base pointer and external
  // pointer.
  CSA_ASSERT(this, IsRegularHeapObjectSize(total_size));

  TNode<Object> elements;

  if (UnalignedLoadSupported(MachineRepresentation::kFloat64) &&
      UnalignedStoreSupported(MachineRepresentation::kFloat64)) {
    elements = AllocateInNewSpace(total_size);
  } else {
    elements = AllocateInNewSpace(total_size, kDoubleAlignment);
  }

  StoreMapNoWriteBarrier(elements, map);
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
  StoreObjectFieldNoWriteBarrier(
      elements, FixedTypedArrayBase::kBasePointerOffset, elements);
  StoreObjectFieldNoWriteBarrier(
      elements, FixedTypedArrayBase::kExternalPointerOffset,
      IntPtrConstant(FixedTypedArrayBase::ExternalPointerValueForOnHeapArray()),
      MachineType::PointerRepresentation());
  return CAST(elements);
}

TNode<RawPtrT> TypedArrayBuiltinsAssembler::LoadDataPtr(
    TNode<JSTypedArray> typed_array) {
  TNode<FixedArrayBase> elements = LoadElements(typed_array);
  CSA_ASSERT(this, IsFixedTypedArray(elements));
  return LoadFixedTypedArrayBackingStore(CAST(elements));
}

TF_BUILTIN(TypedArrayBaseConstructor, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  ThrowTypeError(context, MessageTemplate::kConstructAbstractClass,
                 "TypedArray");
}

// ES #sec-typedarray-constructors
TF_BUILTIN(TypedArrayConstructor, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kJSTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
  Node* argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);
  Node* arg1 = args.GetOptionalArgumentValue(0);
  Node* arg2 = args.GetOptionalArgumentValue(1);
  Node* arg3 = args.GetOptionalArgumentValue(2);

  // If NewTarget is undefined, throw a TypeError exception.
  // All the TypedArray constructors have this as the first step:
  // https://tc39.github.io/ecma262/#sec-typedarray-constructors
  Label throwtypeerror(this, Label::kDeferred);
  GotoIf(IsUndefined(new_target), &throwtypeerror);

  Node* result = CallBuiltin(Builtins::kCreateTypedArray, context, target,
                             new_target, arg1, arg2, arg3);
  args.PopAndReturn(result);

  BIND(&throwtypeerror);
  {
    TNode<String> name =
        CAST(CallRuntime(Runtime::kGetFunctionName, context, target));
    ThrowTypeError(context, MessageTemplate::kConstructorNotFunction, name);
  }
}

// ES6 #sec-get-%typedarray%.prototype.bytelength
TF_BUILTIN(TypedArrayPrototypeByteLength, TypedArrayBuiltinsAssembler) {
  const char* const kMethodName = "get TypedArray.prototype.byteLength";
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  // Check if the {receiver} is actually a JSTypedArray.
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, kMethodName);

  // Default to zero if the {receiver}s buffer was detached.
  TNode<JSArrayBuffer> receiver_buffer =
      LoadJSArrayBufferViewBuffer(CAST(receiver));
  TNode<UintPtrT> byte_length = Select<UintPtrT>(
      IsDetachedBuffer(receiver_buffer), [=] { return UintPtrConstant(0); },
      [=] { return LoadJSArrayBufferViewByteLength(CAST(receiver)); });
  Return(ChangeUintPtrToTagged(byte_length));
}

// ES6 #sec-get-%typedarray%.prototype.byteoffset
TF_BUILTIN(TypedArrayPrototypeByteOffset, TypedArrayBuiltinsAssembler) {
  const char* const kMethodName = "get TypedArray.prototype.byteOffset";
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  // Check if the {receiver} is actually a JSTypedArray.
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, kMethodName);

  // Default to zero if the {receiver}s buffer was detached.
  TNode<JSArrayBuffer> receiver_buffer =
      LoadJSArrayBufferViewBuffer(CAST(receiver));
  TNode<UintPtrT> byte_offset = Select<UintPtrT>(
      IsDetachedBuffer(receiver_buffer), [=] { return UintPtrConstant(0); },
      [=] { return LoadJSArrayBufferViewByteOffset(CAST(receiver)); });
  Return(ChangeUintPtrToTagged(byte_offset));
}

// ES6 #sec-get-%typedarray%.prototype.length
TF_BUILTIN(TypedArrayPrototypeLength, TypedArrayBuiltinsAssembler) {
  const char* const kMethodName = "get TypedArray.prototype.length";
  Node* context = Parameter(Descriptor::kContext);
  Node* receiver = Parameter(Descriptor::kReceiver);

  // Check if the {receiver} is actually a JSTypedArray.
  ThrowIfNotInstanceType(context, receiver, JS_TYPED_ARRAY_TYPE, kMethodName);

  // Default to zero if the {receiver}s buffer was detached.
  TNode<JSArrayBuffer> receiver_buffer =
      LoadJSArrayBufferViewBuffer(CAST(receiver));
  TNode<Smi> length = Select<Smi>(
      IsDetachedBuffer(receiver_buffer), [=] { return SmiConstant(0); },
      [=] { return LoadJSTypedArrayLength(CAST(receiver)); });
  Return(length);
}

TNode<Word32T> TypedArrayBuiltinsAssembler::IsUint8ElementsKind(
    TNode<Word32T> kind) {
  return Word32Or(Word32Equal(kind, Int32Constant(UINT8_ELEMENTS)),
                  Word32Equal(kind, Int32Constant(UINT8_CLAMPED_ELEMENTS)));
}

TNode<Word32T> TypedArrayBuiltinsAssembler::IsBigInt64ElementsKind(
    TNode<Word32T> kind) {
  return Word32Or(Word32Equal(kind, Int32Constant(BIGINT64_ELEMENTS)),
                  Word32Equal(kind, Int32Constant(BIGUINT64_ELEMENTS)));
}

TNode<IntPtrT> TypedArrayBuiltinsAssembler::GetTypedArrayElementSize(
    TNode<Word32T> elements_kind) {
  TVARIABLE(IntPtrT, element_size);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind el_kind, int size, int typed_array_fun_index) {
        element_size = IntPtrConstant(size);
      });

  return element_size.value();
}

TypedArrayBuiltinsFromDSLAssembler::TypedArrayElementsInfo
TypedArrayBuiltinsAssembler::GetTypedArrayElementsInfo(
    TNode<JSTypedArray> typed_array) {
  TNode<Int32T> elements_kind = LoadElementsKind(typed_array);
  TVARIABLE(UintPtrT, var_size_log2);
  TVARIABLE(Map, var_map);
  ReadOnlyRoots roots(isolate());

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind kind, int size, int typed_array_fun_index) {
        DCHECK_GT(size, 0);
        var_size_log2 = UintPtrConstant(ElementsKindToShiftSize(kind));

        Handle<Map> map(roots.MapForFixedTypedArray(kind), isolate());
        var_map = HeapConstant(map);
      });

  return TypedArrayBuiltinsFromDSLAssembler::TypedArrayElementsInfo{
      var_size_log2.value(), var_map.value(), elements_kind};
}

TNode<JSFunction> TypedArrayBuiltinsAssembler::GetDefaultConstructor(
    TNode<Context> context, TNode<JSTypedArray> exemplar) {
  TVARIABLE(IntPtrT, context_slot);
  TNode<Word32T> elements_kind = LoadElementsKind(exemplar);

  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind el_kind, int size, int typed_array_function_index) {
        context_slot = IntPtrConstant(typed_array_function_index);
      });

  return CAST(
      LoadContextElement(LoadNativeContext(context), context_slot.value()));
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::TypedArrayCreateByLength(
    TNode<Context> context, TNode<Object> constructor, TNode<Smi> len,
    const char* method_name) {
  CSA_ASSERT(this, TaggedIsPositiveSmi(len));

  // Let newTypedArray be ? Construct(constructor, argumentList).
  TNode<Object> new_object = CAST(ConstructJS(CodeFactory::Construct(isolate()),
                                              context, constructor, len));

  // Perform ? ValidateTypedArray(newTypedArray).
  TNode<JSTypedArray> new_typed_array =
      ValidateTypedArray(context, new_object, method_name);

  ThrowIfLengthLessThan(context, new_typed_array, len);
  return new_typed_array;
}

void TypedArrayBuiltinsAssembler::ThrowIfLengthLessThan(
    TNode<Context> context, TNode<JSTypedArray> typed_array,
    TNode<Smi> min_length) {
  // If typed_array.[[ArrayLength]] < min_length, throw a TypeError exception.
  Label if_length_is_not_short(this);
  TNode<Smi> new_length = LoadJSTypedArrayLength(typed_array);
  GotoIfNot(SmiLessThan(new_length, min_length), &if_length_is_not_short);
  ThrowTypeError(context, MessageTemplate::kTypedArrayTooShort);

  BIND(&if_length_is_not_short);
}

TNode<JSArrayBuffer> TypedArrayBuiltinsAssembler::GetBuffer(
    TNode<Context> context, TNode<JSTypedArray> array) {
  Label call_runtime(this), done(this);
  TVARIABLE(Object, var_result);

  TNode<Object> buffer = LoadObjectField(array, JSTypedArray::kBufferOffset);
  GotoIf(IsDetachedBuffer(buffer), &call_runtime);
  TNode<UintPtrT> backing_store = LoadObjectField<UintPtrT>(
      CAST(buffer), JSArrayBuffer::kBackingStoreOffset);
  GotoIf(WordEqual(backing_store, IntPtrConstant(0)), &call_runtime);
  var_result = buffer;
  Goto(&done);

  BIND(&call_runtime);
  {
    var_result = CallRuntime(Runtime::kTypedArrayGetBuffer, context, array);
    Goto(&done);
  }

  BIND(&done);
  return CAST(var_result.value());
}

TNode<JSTypedArray> TypedArrayBuiltinsAssembler::ValidateTypedArray(
    TNode<Context> context, TNode<Object> obj, const char* method_name) {
  // If it is not a typed array, throw
  ThrowIfNotInstanceType(context, obj, JS_TYPED_ARRAY_TYPE, method_name);

  // If the typed array's buffer is detached, throw
  ThrowIfArrayBufferViewBufferIsDetached(context, CAST(obj), method_name);

  return CAST(obj);
}

void TypedArrayBuiltinsAssembler::SetTypedArraySource(
    TNode<Context> context, TNode<JSTypedArray> source,
    TNode<JSTypedArray> target, TNode<IntPtrT> offset, Label* call_runtime,
    Label* if_source_too_large) {
  CSA_ASSERT(this, Word32BinaryNot(IsDetachedBuffer(
                       LoadObjectField(source, JSTypedArray::kBufferOffset))));
  CSA_ASSERT(this, Word32BinaryNot(IsDetachedBuffer(
                       LoadObjectField(target, JSTypedArray::kBufferOffset))));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(offset, IntPtrConstant(0)));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(offset, IntPtrConstant(Smi::kMaxValue)));

  // Check for possible range errors.

  TNode<IntPtrT> source_length = SmiUntag(LoadJSTypedArrayLength(source));
  TNode<IntPtrT> target_length = SmiUntag(LoadJSTypedArrayLength(target));
  TNode<IntPtrT> required_target_length = IntPtrAdd(source_length, offset);

  GotoIf(IntPtrGreaterThan(required_target_length, target_length),
         if_source_too_large);

  // Grab pointers and byte lengths we need later on.

  TNode<RawPtrT> target_data_ptr = LoadDataPtr(target);
  TNode<RawPtrT> source_data_ptr = LoadDataPtr(source);

  TNode<Word32T> source_el_kind = LoadElementsKind(source);
  TNode<Word32T> target_el_kind = LoadElementsKind(target);

  TNode<IntPtrT> source_el_size = GetTypedArrayElementSize(source_el_kind);
  TNode<IntPtrT> target_el_size = GetTypedArrayElementSize(target_el_kind);

  // A note on byte lengths: both source- and target byte lengths must be valid,
  // i.e. it must be possible to allocate an array of the given length. That
  // means we're safe from overflows in the following multiplication.
  TNode<IntPtrT> source_byte_length = IntPtrMul(source_length, source_el_size);
  CSA_ASSERT(this,
             UintPtrGreaterThanOrEqual(source_byte_length, IntPtrConstant(0)));

  Label call_memmove(this), fast_c_call(this), out(this), exception(this);

  // A fast memmove call can be used when the source and target types are are
  // the same or either Uint8 or Uint8Clamped.
  GotoIf(Word32Equal(source_el_kind, target_el_kind), &call_memmove);
  GotoIfNot(IsUint8ElementsKind(source_el_kind), &fast_c_call);
  Branch(IsUint8ElementsKind(target_el_kind), &call_memmove, &fast_c_call);

  BIND(&call_memmove);
  {
    TNode<RawPtrT> target_start =
        RawPtrAdd(target_data_ptr, IntPtrMul(offset, target_el_size));
    CallCMemmove(target_start, source_data_ptr, Unsigned(source_byte_length));
    Goto(&out);
  }

  BIND(&fast_c_call);
  {
    CSA_ASSERT(
        this, UintPtrGreaterThanOrEqual(
                  IntPtrMul(target_length, target_el_size), IntPtrConstant(0)));

    GotoIf(Word32NotEqual(IsBigInt64ElementsKind(source_el_kind),
                          IsBigInt64ElementsKind(target_el_kind)),
           &exception);

    TNode<IntPtrT> source_length = SmiUntag(LoadJSTypedArrayLength(source));
    CallCCopyTypedArrayElementsToTypedArray(source, target, source_length,
                                            offset);
    Goto(&out);
  }

  BIND(&exception);
  ThrowTypeError(context, MessageTemplate::kBigIntMixedTypes);

  BIND(&out);
}

void TypedArrayBuiltinsAssembler::SetJSArraySource(
    TNode<Context> context, TNode<JSArray> source, TNode<JSTypedArray> target,
    TNode<IntPtrT> offset, Label* call_runtime, Label* if_source_too_large) {
  CSA_ASSERT(this, IsFastJSArray(source, context));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(offset, IntPtrConstant(0)));
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(offset, IntPtrConstant(Smi::kMaxValue)));

  TNode<IntPtrT> source_length = SmiUntag(LoadFastJSArrayLength(source));
  TNode<IntPtrT> target_length = SmiUntag(LoadJSTypedArrayLength(target));

  // Maybe out of bounds?
  GotoIf(IntPtrGreaterThan(IntPtrAdd(source_length, offset), target_length),
         if_source_too_large);

  // Nothing to do if {source} is empty.
  Label out(this), fast_c_call(this);
  GotoIf(IntPtrEqual(source_length, IntPtrConstant(0)), &out);

  // Dispatch based on the source elements kind.
  {
    // These are the supported elements kinds in TryCopyElementsFastNumber.
    int32_t values[] = {
        PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS, PACKED_DOUBLE_ELEMENTS,
        HOLEY_DOUBLE_ELEMENTS,
    };
    Label* labels[] = {
        &fast_c_call, &fast_c_call, &fast_c_call, &fast_c_call,
    };
    STATIC_ASSERT(arraysize(values) == arraysize(labels));

    TNode<Int32T> source_elements_kind = LoadElementsKind(source);
    Switch(source_elements_kind, call_runtime, values, labels,
           arraysize(values));
  }

  BIND(&fast_c_call);
  GotoIf(IsBigInt64ElementsKind(LoadElementsKind(target)), call_runtime);
  CallCCopyFastNumberJSArrayElementsToTypedArray(context, source, target,
                                                 source_length, offset);
  Goto(&out);
  BIND(&out);
}

void TypedArrayBuiltinsAssembler::CallCMemmove(TNode<RawPtrT> dest_ptr,
                                               TNode<RawPtrT> src_ptr,
                                               TNode<UintPtrT> byte_length) {
  TNode<ExternalReference> memmove =
      ExternalConstant(ExternalReference::libc_memmove_function());
  CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                 MachineType::Pointer(), MachineType::UintPtr(), memmove,
                 dest_ptr, src_ptr, byte_length);
}

void TypedArrayBuiltinsAssembler::CallCMemcpy(TNode<RawPtrT> dest_ptr,
                                              TNode<RawPtrT> src_ptr,
                                              TNode<UintPtrT> byte_length) {
  TNode<ExternalReference> memcpy =
      ExternalConstant(ExternalReference::libc_memcpy_function());
  CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                 MachineType::Pointer(), MachineType::UintPtr(), memcpy,
                 dest_ptr, src_ptr, byte_length);
}

void TypedArrayBuiltinsAssembler::CallCMemset(TNode<RawPtrT> dest_ptr,
                                              TNode<IntPtrT> value,
                                              TNode<UintPtrT> length) {
  TNode<ExternalReference> memset =
      ExternalConstant(ExternalReference::libc_memset_function());
  CallCFunction3(MachineType::AnyTagged(), MachineType::Pointer(),
                 MachineType::IntPtr(), MachineType::UintPtr(), memset,
                 dest_ptr, value, length);
}

void TypedArrayBuiltinsAssembler::
    CallCCopyFastNumberJSArrayElementsToTypedArray(TNode<Context> context,
                                                   TNode<JSArray> source,
                                                   TNode<JSTypedArray> dest,
                                                   TNode<IntPtrT> source_length,
                                                   TNode<IntPtrT> offset) {
  CSA_ASSERT(this,
             Word32BinaryNot(IsBigInt64ElementsKind(LoadElementsKind(dest))));
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_fast_number_jsarray_elements_to_typed_array());
  CallCFunction5(MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::UintPtr(), MachineType::UintPtr(), f, context,
                 source, dest, source_length, offset);
}

void TypedArrayBuiltinsAssembler::CallCCopyTypedArrayElementsToTypedArray(
    TNode<JSTypedArray> source, TNode<JSTypedArray> dest,
    TNode<IntPtrT> source_length, TNode<IntPtrT> offset) {
  TNode<ExternalReference> f = ExternalConstant(
      ExternalReference::copy_typed_array_elements_to_typed_array());
  CallCFunction4(MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::AnyTagged(), MachineType::UintPtr(),
                 MachineType::UintPtr(), f, source, dest, source_length,
                 offset);
}

void TypedArrayBuiltinsAssembler::CallCCopyTypedArrayElementsSlice(
    TNode<JSTypedArray> source, TNode<JSTypedArray> dest, TNode<IntPtrT> start,
    TNode<IntPtrT> end) {
  TNode<ExternalReference> f =
      ExternalConstant(ExternalReference::copy_typed_array_elements_slice());
  CallCFunction4(MachineType::AnyTagged(), MachineType::AnyTagged(),
                 MachineType::AnyTagged(), MachineType::UintPtr(),
                 MachineType::UintPtr(), f, source, dest, start, end);
}

void TypedArrayBuiltinsAssembler::DispatchTypedArrayByElementsKind(
    TNode<Word32T> elements_kind, const TypedArraySwitchCase& case_function) {
  Label next(this), if_unknown_type(this, Label::kDeferred);

  int32_t elements_kinds[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) TYPE##_ELEMENTS,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) Label if_##type##array(this);
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  Label* elements_kind_labels[] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) &if_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  STATIC_ASSERT(arraysize(elements_kinds) == arraysize(elements_kind_labels));

  Switch(elements_kind, &if_unknown_type, elements_kinds, elements_kind_labels,
         arraysize(elements_kinds));

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype)   \
  BIND(&if_##type##array);                          \
  {                                                 \
    case_function(TYPE##_ELEMENTS, sizeof(ctype),   \
                  Context::TYPE##_ARRAY_FUN_INDEX); \
    Goto(&next);                                    \
  }
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  BIND(&if_unknown_type);
  Unreachable();

  BIND(&next);
}

TNode<BoolT> TypedArrayBuiltinsAssembler::IsSharedArrayBuffer(
    TNode<JSArrayBuffer> buffer) {
  TNode<Uint32T> bitfield =
      LoadObjectField<Uint32T>(buffer, JSArrayBuffer::kBitFieldOffset);
  return IsSetWord32<JSArrayBuffer::IsSharedBit>(bitfield);
}

// ES #sec-get-%typedarray%.prototype.set
TF_BUILTIN(TypedArrayPrototypeSet, TypedArrayBuiltinsAssembler) {
  const char* method_name = "%TypedArray%.prototype.set";
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  CodeStubArguments args(
      this,
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount)));

  Label if_source_is_typed_array(this), if_source_is_fast_jsarray(this),
      if_offset_is_out_of_bounds(this, Label::kDeferred),
      if_source_too_large(this, Label::kDeferred),
      if_receiver_is_not_typedarray(this, Label::kDeferred);

  // Check the receiver is a typed array.
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &if_receiver_is_not_typedarray);
  GotoIfNot(IsJSTypedArray(CAST(receiver)), &if_receiver_is_not_typedarray);

  // Normalize offset argument (using ToInteger) and handle heap number cases.
  TNode<Object> offset = args.GetOptionalArgumentValue(1, SmiConstant(0));
  TNode<Number> offset_num =
      ToInteger_Inline(context, offset, kTruncateMinusZero);

  // Since ToInteger always returns a Smi if the given value is within Smi
  // range, and the only corner case of -0.0 has already been truncated to 0.0,
  // we can simply throw unless the offset is a non-negative Smi.
  // TODO(jgruber): It's an observable spec violation to throw here if
  // {offset_num} is a positive number outside the Smi range. Per spec, we need
  // to check for detached buffers and call the observable ToObject/ToLength
  // operations first.
  GotoIfNot(TaggedIsPositiveSmi(offset_num), &if_offset_is_out_of_bounds);
  TNode<Smi> offset_smi = CAST(offset_num);

  // Check the receiver is not detached.
  ThrowIfArrayBufferViewBufferIsDetached(context, CAST(receiver), method_name);

  // Check the source argument is valid and whether a fast path can be taken.
  Label call_runtime(this);
  TNode<Object> source = args.GetOptionalArgumentValue(0);
  GotoIf(TaggedIsSmi(source), &call_runtime);
  GotoIf(IsJSTypedArray(CAST(source)), &if_source_is_typed_array);
  BranchIfFastJSArray(source, context, &if_source_is_fast_jsarray,
                      &call_runtime);

  // Fast path for a typed array source argument.
  BIND(&if_source_is_typed_array);
  {
    // Check the source argument is not detached.
    ThrowIfArrayBufferViewBufferIsDetached(context, CAST(source), method_name);

    SetTypedArraySource(context, CAST(source), CAST(receiver),
                        SmiUntag(offset_smi), &call_runtime,
                        &if_source_too_large);
    args.PopAndReturn(UndefinedConstant());
  }

  // Fast path for a fast JSArray source argument.
  BIND(&if_source_is_fast_jsarray);
  {
    SetJSArraySource(context, CAST(source), CAST(receiver),
                     SmiUntag(offset_smi), &call_runtime, &if_source_too_large);
    args.PopAndReturn(UndefinedConstant());
  }

  BIND(&call_runtime);
  args.PopAndReturn(CallRuntime(Runtime::kTypedArraySet, context, receiver,
                                source, offset_smi));

  BIND(&if_offset_is_out_of_bounds);
  ThrowRangeError(context, MessageTemplate::kTypedArraySetOffsetOutOfBounds);

  BIND(&if_source_too_large);
  ThrowRangeError(context, MessageTemplate::kTypedArraySetSourceTooLarge);

  BIND(&if_receiver_is_not_typedarray);
  ThrowTypeError(context, MessageTemplate::kNotTypedArray);
}

// ES #sec-get-%typedarray%.prototype-@@tostringtag
TF_BUILTIN(TypedArrayPrototypeToStringTag, TypedArrayBuiltinsAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Label if_receiverisheapobject(this), return_undefined(this);
  Branch(TaggedIsSmi(receiver), &return_undefined, &if_receiverisheapobject);

  // Dispatch on the elements kind, offset by
  // FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND.
  size_t const kTypedElementsKindCount = LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND -
                                         FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND +
                                         1;
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  Label return_##type##array(this);               \
  BIND(&return_##type##array);                    \
  Return(StringConstant(#Type "Array"));
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  Label* elements_kind_labels[kTypedElementsKindCount] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) &return_##type##array,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };
  int32_t elements_kinds[kTypedElementsKindCount] = {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  TYPE##_ELEMENTS - FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND,
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  };

  // We offset the dispatch by FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND, so
  // that this can be turned into a non-sparse table switch for ideal
  // performance.
  BIND(&if_receiverisheapobject);
  Node* elements_kind =
      Int32Sub(LoadElementsKind(receiver),
               Int32Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));
  Switch(elements_kind, &return_undefined, elements_kinds, elements_kind_labels,
         kTypedElementsKindCount);

  BIND(&return_undefined);
  Return(UndefinedConstant());
}

void TypedArrayBuiltinsAssembler::GenerateTypedArrayPrototypeIterationMethod(
    TNode<Context> context, TNode<Object> receiver, const char* method_name,
    IterationKind kind) {
  Label throw_bad_receiver(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &throw_bad_receiver);
  GotoIfNot(IsJSTypedArray(CAST(receiver)), &throw_bad_receiver);

  // Check if the {receiver}'s JSArrayBuffer was detached.
  ThrowIfArrayBufferViewBufferIsDetached(context, CAST(receiver), method_name);

  Return(CreateArrayIterator(context, receiver, kind));

  BIND(&throw_bad_receiver);
  ThrowTypeError(context, MessageTemplate::kNotTypedArray, method_name);
}

// ES #sec-%typedarray%.prototype.values
TF_BUILTIN(TypedArrayPrototypeValues, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.values()",
                                             IterationKind::kValues);
}

// ES #sec-%typedarray%.prototype.entries
TF_BUILTIN(TypedArrayPrototypeEntries, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  GenerateTypedArrayPrototypeIterationMethod(context, receiver,
                                             "%TypedArray%.prototype.entries()",
                                             IterationKind::kEntries);
}

// ES #sec-%typedarray%.prototype.keys
TF_BUILTIN(TypedArrayPrototypeKeys, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  GenerateTypedArrayPrototypeIterationMethod(
      context, receiver, "%TypedArray%.prototype.keys()", IterationKind::kKeys);
}

// ES6 #sec-%typedarray%.of
TF_BUILTIN(TypedArrayOf, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // 1. Let len be the actual number of arguments passed to this function.
  TNode<IntPtrT> length = ChangeInt32ToIntPtr(
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount)));
  // 2. Let items be the List of arguments passed to this function.
  CodeStubArguments args(this, length, nullptr, INTPTR_PARAMETERS,
                         CodeStubArguments::ReceiverMode::kHasReceiver);

  Label if_not_constructor(this, Label::kDeferred),
      if_detached(this, Label::kDeferred);

  // 3. Let C be the this value.
  // 4. If IsConstructor(C) is false, throw a TypeError exception.
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &if_not_constructor);
  GotoIfNot(IsConstructor(CAST(receiver)), &if_not_constructor);

  // 5. Let newObj be ? TypedArrayCreate(C, len).
  TNode<JSTypedArray> new_typed_array = TypedArrayCreateByLength(
      context, receiver, SmiTag(length), "%TypedArray%.of");

  TNode<Word32T> elements_kind = LoadElementsKind(new_typed_array);

  // 6. Let k be 0.
  // 7. Repeat, while k < len
  //  a. Let kValue be items[k].
  //  b. Let Pk be ! ToString(k).
  //  c. Perform ? Set(newObj, Pk, kValue, true).
  //  d. Increase k by 1.
  DispatchTypedArrayByElementsKind(
      elements_kind,
      [&](ElementsKind kind, int size, int typed_array_fun_index) {
        TNode<FixedTypedArrayBase> elements =
            CAST(LoadElements(new_typed_array));
        BuildFastLoop(
            IntPtrConstant(0), length,
            [&](Node* index) {
              TNode<Object> item = args.AtIndex(index, INTPTR_PARAMETERS);
              TNode<IntPtrT> intptr_index = UncheckedCast<IntPtrT>(index);
              if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
                EmitBigTypedArrayElementStore(new_typed_array, elements,
                                              intptr_index, item, context,
                                              &if_detached);
              } else {
                Node* value =
                    PrepareValueForWriteToTypedArray(item, kind, context);

                // ToNumber may execute JavaScript code, which could detach
                // the array's buffer.
                Node* buffer = LoadObjectField(new_typed_array,
                                               JSTypedArray::kBufferOffset);
                GotoIf(IsDetachedBuffer(buffer), &if_detached);

                // GC may move backing store in ToNumber, thus load backing
                // store everytime in this loop.
                TNode<RawPtrT> backing_store =
                    LoadFixedTypedArrayBackingStore(elements);
                StoreElement(backing_store, kind, index, value,
                             INTPTR_PARAMETERS);
              }
            },
            1, ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      });

  // 8. Return newObj.
  args.PopAndReturn(new_typed_array);

  BIND(&if_not_constructor);
  ThrowTypeError(context, MessageTemplate::kNotConstructor, receiver);

  BIND(&if_detached);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation,
                 "%TypedArray%.of");
}

// ES6 #sec-%typedarray%.from
TF_BUILTIN(TypedArrayFrom, TypedArrayBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label check_iterator(this), from_array_like(this), fast_path(this),
      slow_path(this), create_typed_array(this), check_typedarray(this),
      if_not_constructor(this, Label::kDeferred),
      if_map_fn_not_callable(this, Label::kDeferred),
      if_iterator_fn_not_callable(this, Label::kDeferred),
      if_detached(this, Label::kDeferred);

  CodeStubArguments args(
      this,
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount)));
  TNode<Object> source = args.GetOptionalArgumentValue(0);

  // 5. If thisArg is present, let T be thisArg; else let T be undefined.
  TNode<Object> this_arg = args.GetOptionalArgumentValue(2);

  // 1. Let C be the this value.
  // 2. If IsConstructor(C) is false, throw a TypeError exception.
  TNode<Object> receiver = args.GetReceiver();
  GotoIf(TaggedIsSmi(receiver), &if_not_constructor);
  GotoIfNot(IsConstructor(CAST(receiver)), &if_not_constructor);

  // 3. If mapfn is present and mapfn is not undefined, then
  TNode<Object> map_fn = args.GetOptionalArgumentValue(1);
  TVARIABLE(BoolT, mapping, Int32FalseConstant());
  GotoIf(IsUndefined(map_fn), &check_typedarray);

  //  a. If IsCallable(mapfn) is false, throw a TypeError exception.
  //  b. Let mapping be true.
  // 4. Else, let mapping be false.
  GotoIf(TaggedIsSmi(map_fn), &if_map_fn_not_callable);
  GotoIfNot(IsCallable(CAST(map_fn)), &if_map_fn_not_callable);
  mapping = Int32TrueConstant();
  Goto(&check_typedarray);

  TVARIABLE(Object, final_source);
  TVARIABLE(Smi, final_length);

  // We split up this builtin differently to the way it is written in the spec.
  // We already have great code in the elements accessor for copying from a
  // JSArray into a TypedArray, so we use that when possible. We only avoid
  // calling into the elements accessor when we have a mapping function, because
  // we can't handle that. Here, presence of a mapping function is the slow
  // path. We also combine the two different loops in the specification
  // (starting at 7.e and 13) because they are essentially identical. We also
  // save on code-size this way.

  // Get the iterator function
  BIND(&check_typedarray);
  TNode<Object> iterator_fn =
      CAST(GetMethod(context, source, isolate()->factory()->iterator_symbol(),
                     &from_array_like));
  GotoIf(TaggedIsSmi(iterator_fn), &if_iterator_fn_not_callable);

  {
    // TypedArrays have iterators, so normally we would go through the
    // IterableToList case below, which would convert the TypedArray to a
    // JSArray (boxing the values if they won't fit in a Smi).
    //
    // However, if we can guarantee that the source object has the built-in
    // iterator and that the %ArrayIteratorPrototype%.next method has not been
    // overridden, then we know the behavior of the iterator: returning the
    // values in the TypedArray sequentially from index 0 to length-1.
    //
    // In this case, we can avoid creating the intermediate array and the
    // associated HeapNumbers, and use the fast path in TypedArrayCopyElements
    // which uses the same ordering as the default iterator.
    //
    // Drop through to the default check_iterator behavior if any of these
    // checks fail.

    // Check that the source is a TypedArray
    GotoIf(TaggedIsSmi(source), &check_iterator);
    GotoIfNot(IsJSTypedArray(CAST(source)), &check_iterator);
    TNode<JSArrayBuffer> source_buffer =
        LoadJSArrayBufferViewBuffer(CAST(source));
    GotoIf(IsDetachedBuffer(source_buffer), &check_iterator);

    // Check that the iterator function is Builtins::kTypedArrayPrototypeValues
    GotoIfNot(IsJSFunction(CAST(iterator_fn)), &check_iterator);
    TNode<SharedFunctionInfo> shared_info = LoadObjectField<SharedFunctionInfo>(
        CAST(iterator_fn), JSFunction::kSharedFunctionInfoOffset);
    GotoIfNot(
        WordEqual(LoadObjectField(shared_info,
                                  SharedFunctionInfo::kFunctionDataOffset),
                  SmiConstant(Builtins::kTypedArrayPrototypeValues)),
        &check_iterator);
    // Check that the ArrayIterator prototype's "next" method hasn't been
    // overridden
    TNode<PropertyCell> protector_cell =
        CAST(LoadRoot(RootIndex::kArrayIteratorProtector));
    GotoIfNot(
        WordEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                  SmiConstant(Isolate::kProtectorValid)),
        &check_iterator);

    // Source is a TypedArray with unmodified iterator behavior. Use the
    // source object directly, taking advantage of the special-case code in
    // TypedArrayCopyElements
    final_length = LoadJSTypedArrayLength(CAST(source));
    final_source = source;
    Goto(&create_typed_array);
  }

  BIND(&check_iterator);
  {
    // 6. Let usingIterator be ? GetMethod(source, @@iterator).
    GotoIfNot(IsCallable(CAST(iterator_fn)), &if_iterator_fn_not_callable);

    // We are using the iterator.
    Label if_length_not_smi(this, Label::kDeferred);
    // 7. If usingIterator is not undefined, then
    //  a. Let values be ? IterableToList(source, usingIterator).
    //  b. Let len be the number of elements in values.
    TNode<JSArray> values = CAST(
        CallBuiltin(Builtins::kIterableToList, context, source, iterator_fn));

    // This is not a spec'd limit, so it doesn't particularly matter when we
    // throw the range error for typed array length > MaxSmi.
    TNode<Object> raw_length = LoadJSArrayLength(values);
    GotoIfNot(TaggedIsSmi(raw_length), &if_length_not_smi);

    final_length = CAST(raw_length);
    final_source = values;
    Goto(&create_typed_array);

    BIND(&if_length_not_smi);
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength,
                    raw_length);
  }

  BIND(&from_array_like);
  {
    // TODO(7881): support larger-than-smi typed array lengths
    Label if_length_not_smi(this, Label::kDeferred);
    final_source = source;

    // 10. Let len be ? ToLength(? Get(arrayLike, "length")).
    TNode<Object> raw_length =
        GetProperty(context, final_source.value(), LengthStringConstant());
    final_length = ToSmiLength(context, raw_length, &if_length_not_smi);
    Goto(&create_typed_array);

    BIND(&if_length_not_smi);
    ThrowRangeError(context, MessageTemplate::kInvalidTypedArrayLength,
                    raw_length);
  }

  TVARIABLE(JSTypedArray, target_obj);

  BIND(&create_typed_array);
  {
    // 7c/11. Let targetObj be ? TypedArrayCreate(C, «len»).
    target_obj = TypedArrayCreateByLength(
        context, receiver, final_length.value(), "%TypedArray%.from");

    Branch(mapping.value(), &slow_path, &fast_path);
  }

  BIND(&fast_path);
  {
    Label done(this);
    GotoIf(SmiEqual(final_length.value(), SmiConstant(0)), &done);

    CallRuntime(Runtime::kTypedArrayCopyElements, context, target_obj.value(),
                final_source.value(), final_length.value());
    Goto(&done);

    BIND(&done);
    args.PopAndReturn(target_obj.value());
  }

  BIND(&slow_path);
  TNode<Word32T> elements_kind = LoadElementsKind(target_obj.value());

  // 7e/13 : Copy the elements
  TNode<FixedTypedArrayBase> elements = CAST(LoadElements(target_obj.value()));
  BuildFastLoop(
      SmiConstant(0), final_length.value(),
      [&](Node* index) {
        TNode<Object> const k_value =
            GetProperty(context, final_source.value(), index);

        TNode<Object> const mapped_value =
            CAST(CallJS(CodeFactory::Call(isolate()), context, map_fn, this_arg,
                        k_value, index));

        TNode<IntPtrT> intptr_index = SmiUntag(index);
        DispatchTypedArrayByElementsKind(
            elements_kind,
            [&](ElementsKind kind, int size, int typed_array_fun_index) {
              if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
                EmitBigTypedArrayElementStore(target_obj.value(), elements,
                                              intptr_index, mapped_value,
                                              context, &if_detached);
              } else {
                Node* const final_value = PrepareValueForWriteToTypedArray(
                    mapped_value, kind, context);

                // ToNumber may execute JavaScript code, which could detach
                // the array's buffer.
                Node* buffer = LoadObjectField(target_obj.value(),
                                               JSTypedArray::kBufferOffset);
                GotoIf(IsDetachedBuffer(buffer), &if_detached);

                // GC may move backing store in map_fn, thus load backing
                // store in each iteration of this loop.
                TNode<RawPtrT> backing_store =
                    LoadFixedTypedArrayBackingStore(elements);
                StoreElement(backing_store, kind, index, final_value,
                             SMI_PARAMETERS);
              }
            });
      },
      1, ParameterMode::SMI_PARAMETERS, IndexAdvanceMode::kPost);

  args.PopAndReturn(target_obj.value());

  BIND(&if_not_constructor);
  ThrowTypeError(context, MessageTemplate::kNotConstructor, receiver);

  BIND(&if_map_fn_not_callable);
  ThrowTypeError(context, MessageTemplate::kCalledNonCallable, map_fn);

  BIND(&if_iterator_fn_not_callable);
  ThrowTypeError(context, MessageTemplate::kIteratorSymbolNonCallable);

  BIND(&if_detached);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation,
                 "%TypedArray%.from");
}

#undef V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP

}  // namespace internal
}  // namespace v8
