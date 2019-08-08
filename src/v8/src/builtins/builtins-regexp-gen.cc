// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-regexp-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/builtins/growable-fixed-array-gen.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/heap/factory-inl.h"
#include "src/logging/counters.h"
#include "src/objects/js-regexp-string-iterator.h"
#include "src/objects/js-regexp.h"
#include "src/objects/regexp-match-info.h"
#include "src/regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;

TNode<Smi> RegExpBuiltinsAssembler::SmiZero() { return SmiConstant(0); }

TNode<IntPtrT> RegExpBuiltinsAssembler::IntPtrZero() {
  return IntPtrConstant(0);
}

// -----------------------------------------------------------------------------
// ES6 section 21.2 RegExp Objects

TNode<JSRegExpResult> RegExpBuiltinsAssembler::AllocateRegExpResult(
    TNode<Context> context, TNode<Smi> length, TNode<Smi> index,
    TNode<String> input, TNode<FixedArray>* elements_out) {
  CSA_ASSERT(this, SmiLessThanOrEqual(
                       length, SmiConstant(JSArray::kMaxFastArrayLength)));
  CSA_ASSERT(this, SmiGreaterThan(length, SmiConstant(0)));

  // Allocate.

  const ElementsKind elements_kind = PACKED_ELEMENTS;
  TNode<Map> map = CAST(LoadContextElement(LoadNativeContext(context),
                                           Context::REGEXP_RESULT_MAP_INDEX));
  Node* no_allocation_site = nullptr;
  TNode<IntPtrT> length_intptr = SmiUntag(length);
  TNode<IntPtrT> capacity = length_intptr;

  // Note: The returned `elements` may be in young large object space, but
  // `array` is guaranteed to be in new space so we could skip write barriers
  // below.
  TNode<JSArray> array;
  TNode<FixedArrayBase> elements;
  std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
      elements_kind, map, length, no_allocation_site, capacity,
      INTPTR_PARAMETERS, kAllowLargeObjectAllocation, JSRegExpResult::kSize);

  // Finish result initialization.

  TNode<JSRegExpResult> result = CAST(array);

  StoreObjectFieldNoWriteBarrier(result, JSRegExpResult::kIndexOffset, index);
  // TODO(jgruber,tebbi): Could skip barrier but the MemoryOptimizer complains.
  StoreObjectField(result, JSRegExpResult::kInputOffset, input);
  StoreObjectFieldNoWriteBarrier(result, JSRegExpResult::kGroupsOffset,
                                 UndefinedConstant());

  // Finish elements initialization.

  FillFixedArrayWithValue(elements_kind, elements, IntPtrZero(), length_intptr,
                          RootIndex::kUndefinedValue);

  if (elements_out) *elements_out = CAST(elements);
  return result;
}

TNode<Object> RegExpBuiltinsAssembler::RegExpCreate(
    TNode<Context> context, TNode<Context> native_context,
    TNode<Object> maybe_string, TNode<String> flags) {
  TNode<JSFunction> regexp_function =
      CAST(LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX));
  TNode<Map> initial_map = CAST(LoadObjectField(
      regexp_function, JSFunction::kPrototypeOrInitialMapOffset));
  return RegExpCreate(context, initial_map, maybe_string, flags);
}

TNode<Object> RegExpBuiltinsAssembler::RegExpCreate(TNode<Context> context,
                                                    TNode<Map> initial_map,
                                                    TNode<Object> maybe_string,
                                                    TNode<String> flags) {
  TNode<String> pattern = Select<String>(
      IsUndefined(maybe_string), [=] { return EmptyStringConstant(); },
      [=] { return ToString_Inline(context, maybe_string); });
  TNode<Object> regexp = CAST(AllocateJSObjectFromMap(initial_map));
  return CallRuntime(Runtime::kRegExpInitializeAndCompile, context, regexp,
                     pattern, flags);
}

TNode<Object> RegExpBuiltinsAssembler::FastLoadLastIndex(
    TNode<JSRegExp> regexp) {
  // Load the in-object field.
  static const int field_offset =
      JSRegExp::kSize + JSRegExp::kLastIndexFieldIndex * kTaggedSize;
  return LoadObjectField(regexp, field_offset);
}

TNode<Object> RegExpBuiltinsAssembler::SlowLoadLastIndex(TNode<Context> context,
                                                         TNode<Object> regexp) {
  return GetProperty(context, regexp, isolate()->factory()->lastIndex_string());
}

TNode<Object> RegExpBuiltinsAssembler::LoadLastIndex(TNode<Context> context,
                                                     TNode<Object> regexp,
                                                     bool is_fastpath) {
  return is_fastpath ? FastLoadLastIndex(CAST(regexp))
                     : SlowLoadLastIndex(context, regexp);
}

// The fast-path of StoreLastIndex when regexp is guaranteed to be an unmodified
// JSRegExp instance.
void RegExpBuiltinsAssembler::FastStoreLastIndex(Node* regexp, Node* value) {
  // Store the in-object field.
  static const int field_offset =
      JSRegExp::kSize + JSRegExp::kLastIndexFieldIndex * kTaggedSize;
  StoreObjectField(regexp, field_offset, value);
}

void RegExpBuiltinsAssembler::SlowStoreLastIndex(Node* context, Node* regexp,
                                                 Node* value) {
  Node* const name = HeapConstant(isolate()->factory()->lastIndex_string());
  SetPropertyStrict(CAST(context), CAST(regexp), CAST(name), CAST(value));
}

void RegExpBuiltinsAssembler::StoreLastIndex(Node* context, Node* regexp,
                                             Node* value, bool is_fastpath) {
  if (is_fastpath) {
    FastStoreLastIndex(regexp, value);
  } else {
    SlowStoreLastIndex(context, regexp, value);
  }
}

TNode<JSRegExpResult> RegExpBuiltinsAssembler::ConstructNewResultFromMatchInfo(
    TNode<Context> context, TNode<JSReceiver> maybe_regexp,
    TNode<RegExpMatchInfo> match_info, TNode<String> string) {
  Label named_captures(this), out(this);

  TNode<IntPtrT> num_indices = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
      match_info, RegExpMatchInfo::kNumberOfCapturesIndex)));
  TNode<Smi> num_results = SmiTag(WordShr(num_indices, 1));
  TNode<Smi> start = CAST(UnsafeLoadFixedArrayElement(
      match_info, RegExpMatchInfo::kFirstCaptureIndex));
  TNode<Smi> end = CAST(UnsafeLoadFixedArrayElement(
      match_info, RegExpMatchInfo::kFirstCaptureIndex + 1));

  // Calculate the substring of the first match before creating the result array
  // to avoid an unnecessary write barrier storing the first result.

  TNode<String> first =
      CAST(CallBuiltin(Builtins::kSubString, context, string, start, end));

  TNode<FixedArray> result_elements;
  TNode<JSRegExpResult> result = AllocateRegExpResult(
      context, num_results, start, string, &result_elements);

  UnsafeStoreFixedArrayElement(result_elements, 0, first);

  // If no captures exist we can skip named capture handling as well.
  GotoIf(SmiEqual(num_results, SmiConstant(1)), &out);

  // Store all remaining captures.
  TNode<IntPtrT> limit = IntPtrAdd(
      IntPtrConstant(RegExpMatchInfo::kFirstCaptureIndex), num_indices);

  TVARIABLE(IntPtrT, var_from_cursor,
            IntPtrConstant(RegExpMatchInfo::kFirstCaptureIndex + 2));
  TVARIABLE(IntPtrT, var_to_cursor, IntPtrConstant(1));

  Variable* vars[] = {&var_from_cursor, &var_to_cursor};
  Label loop(this, 2, vars);

  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> from_cursor = var_from_cursor.value();
    TNode<IntPtrT> to_cursor = var_to_cursor.value();
    TNode<Smi> start =
        CAST(UnsafeLoadFixedArrayElement(match_info, from_cursor));

    Label next_iter(this);
    GotoIf(SmiEqual(start, SmiConstant(-1)), &next_iter);

    TNode<IntPtrT> from_cursor_plus1 =
        IntPtrAdd(from_cursor, IntPtrConstant(1));
    TNode<Smi> end =
        CAST(UnsafeLoadFixedArrayElement(match_info, from_cursor_plus1));

    TNode<String> capture =
        CAST(CallBuiltin(Builtins::kSubString, context, string, start, end));
    UnsafeStoreFixedArrayElement(result_elements, to_cursor, capture);
    Goto(&next_iter);

    BIND(&next_iter);
    var_from_cursor = IntPtrAdd(from_cursor, IntPtrConstant(2));
    var_to_cursor = IntPtrAdd(to_cursor, IntPtrConstant(1));
    Branch(UintPtrLessThan(var_from_cursor.value(), limit), &loop,
           &named_captures);
  }

  BIND(&named_captures);
  {
    CSA_ASSERT(this, SmiGreaterThan(num_results, SmiConstant(1)));

    // We reach this point only if captures exist, implying that this is an
    // IRREGEXP JSRegExp.

    TNode<JSRegExp> regexp = CAST(maybe_regexp);

    // Preparations for named capture properties. Exit early if the result does
    // not have any named captures to minimize performance impact.

    TNode<FixedArray> data =
        CAST(LoadObjectField(regexp, JSRegExp::kDataOffset));
    CSA_ASSERT(this,
               SmiEqual(CAST(LoadFixedArrayElement(data, JSRegExp::kTagIndex)),
                        SmiConstant(JSRegExp::IRREGEXP)));

    // The names fixed array associates names at even indices with a capture
    // index at odd indices.
    TNode<Object> maybe_names =
        LoadFixedArrayElement(data, JSRegExp::kIrregexpCaptureNameMapIndex);
    GotoIf(WordEqual(maybe_names, SmiZero()), &out);

    // One or more named captures exist, add a property for each one.

    TNode<FixedArray> names = CAST(maybe_names);
    TNode<IntPtrT> names_length = LoadAndUntagFixedArrayBaseLength(names);
    CSA_ASSERT(this, IntPtrGreaterThan(names_length, IntPtrZero()));

    // Allocate a new object to store the named capture properties.
    // TODO(jgruber): Could be optimized by adding the object map to the heap
    // root list.

    TNode<IntPtrT> num_properties = WordSar(names_length, 1);
    TNode<Context> native_context = LoadNativeContext(context);
    TNode<Map> map = CAST(LoadContextElement(
        native_context, Context::SLOW_OBJECT_WITH_NULL_PROTOTYPE_MAP));
    TNode<NameDictionary> properties = AllocateNameDictionary(num_properties);

    TNode<JSObject> group_object =
        CAST(AllocateJSObjectFromMap(map, properties));
    StoreObjectField(result, JSRegExpResult::kGroupsOffset, group_object);

    TVARIABLE(IntPtrT, var_i, IntPtrZero());

    Variable* vars[] = {&var_i};
    const int vars_count = sizeof(vars) / sizeof(vars[0]);
    Label loop(this, vars_count, vars);

    Goto(&loop);
    BIND(&loop);
    {
      TNode<IntPtrT> i = var_i.value();
      TNode<IntPtrT> i_plus_1 = IntPtrAdd(i, IntPtrConstant(1));
      TNode<IntPtrT> i_plus_2 = IntPtrAdd(i_plus_1, IntPtrConstant(1));

      TNode<String> name = CAST(LoadFixedArrayElement(names, i));
      TNode<Smi> index = CAST(LoadFixedArrayElement(names, i_plus_1));
      TNode<HeapObject> capture =
          CAST(LoadFixedArrayElement(result_elements, SmiUntag(index)));

      // TODO(v8:8213): For maintainability, we should call a CSA/Torque
      // implementation of CreateDataProperty instead.

      // At this point the spec says to call CreateDataProperty. However, we can
      // skip most of the steps and go straight to adding a dictionary entry
      // because we know a bunch of useful facts:
      // - All keys are non-numeric internalized strings
      // - No keys repeat
      // - Receiver has no prototype
      // - Receiver isn't used as a prototype
      // - Receiver isn't any special object like a Promise intrinsic object
      // - Receiver is extensible
      // - Receiver has no interceptors
      Label add_dictionary_property_slow(this, Label::kDeferred);
      Add<NameDictionary>(properties, name, capture,
                          &add_dictionary_property_slow);

      var_i = i_plus_2;
      Branch(IntPtrGreaterThanOrEqual(var_i.value(), names_length), &out,
             &loop);

      BIND(&add_dictionary_property_slow);
      // If the dictionary needs resizing, the above Add call will jump here
      // before making any changes. This shouldn't happen because we allocated
      // the dictionary with enough space above.
      Unreachable();
    }
  }

  BIND(&out);
  return result;
}

void RegExpBuiltinsAssembler::GetStringPointers(
    Node* const string_data, Node* const offset, Node* const last_index,
    Node* const string_length, String::Encoding encoding,
    Variable* var_string_start, Variable* var_string_end) {
  DCHECK_EQ(var_string_start->rep(), MachineType::PointerRepresentation());
  DCHECK_EQ(var_string_end->rep(), MachineType::PointerRepresentation());

  const ElementsKind kind = (encoding == String::ONE_BYTE_ENCODING)
                                ? UINT8_ELEMENTS
                                : UINT16_ELEMENTS;

  Node* const from_offset = ElementOffsetFromIndex(
      IntPtrAdd(offset, last_index), kind, INTPTR_PARAMETERS);
  var_string_start->Bind(IntPtrAdd(string_data, from_offset));

  Node* const to_offset = ElementOffsetFromIndex(
      IntPtrAdd(offset, string_length), kind, INTPTR_PARAMETERS);
  var_string_end->Bind(IntPtrAdd(string_data, to_offset));
}

TNode<HeapObject> RegExpBuiltinsAssembler::RegExpExecInternal(
    TNode<Context> context, TNode<JSRegExp> regexp, TNode<String> string,
    TNode<Number> last_index, TNode<RegExpMatchInfo> match_info) {
  ToDirectStringAssembler to_direct(state(), string);

  TVARIABLE(HeapObject, var_result);
  Label out(this), atom(this), runtime(this, Label::kDeferred);

  // External constants.
  TNode<ExternalReference> isolate_address =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  TNode<ExternalReference> regexp_stack_memory_address_address =
      ExternalConstant(
          ExternalReference::address_of_regexp_stack_memory_address(isolate()));
  TNode<ExternalReference> regexp_stack_memory_size_address = ExternalConstant(
      ExternalReference::address_of_regexp_stack_memory_size(isolate()));
  TNode<ExternalReference> static_offsets_vector_address = ExternalConstant(
      ExternalReference::address_of_static_offsets_vector(isolate()));

  // At this point, last_index is definitely a canonicalized non-negative
  // number, which implies that any non-Smi last_index is greater than
  // the maximal string length. If lastIndex > string.length then the matcher
  // must fail.

  Label if_failure(this);

  CSA_ASSERT(this, IsNumberNormalized(last_index));
  CSA_ASSERT(this, IsNumberPositive(last_index));
  GotoIf(TaggedIsNotSmi(last_index), &if_failure);

  TNode<IntPtrT> int_string_length = LoadStringLengthAsWord(string);
  TNode<IntPtrT> int_last_index = SmiUntag(CAST(last_index));

  GotoIf(UintPtrGreaterThan(int_last_index, int_string_length), &if_failure);

  // Since the RegExp has been compiled, data contains a fixed array.
  TNode<FixedArray> data = CAST(LoadObjectField(regexp, JSRegExp::kDataOffset));
  {
    // Dispatch on the type of the RegExp.
    {
      Label next(this), unreachable(this, Label::kDeferred);
      TNode<Int32T> tag = LoadAndUntagToWord32FixedArrayElement(
          data, IntPtrConstant(JSRegExp::kTagIndex));

      int32_t values[] = {
          JSRegExp::IRREGEXP, JSRegExp::ATOM, JSRegExp::NOT_COMPILED,
      };
      Label* labels[] = {&next, &atom, &runtime};

      STATIC_ASSERT(arraysize(values) == arraysize(labels));
      Switch(tag, &unreachable, values, labels, arraysize(values));

      BIND(&unreachable);
      Unreachable();

      BIND(&next);
    }

    // Check (number_of_captures + 1) * 2 <= offsets vector size
    // Or              number_of_captures <= offsets vector size / 2 - 1
    TNode<Smi> capture_count = CAST(UnsafeLoadFixedArrayElement(
        data, JSRegExp::kIrregexpCaptureCountIndex));

    const int kOffsetsSize = Isolate::kJSRegexpStaticOffsetsVectorSize;
    STATIC_ASSERT(kOffsetsSize >= 2);
    GotoIf(SmiAbove(capture_count, SmiConstant(kOffsetsSize / 2 - 1)),
           &runtime);
  }

  // Ensure that a RegExp stack is allocated. This check is after branching off
  // for ATOM regexps to avoid unnecessary trips to runtime.
  {
    TNode<IntPtrT> stack_size = UncheckedCast<IntPtrT>(
        Load(MachineType::IntPtr(), regexp_stack_memory_size_address));
    GotoIf(IntPtrEqual(stack_size, IntPtrZero()), &runtime);
  }

  // Unpack the string if possible.

  to_direct.TryToDirect(&runtime);

  // Load the irregexp code object and offsets into the subject string. Both
  // depend on whether the string is one- or two-byte.

  TVARIABLE(RawPtrT, var_string_start);
  TVARIABLE(RawPtrT, var_string_end);
  TVARIABLE(Object, var_code);

  {
    TNode<RawPtrT> direct_string_data = to_direct.PointerToData(&runtime);

    Label next(this), if_isonebyte(this), if_istwobyte(this, Label::kDeferred);
    Branch(IsOneByteStringInstanceType(to_direct.instance_type()),
           &if_isonebyte, &if_istwobyte);

    BIND(&if_isonebyte);
    {
      GetStringPointers(direct_string_data, to_direct.offset(), int_last_index,
                        int_string_length, String::ONE_BYTE_ENCODING,
                        &var_string_start, &var_string_end);
      var_code =
          UnsafeLoadFixedArrayElement(data, JSRegExp::kIrregexpLatin1CodeIndex);
      Goto(&next);
    }

    BIND(&if_istwobyte);
    {
      GetStringPointers(direct_string_data, to_direct.offset(), int_last_index,
                        int_string_length, String::TWO_BYTE_ENCODING,
                        &var_string_start, &var_string_end);
      var_code =
          UnsafeLoadFixedArrayElement(data, JSRegExp::kIrregexpUC16CodeIndex);
      Goto(&next);
    }

    BIND(&next);
  }

  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object; and otherwise it
  // contains the uninitialized sentinel as a smi.
#ifdef DEBUG
  {
    Label next(this);
    GotoIfNot(TaggedIsSmi(var_code.value()), &next);
    CSA_ASSERT(this, SmiEqual(CAST(var_code.value()),
                              SmiConstant(JSRegExp::kUninitializedValue)));
    Goto(&next);
    BIND(&next);
  }
#endif

  GotoIf(TaggedIsSmi(var_code.value()), &runtime);
  GotoIfNot(IsCode(CAST(var_code.value())), &runtime);
  TNode<Code> code = CAST(var_code.value());

  Label if_success(this), if_exception(this, Label::kDeferred);
  {
    IncrementCounter(isolate()->counters()->regexp_entry_native(), 1);

    // Set up args for the final call into generated Irregexp code.

    MachineType type_int32 = MachineType::Int32();
    MachineType type_tagged = MachineType::AnyTagged();
    MachineType type_ptr = MachineType::Pointer();

    // Result: A NativeRegExpMacroAssembler::Result return code.
    MachineType retval_type = type_int32;

    // Argument 0: Original subject string.
    MachineType arg0_type = type_tagged;
    TNode<String> arg0 = string;

    // Argument 1: Previous index.
    MachineType arg1_type = type_int32;
    TNode<Int32T> arg1 = TruncateIntPtrToInt32(int_last_index);

    // Argument 2: Start of string data.
    MachineType arg2_type = type_ptr;
    TNode<RawPtrT> arg2 = var_string_start.value();

    // Argument 3: End of string data.
    MachineType arg3_type = type_ptr;
    TNode<RawPtrT> arg3 = var_string_end.value();

    // Argument 4: static offsets vector buffer.
    MachineType arg4_type = type_ptr;
    TNode<ExternalReference> arg4 = static_offsets_vector_address;

    // Argument 5: Set the number of capture registers to zero to force global
    // regexps to behave as non-global.  This does not affect non-global
    // regexps.
    MachineType arg5_type = type_int32;
    TNode<Int32T> arg5 = Int32Constant(0);

    // Argument 6: Start (high end) of backtracking stack memory area.
    TNode<RawPtrT> stack_start = UncheckedCast<RawPtrT>(
        Load(MachineType::Pointer(), regexp_stack_memory_address_address));
    TNode<IntPtrT> stack_size = UncheckedCast<IntPtrT>(
        Load(MachineType::IntPtr(), regexp_stack_memory_size_address));
    TNode<RawPtrT> stack_end =
        ReinterpretCast<RawPtrT>(IntPtrAdd(stack_start, stack_size));

    MachineType arg6_type = type_ptr;
    TNode<RawPtrT> arg6 = stack_end;

    // Argument 7: Indicate that this is a direct call from JavaScript.
    MachineType arg7_type = type_int32;
    TNode<Int32T> arg7 = Int32Constant(1);

    // Argument 8: Pass current isolate address.
    MachineType arg8_type = type_ptr;
    TNode<ExternalReference> arg8 = isolate_address;

    TNode<RawPtrT> code_entry = ReinterpretCast<RawPtrT>(
        IntPtrAdd(BitcastTaggedToWord(code),
                  IntPtrConstant(Code::kHeaderSize - kHeapObjectTag)));

    TNode<Int32T> result = UncheckedCast<Int32T>(CallCFunction(
        code_entry, retval_type, std::make_pair(arg0_type, arg0),
        std::make_pair(arg1_type, arg1), std::make_pair(arg2_type, arg2),
        std::make_pair(arg3_type, arg3), std::make_pair(arg4_type, arg4),
        std::make_pair(arg5_type, arg5), std::make_pair(arg6_type, arg6),
        std::make_pair(arg7_type, arg7), std::make_pair(arg8_type, arg8)));

    // Check the result.
    // We expect exactly one result since we force the called regexp to behave
    // as non-global.
    TNode<IntPtrT> int_result = ChangeInt32ToIntPtr(result);
    GotoIf(IntPtrEqual(int_result,
                       IntPtrConstant(NativeRegExpMacroAssembler::SUCCESS)),
           &if_success);
    GotoIf(IntPtrEqual(int_result,
                       IntPtrConstant(NativeRegExpMacroAssembler::FAILURE)),
           &if_failure);
    GotoIf(IntPtrEqual(int_result,
                       IntPtrConstant(NativeRegExpMacroAssembler::EXCEPTION)),
           &if_exception);

    CSA_ASSERT(this,
               IntPtrEqual(int_result,
                           IntPtrConstant(NativeRegExpMacroAssembler::RETRY)));
    Goto(&runtime);
  }

  BIND(&if_success);
  {
    // Check that the last match info has space for the capture registers and
    // the additional information. Ensure no overflow in add.
    STATIC_ASSERT(FixedArray::kMaxLength < kMaxInt - FixedArray::kLengthOffset);
    TNode<Smi> available_slots =
        SmiSub(LoadFixedArrayBaseLength(match_info),
               SmiConstant(RegExpMatchInfo::kLastMatchOverhead));
    TNode<Smi> capture_count = CAST(UnsafeLoadFixedArrayElement(
        data, JSRegExp::kIrregexpCaptureCountIndex));
    // Calculate number of register_count = (capture_count + 1) * 2.
    TNode<Smi> register_count =
        SmiShl(SmiAdd(capture_count, SmiConstant(1)), 1);
    GotoIf(SmiGreaterThan(register_count, available_slots), &runtime);

    // Fill match_info.
    UnsafeStoreFixedArrayElement(match_info,
                                 RegExpMatchInfo::kNumberOfCapturesIndex,
                                 register_count, SKIP_WRITE_BARRIER);
    UnsafeStoreFixedArrayElement(match_info, RegExpMatchInfo::kLastSubjectIndex,
                                 string);
    UnsafeStoreFixedArrayElement(match_info, RegExpMatchInfo::kLastInputIndex,
                                 string);

    // Fill match and capture offsets in match_info.
    {
      TNode<IntPtrT> limit_offset = ElementOffsetFromIndex(
          register_count, INT32_ELEMENTS, SMI_PARAMETERS, 0);

      TNode<IntPtrT> to_offset = ElementOffsetFromIndex(
          IntPtrConstant(RegExpMatchInfo::kFirstCaptureIndex), PACKED_ELEMENTS,
          INTPTR_PARAMETERS, RegExpMatchInfo::kHeaderSize - kHeapObjectTag);
      TVARIABLE(IntPtrT, var_to_offset, to_offset);

      VariableList vars({&var_to_offset}, zone());
      BuildFastLoop(
          vars, IntPtrZero(), limit_offset,
          [=, &var_to_offset](Node* offset) {
            TNode<Int32T> value = UncheckedCast<Int32T>(Load(
                MachineType::Int32(), static_offsets_vector_address, offset));
            TNode<Smi> smi_value = SmiFromInt32(value);
            StoreNoWriteBarrier(MachineRepresentation::kTagged, match_info,
                                var_to_offset.value(), smi_value);
            Increment(&var_to_offset, kTaggedSize);
          },
          kInt32Size, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
    }

    var_result = match_info;
    Goto(&out);
  }

  BIND(&if_failure);
  {
    var_result = NullConstant();
    Goto(&out);
  }

  BIND(&if_exception);
  {
// A stack overflow was detected in RegExp code.
#ifdef DEBUG
    TNode<ExternalReference> pending_exception_address =
        ExternalConstant(ExternalReference::Create(
            IsolateAddressId::kPendingExceptionAddress, isolate()));
    CSA_ASSERT(this, IsTheHole(Load(MachineType::AnyTagged(),
                                    pending_exception_address)));
#endif  // DEBUG
    CallRuntime(Runtime::kThrowStackOverflow, context);
    Unreachable();
  }

  BIND(&runtime);
  {
    var_result = CAST(CallRuntime(Runtime::kRegExpExec, context, regexp, string,
                                  last_index, match_info));
    Goto(&out);
  }

  BIND(&atom);
  {
    // TODO(jgruber): A call with 4 args stresses register allocation, this
    // should probably just be inlined.
    var_result = CAST(CallBuiltin(Builtins::kRegExpExecAtom, context, regexp,
                                  string, last_index, match_info));
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

// ES#sec-regexp.prototype.exec
// RegExp.prototype.exec ( string )
// Implements the core of RegExp.prototype.exec but without actually
// constructing the JSRegExpResult. Returns a fixed array containing match
// indices as returned by RegExpExecStub on successful match, and jumps to
// if_didnotmatch otherwise.
TNode<RegExpMatchInfo>
RegExpBuiltinsAssembler::RegExpPrototypeExecBodyWithoutResult(
    TNode<Context> context, TNode<JSReceiver> maybe_regexp,
    TNode<String> string, Label* if_didnotmatch, const bool is_fastpath) {
  if (!is_fastpath) {
    ThrowIfNotInstanceType(context, maybe_regexp, JS_REGEXP_TYPE,
                           "RegExp.prototype.exec");
  }

  TNode<JSRegExp> regexp = CAST(maybe_regexp);

  TVARIABLE(HeapObject, var_result);
  Label out(this);

  // Load lastIndex.
  TVARIABLE(Number, var_lastindex);
  {
    TNode<Object> regexp_lastindex =
        LoadLastIndex(context, regexp, is_fastpath);

    if (is_fastpath) {
      // ToLength on a positive smi is a nop and can be skipped.
      CSA_ASSERT(this, TaggedIsPositiveSmi(regexp_lastindex));
      var_lastindex = CAST(regexp_lastindex);
    } else {
      // Omit ToLength if lastindex is a non-negative smi.
      Label call_tolength(this, Label::kDeferred), is_smi(this), next(this);
      Branch(TaggedIsPositiveSmi(regexp_lastindex), &is_smi, &call_tolength);

      BIND(&call_tolength);
      var_lastindex = ToLength_Inline(context, regexp_lastindex);
      Goto(&next);

      BIND(&is_smi);
      var_lastindex = CAST(regexp_lastindex);
      Goto(&next);

      BIND(&next);
    }
  }

  // Check whether the regexp is global or sticky, which determines whether we
  // update last index later on.
  TNode<Smi> flags = CAST(LoadObjectField(regexp, JSRegExp::kFlagsOffset));
  TNode<IntPtrT> is_global_or_sticky = WordAnd(
      SmiUntag(flags), IntPtrConstant(JSRegExp::kGlobal | JSRegExp::kSticky));
  TNode<BoolT> should_update_last_index =
      WordNotEqual(is_global_or_sticky, IntPtrZero());

  // Grab and possibly update last index.
  Label run_exec(this);
  {
    Label if_doupdate(this), if_dontupdate(this);
    Branch(should_update_last_index, &if_doupdate, &if_dontupdate);

    BIND(&if_doupdate);
    {
      Label if_isoob(this, Label::kDeferred);
      GotoIfNot(TaggedIsSmi(var_lastindex.value()), &if_isoob);
      TNode<Smi> string_length = LoadStringLengthAsSmi(string);
      GotoIfNot(SmiLessThanOrEqual(CAST(var_lastindex.value()), string_length),
                &if_isoob);
      Goto(&run_exec);

      BIND(&if_isoob);
      {
        StoreLastIndex(context, regexp, SmiZero(), is_fastpath);
        Goto(if_didnotmatch);
      }
    }

    BIND(&if_dontupdate);
    {
      var_lastindex = SmiZero();
      Goto(&run_exec);
    }
  }

  TNode<HeapObject> match_indices;
  Label successful_match(this);
  BIND(&run_exec);
  {
    // Get last match info from the context.
    TNode<Context> native_context = LoadNativeContext(context);
    TNode<RegExpMatchInfo> last_match_info = CAST(LoadContextElement(
        native_context, Context::REGEXP_LAST_MATCH_INFO_INDEX));

    // Call the exec stub.
    match_indices = RegExpExecInternal(context, regexp, string,
                                       var_lastindex.value(), last_match_info);
    var_result = match_indices;

    // {match_indices} is either null or the RegExpMatchInfo array.
    // Return early if exec failed, possibly updating last index.
    GotoIfNot(IsNull(match_indices), &successful_match);

    GotoIfNot(should_update_last_index, if_didnotmatch);

    StoreLastIndex(context, regexp, SmiZero(), is_fastpath);
    Goto(if_didnotmatch);
  }

  BIND(&successful_match);
  {
    GotoIfNot(should_update_last_index, &out);

    // Update the new last index from {match_indices}.
    TNode<Number> new_lastindex = CAST(UnsafeLoadFixedArrayElement(
        CAST(match_indices), RegExpMatchInfo::kFirstCaptureIndex + 1));

    StoreLastIndex(context, regexp, new_lastindex, is_fastpath);
    Goto(&out);
  }

  BIND(&out);
  return CAST(var_result.value());
}

TNode<RegExpMatchInfo>
RegExpBuiltinsAssembler::RegExpPrototypeExecBodyWithoutResultFast(
    TNode<Context> context, TNode<JSReceiver> maybe_regexp,
    TNode<String> string, Label* if_didnotmatch) {
  return RegExpPrototypeExecBodyWithoutResult(context, maybe_regexp, string,
                                              if_didnotmatch, true);
}

// ES#sec-regexp.prototype.exec
// RegExp.prototype.exec ( string )
TNode<HeapObject> RegExpBuiltinsAssembler::RegExpPrototypeExecBody(
    TNode<Context> context, TNode<JSReceiver> maybe_regexp,
    TNode<String> string, const bool is_fastpath) {
  TVARIABLE(HeapObject, var_result);

  Label if_didnotmatch(this), out(this);
  TNode<RegExpMatchInfo> match_indices = RegExpPrototypeExecBodyWithoutResult(
      context, maybe_regexp, string, &if_didnotmatch, is_fastpath);

  // Successful match.
  {
    var_result = ConstructNewResultFromMatchInfo(context, maybe_regexp,
                                                 match_indices, string);
    Goto(&out);
  }

  BIND(&if_didnotmatch);
  {
    var_result = NullConstant();
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

Node* RegExpBuiltinsAssembler::ThrowIfNotJSReceiver(
    Node* context, Node* maybe_receiver, MessageTemplate msg_template,
    char const* method_name) {
  Label out(this), throw_exception(this, Label::kDeferred);
  VARIABLE(var_value_map, MachineRepresentation::kTagged);

  GotoIf(TaggedIsSmi(maybe_receiver), &throw_exception);

  // Load the instance type of the {value}.
  var_value_map.Bind(LoadMap(maybe_receiver));
  Node* const value_instance_type = LoadMapInstanceType(var_value_map.value());

  Branch(IsJSReceiverInstanceType(value_instance_type), &out, &throw_exception);

  // The {value} is not a compatible receiver for this method.
  BIND(&throw_exception);
  {
    Node* const value_str =
        CallBuiltin(Builtins::kToString, context, maybe_receiver);
    ThrowTypeError(context, msg_template, StringConstant(method_name),
                   value_str);
  }

  BIND(&out);
  return var_value_map.value();
}

Node* RegExpBuiltinsAssembler::IsFastRegExpNoPrototype(Node* const context,
                                                       Node* const object,
                                                       Node* const map) {
  Label out(this);
  VARIABLE(var_result, MachineRepresentation::kWord32);

#ifdef V8_ENABLE_FORCE_SLOW_PATH
  var_result.Bind(Int32Constant(0));
  GotoIfForceSlowPath(&out);
#endif

  Node* const native_context = LoadNativeContext(context);
  Node* const regexp_fun =
      LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX);
  Node* const initial_map =
      LoadObjectField(regexp_fun, JSFunction::kPrototypeOrInitialMapOffset);
  Node* const has_initialmap = WordEqual(map, initial_map);

  var_result.Bind(has_initialmap);
  GotoIfNot(has_initialmap, &out);

  // The smi check is required to omit ToLength(lastIndex) calls with possible
  // user-code execution on the fast path.
  Node* const last_index = FastLoadLastIndex(CAST(object));
  var_result.Bind(TaggedIsPositiveSmi(last_index));
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

// We also return true if exec is undefined (and hence per spec)
// the original {exec} will be used.
TNode<BoolT> RegExpBuiltinsAssembler::IsFastRegExpWithOriginalExec(
    TNode<Context> context, TNode<JSRegExp> object) {
  CSA_ASSERT(this, TaggedIsNotSmi(object));
  Label out(this);
  Label check_last_index(this);
  TVARIABLE(BoolT, var_result);

#ifdef V8_ENABLE_FORCE_SLOW_PATH
  var_result = BoolConstant(false);
  GotoIfForceSlowPath(&out);
#endif

  TNode<BoolT> is_regexp = HasInstanceType(object, JS_REGEXP_TYPE);

  var_result = is_regexp;
  GotoIfNot(is_regexp, &out);

  TNode<Context> native_context = LoadNativeContext(context);
  TNode<Object> original_exec =
      LoadContextElement(native_context, Context::REGEXP_EXEC_FUNCTION_INDEX);

  TNode<Object> regexp_exec =
      GetProperty(context, object, isolate()->factory()->exec_string());

  TNode<BoolT> has_initialexec = WordEqual(regexp_exec, original_exec);
  var_result = has_initialexec;
  GotoIf(has_initialexec, &check_last_index);
  TNode<BoolT> is_undefined = IsUndefined(regexp_exec);
  var_result = is_undefined;
  GotoIfNot(is_undefined, &out);
  Goto(&check_last_index);

  BIND(&check_last_index);
  // The smi check is required to omit ToLength(lastIndex) calls with possible
  // user-code execution on the fast path.
  TNode<Object> last_index = FastLoadLastIndex(object);
  var_result = TaggedIsPositiveSmi(last_index);
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

Node* RegExpBuiltinsAssembler::IsFastRegExpNoPrototype(Node* const context,
                                                       Node* const object) {
  CSA_ASSERT(this, TaggedIsNotSmi(object));
  return IsFastRegExpNoPrototype(context, object, LoadMap(object));
}

// RegExp fast path implementations rely on unmodified JSRegExp instances.
// We use a fairly coarse granularity for this and simply check whether both
// the regexp itself is unmodified (i.e. its map has not changed), its
// prototype is unmodified, and lastIndex is a non-negative smi.
void RegExpBuiltinsAssembler::BranchIfFastRegExp(
    Node* const context, Node* const object, Node* const map,
    base::Optional<DescriptorIndexAndName> additional_property_to_check,
    Label* const if_isunmodified, Label* const if_ismodified) {
  CSA_ASSERT(this, WordEqual(LoadMap(object), map));

  GotoIfForceSlowPath(if_ismodified);

  // This should only be needed for String.p.(split||matchAll), but we are
  // conservative here.
  GotoIf(IsRegExpSpeciesProtectorCellInvalid(), if_ismodified);

  Node* const native_context = LoadNativeContext(context);
  Node* const regexp_fun =
      LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX);
  Node* const initial_map =
      LoadObjectField(regexp_fun, JSFunction::kPrototypeOrInitialMapOffset);
  Node* const has_initialmap = WordEqual(map, initial_map);

  GotoIfNot(has_initialmap, if_ismodified);

  Node* const initial_proto_initial_map =
      LoadContextElement(native_context, Context::REGEXP_PROTOTYPE_MAP_INDEX);

  DescriptorIndexAndName properties_to_check[2];
  int property_count = 0;
  properties_to_check[property_count++] = DescriptorIndexAndName{
      JSRegExp::kExecFunctionDescriptorIndex, RootIndex::kexec_string};
  if (additional_property_to_check) {
    properties_to_check[property_count++] = *additional_property_to_check;
  }

  GotoIfInitialPrototypePropertiesModified(
      CAST(map), CAST(initial_proto_initial_map),
      Vector<DescriptorIndexAndName>(properties_to_check, property_count),
      if_ismodified);

  // The smi check is required to omit ToLength(lastIndex) calls with possible
  // user-code execution on the fast path.
  Node* const last_index = FastLoadLastIndex(CAST(object));
  Branch(TaggedIsPositiveSmi(last_index), if_isunmodified, if_ismodified);
}

void RegExpBuiltinsAssembler::BranchIfFastRegExp(Node* const context,
                                                 Node* const object,
                                                 Label* const if_isunmodified,
                                                 Label* const if_ismodified) {
  CSA_ASSERT(this, TaggedIsNotSmi(object));
  BranchIfFastRegExp(context, object, LoadMap(object), base::nullopt,
                     if_isunmodified, if_ismodified);
}

TNode<BoolT> RegExpBuiltinsAssembler::IsFastRegExp(SloppyTNode<Context> context,
                                                   SloppyTNode<Object> object) {
  Label yup(this), nope(this), out(this);
  TVARIABLE(BoolT, var_result);

  BranchIfFastRegExp(context, object, &yup, &nope);

  BIND(&yup);
  var_result = Int32TrueConstant();
  Goto(&out);

  BIND(&nope);
  var_result = Int32FalseConstant();
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

void RegExpBuiltinsAssembler::BranchIfFastRegExpResult(Node* const context,
                                                       Node* const object,
                                                       Label* if_isunmodified,
                                                       Label* if_ismodified) {
  // Could be a Smi.
  Node* const map = LoadReceiverMap(object);

  Node* const native_context = LoadNativeContext(context);
  Node* const initial_regexp_result_map =
      LoadContextElement(native_context, Context::REGEXP_RESULT_MAP_INDEX);

  Branch(WordEqual(map, initial_regexp_result_map), if_isunmodified,
         if_ismodified);
}

// Slow path stub for RegExpPrototypeExec to decrease code size.
TF_BUILTIN(RegExpPrototypeExecSlow, RegExpBuiltinsAssembler) {
  TNode<JSRegExp> regexp = CAST(Parameter(Descriptor::kReceiver));
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Return(RegExpPrototypeExecBody(context, regexp, string, false));
}

// Fast path stub for ATOM regexps. String matching is done by StringIndexOf,
// and {match_info} is updated on success.
// The slow path is implemented in RegExpImpl::AtomExec.
TF_BUILTIN(RegExpExecAtom, RegExpBuiltinsAssembler) {
  TNode<JSRegExp> regexp = CAST(Parameter(Descriptor::kRegExp));
  TNode<String> subject_string = CAST(Parameter(Descriptor::kString));
  TNode<Smi> last_index = CAST(Parameter(Descriptor::kLastIndex));
  TNode<FixedArray> match_info = CAST(Parameter(Descriptor::kMatchInfo));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CSA_ASSERT(this, TaggedIsPositiveSmi(last_index));

  TNode<FixedArray> data = CAST(LoadObjectField(regexp, JSRegExp::kDataOffset));
  CSA_ASSERT(
      this,
      SmiEqual(CAST(UnsafeLoadFixedArrayElement(data, JSRegExp::kTagIndex)),
               SmiConstant(JSRegExp::ATOM)));

  // Callers ensure that last_index is in-bounds.
  CSA_ASSERT(this,
             UintPtrLessThanOrEqual(SmiUntag(last_index),
                                    LoadStringLengthAsWord(subject_string)));

  Node* const needle_string =
      UnsafeLoadFixedArrayElement(data, JSRegExp::kAtomPatternIndex);
  CSA_ASSERT(this, IsString(needle_string));

  TNode<Smi> const match_from =
      CAST(CallBuiltin(Builtins::kStringIndexOf, context, subject_string,
                       needle_string, last_index));

  Label if_failure(this), if_success(this);
  Branch(SmiEqual(match_from, SmiConstant(-1)), &if_failure, &if_success);

  BIND(&if_success);
  {
    CSA_ASSERT(this, TaggedIsPositiveSmi(match_from));
    CSA_ASSERT(this, UintPtrLessThan(SmiUntag(match_from),
                                     LoadStringLengthAsWord(subject_string)));

    const int kNumRegisters = 2;
    STATIC_ASSERT(RegExpMatchInfo::kInitialCaptureIndices >= kNumRegisters);

    TNode<Smi> const match_to =
        SmiAdd(match_from, LoadStringLengthAsSmi(needle_string));

    UnsafeStoreFixedArrayElement(
        match_info, RegExpMatchInfo::kNumberOfCapturesIndex,
        SmiConstant(kNumRegisters), SKIP_WRITE_BARRIER);
    UnsafeStoreFixedArrayElement(match_info, RegExpMatchInfo::kLastSubjectIndex,
                                 subject_string);
    UnsafeStoreFixedArrayElement(match_info, RegExpMatchInfo::kLastInputIndex,
                                 subject_string);
    UnsafeStoreFixedArrayElement(match_info,
                                 RegExpMatchInfo::kFirstCaptureIndex,
                                 match_from, SKIP_WRITE_BARRIER);
    UnsafeStoreFixedArrayElement(match_info,
                                 RegExpMatchInfo::kFirstCaptureIndex + 1,
                                 match_to, SKIP_WRITE_BARRIER);

    Return(match_info);
  }

  BIND(&if_failure);
  Return(NullConstant());
}

TF_BUILTIN(RegExpExecInternal, RegExpBuiltinsAssembler) {
  TNode<JSRegExp> regexp = CAST(Parameter(Descriptor::kRegExp));
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<Number> last_index = CAST(Parameter(Descriptor::kLastIndex));
  TNode<RegExpMatchInfo> match_info = CAST(Parameter(Descriptor::kMatchInfo));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CSA_ASSERT(this, IsNumberNormalized(last_index));
  CSA_ASSERT(this, IsNumberPositive(last_index));

  Return(RegExpExecInternal(context, regexp, string, last_index, match_info));
}

// ES#sec-regexp.prototype.exec
// RegExp.prototype.exec ( string )
TF_BUILTIN(RegExpPrototypeExec, RegExpBuiltinsAssembler) {
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_string = CAST(Parameter(Descriptor::kString));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Ensure {maybe_receiver} is a JSRegExp.
  ThrowIfNotInstanceType(context, maybe_receiver, JS_REGEXP_TYPE,
                         "RegExp.prototype.exec");
  TNode<JSRegExp> receiver = CAST(maybe_receiver);

  // Convert {maybe_string} to a String.
  TNode<String> string = ToString_Inline(context, maybe_string);

  Label if_isfastpath(this), if_isslowpath(this);
  Branch(IsFastRegExpNoPrototype(context, receiver), &if_isfastpath,
         &if_isslowpath);

  BIND(&if_isfastpath);
  Return(RegExpPrototypeExecBody(context, receiver, string, true));

  BIND(&if_isslowpath);
  Return(CallBuiltin(Builtins::kRegExpPrototypeExecSlow, context, receiver,
                     string));
}

Node* RegExpBuiltinsAssembler::FlagsGetter(Node* const context,
                                           Node* const regexp,
                                           bool is_fastpath) {
  Isolate* isolate = this->isolate();

  TNode<IntPtrT> const int_one = IntPtrConstant(1);
  TVARIABLE(Uint32T, var_length, Uint32Constant(0));
  TVARIABLE(IntPtrT, var_flags);

  // First, count the number of characters we will need and check which flags
  // are set.

  if (is_fastpath) {
    // Refer to JSRegExp's flag property on the fast-path.
    CSA_ASSERT(this, IsJSRegExp(regexp));
    Node* const flags_smi = LoadObjectField(regexp, JSRegExp::kFlagsOffset);
    var_flags = SmiUntag(flags_smi);

#define CASE_FOR_FLAG(FLAG)                                        \
  do {                                                             \
    Label next(this);                                              \
    GotoIfNot(IsSetWord(var_flags.value(), FLAG), &next);          \
    var_length = Uint32Add(var_length.value(), Uint32Constant(1)); \
    Goto(&next);                                                   \
    BIND(&next);                                                   \
  } while (false)

    CASE_FOR_FLAG(JSRegExp::kGlobal);
    CASE_FOR_FLAG(JSRegExp::kIgnoreCase);
    CASE_FOR_FLAG(JSRegExp::kMultiline);
    CASE_FOR_FLAG(JSRegExp::kDotAll);
    CASE_FOR_FLAG(JSRegExp::kUnicode);
    CASE_FOR_FLAG(JSRegExp::kSticky);
#undef CASE_FOR_FLAG
  } else {
    DCHECK(!is_fastpath);

    // Fall back to GetProperty stub on the slow-path.
    var_flags = IntPtrZero();

#define CASE_FOR_FLAG(NAME, FLAG)                                          \
  do {                                                                     \
    Label next(this);                                                      \
    Node* const flag = GetProperty(                                        \
        context, regexp, isolate->factory()->InternalizeUtf8String(NAME)); \
    Label if_isflagset(this);                                              \
    BranchIfToBooleanIsTrue(flag, &if_isflagset, &next);                   \
    BIND(&if_isflagset);                                                   \
    var_length = Uint32Add(var_length.value(), Uint32Constant(1));         \
    var_flags = Signed(WordOr(var_flags.value(), IntPtrConstant(FLAG)));   \
    Goto(&next);                                                           \
    BIND(&next);                                                           \
  } while (false)

    CASE_FOR_FLAG("global", JSRegExp::kGlobal);
    CASE_FOR_FLAG("ignoreCase", JSRegExp::kIgnoreCase);
    CASE_FOR_FLAG("multiline", JSRegExp::kMultiline);
    CASE_FOR_FLAG("dotAll", JSRegExp::kDotAll);
    CASE_FOR_FLAG("unicode", JSRegExp::kUnicode);
    CASE_FOR_FLAG("sticky", JSRegExp::kSticky);
#undef CASE_FOR_FLAG
  }

  // Allocate a string of the required length and fill it with the corresponding
  // char for each set flag.

  {
    Node* const result = AllocateSeqOneByteString(context, var_length.value());

    VARIABLE(var_offset, MachineType::PointerRepresentation(),
             IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag));

#define CASE_FOR_FLAG(FLAG, CHAR)                              \
  do {                                                         \
    Label next(this);                                          \
    GotoIfNot(IsSetWord(var_flags.value(), FLAG), &next);      \
    Node* const value = Int32Constant(CHAR);                   \
    StoreNoWriteBarrier(MachineRepresentation::kWord8, result, \
                        var_offset.value(), value);            \
    var_offset.Bind(IntPtrAdd(var_offset.value(), int_one));   \
    Goto(&next);                                               \
    BIND(&next);                                               \
  } while (false)

    CASE_FOR_FLAG(JSRegExp::kGlobal, 'g');
    CASE_FOR_FLAG(JSRegExp::kIgnoreCase, 'i');
    CASE_FOR_FLAG(JSRegExp::kMultiline, 'm');
    CASE_FOR_FLAG(JSRegExp::kDotAll, 's');
    CASE_FOR_FLAG(JSRegExp::kUnicode, 'u');
    CASE_FOR_FLAG(JSRegExp::kSticky, 'y');
#undef CASE_FOR_FLAG

    return result;
  }
}

// ES#sec-isregexp IsRegExp ( argument )
TNode<BoolT> RegExpBuiltinsAssembler::IsRegExp(TNode<Context> context,
                                               TNode<Object> maybe_receiver) {
  Label out(this), if_isregexp(this);

  TVARIABLE(BoolT, var_result, Int32FalseConstant());

  GotoIf(TaggedIsSmi(maybe_receiver), &out);
  GotoIfNot(IsJSReceiver(CAST(maybe_receiver)), &out);

  TNode<JSReceiver> receiver = CAST(maybe_receiver);

  // Check @@match.
  {
    TNode<Object> value =
        GetProperty(context, receiver, isolate()->factory()->match_symbol());

    Label match_isundefined(this), match_isnotundefined(this);
    Branch(IsUndefined(value), &match_isundefined, &match_isnotundefined);

    BIND(&match_isundefined);
    Branch(IsJSRegExp(receiver), &if_isregexp, &out);

    BIND(&match_isnotundefined);
    Label match_istrueish(this), match_isfalseish(this);
    BranchIfToBooleanIsTrue(value, &match_istrueish, &match_isfalseish);

    // The common path. Symbol.match exists, equals the RegExpPrototypeMatch
    // function (and is thus trueish), and the receiver is a JSRegExp.
    BIND(&match_istrueish);
    GotoIf(IsJSRegExp(receiver), &if_isregexp);
    CallRuntime(Runtime::kIncrementUseCounter, context,
                SmiConstant(v8::Isolate::kRegExpMatchIsTrueishOnNonJSRegExp));
    Goto(&if_isregexp);

    BIND(&match_isfalseish);
    GotoIfNot(IsJSRegExp(receiver), &out);
    CallRuntime(Runtime::kIncrementUseCounter, context,
                SmiConstant(v8::Isolate::kRegExpMatchIsFalseishOnJSRegExp));
    Goto(&out);
  }

  BIND(&if_isregexp);
  var_result = Int32TrueConstant();
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

// ES#sec-regexpinitialize
// Runtime Semantics: RegExpInitialize ( obj, pattern, flags )
Node* RegExpBuiltinsAssembler::RegExpInitialize(Node* const context,
                                                Node* const regexp,
                                                Node* const maybe_pattern,
                                                Node* const maybe_flags) {
  CSA_ASSERT(this, IsJSRegExp(regexp));

  // Normalize pattern.
  TNode<Object> const pattern = Select<Object>(
      IsUndefined(maybe_pattern), [=] { return EmptyStringConstant(); },
      [=] { return ToString_Inline(context, maybe_pattern); });

  // Normalize flags.
  TNode<Object> const flags = Select<Object>(
      IsUndefined(maybe_flags), [=] { return EmptyStringConstant(); },
      [=] { return ToString_Inline(context, maybe_flags); });

  // Initialize.

  return CallRuntime(Runtime::kRegExpInitializeAndCompile, context, regexp,
                     pattern, flags);
}

// ES #sec-get-regexp.prototype.flags
TF_BUILTIN(RegExpPrototypeFlagsGetter, RegExpBuiltinsAssembler) {
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  TNode<Map> map = CAST(ThrowIfNotJSReceiver(context, maybe_receiver,
                                             MessageTemplate::kRegExpNonObject,
                                             "RegExp.prototype.flags"));
  TNode<JSReceiver> receiver = CAST(maybe_receiver);

  Label if_isfastpath(this), if_isslowpath(this, Label::kDeferred);
  BranchIfFastRegExp(context, receiver, map, base::nullopt, &if_isfastpath,
                     &if_isslowpath);

  BIND(&if_isfastpath);
  Return(FlagsGetter(context, receiver, true));

  BIND(&if_isslowpath);
  Return(FlagsGetter(context, receiver, false));
}

// ES#sec-regexp-pattern-flags
// RegExp ( pattern, flags )
TF_BUILTIN(RegExpConstructor, RegExpBuiltinsAssembler) {
  TNode<Object> pattern = CAST(Parameter(Descriptor::kPattern));
  TNode<Object> flags = CAST(Parameter(Descriptor::kFlags));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Isolate* isolate = this->isolate();

  VARIABLE(var_flags, MachineRepresentation::kTagged, flags);
  VARIABLE(var_pattern, MachineRepresentation::kTagged, pattern);
  VARIABLE(var_new_target, MachineRepresentation::kTagged, new_target);

  Node* const native_context = LoadNativeContext(context);
  Node* const regexp_function =
      LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX);

  TNode<BoolT> pattern_is_regexp = IsRegExp(context, pattern);

  {
    Label next(this);

    GotoIfNot(IsUndefined(new_target), &next);
    var_new_target.Bind(regexp_function);

    GotoIfNot(pattern_is_regexp, &next);
    GotoIfNot(IsUndefined(flags), &next);

    Node* const value =
        GetProperty(context, pattern, isolate->factory()->constructor_string());

    GotoIfNot(WordEqual(value, regexp_function), &next);
    Return(pattern);

    BIND(&next);
  }

  {
    Label next(this), if_patternisfastregexp(this),
        if_patternisslowregexp(this);
    GotoIf(TaggedIsSmi(pattern), &next);

    GotoIf(IsJSRegExp(CAST(pattern)), &if_patternisfastregexp);

    Branch(pattern_is_regexp, &if_patternisslowregexp, &next);

    BIND(&if_patternisfastregexp);
    {
      Node* const source =
          LoadObjectField(CAST(pattern), JSRegExp::kSourceOffset);
      var_pattern.Bind(source);

      {
        Label inner_next(this);
        GotoIfNot(IsUndefined(flags), &inner_next);

        Node* const value = FlagsGetter(context, pattern, true);
        var_flags.Bind(value);
        Goto(&inner_next);

        BIND(&inner_next);
      }

      Goto(&next);
    }

    BIND(&if_patternisslowregexp);
    {
      {
        Node* const value =
            GetProperty(context, pattern, isolate->factory()->source_string());
        var_pattern.Bind(value);
      }

      {
        Label inner_next(this);
        GotoIfNot(IsUndefined(flags), &inner_next);

        Node* const value =
            GetProperty(context, pattern, isolate->factory()->flags_string());
        var_flags.Bind(value);
        Goto(&inner_next);

        BIND(&inner_next);
      }

      Goto(&next);
    }

    BIND(&next);
  }

  // Allocate.

  VARIABLE(var_regexp, MachineRepresentation::kTagged);
  {
    Label allocate_jsregexp(this), allocate_generic(this, Label::kDeferred),
        next(this);
    Branch(WordEqual(var_new_target.value(), regexp_function),
           &allocate_jsregexp, &allocate_generic);

    BIND(&allocate_jsregexp);
    {
      Node* const initial_map = LoadObjectField(
          regexp_function, JSFunction::kPrototypeOrInitialMapOffset);
      Node* const regexp = AllocateJSObjectFromMap(initial_map);
      var_regexp.Bind(regexp);
      Goto(&next);
    }

    BIND(&allocate_generic);
    {
      ConstructorBuiltinsAssembler constructor_assembler(this->state());
      Node* const regexp = constructor_assembler.EmitFastNewObject(
          context, regexp_function, var_new_target.value());
      var_regexp.Bind(regexp);
      Goto(&next);
    }

    BIND(&next);
  }

  Node* const result = RegExpInitialize(context, var_regexp.value(),
                                        var_pattern.value(), var_flags.value());
  Return(result);
}

// ES#sec-regexp.prototype.compile
// RegExp.prototype.compile ( pattern, flags )
TF_BUILTIN(RegExpPrototypeCompile, RegExpBuiltinsAssembler) {
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_pattern = CAST(Parameter(Descriptor::kPattern));
  TNode<Object> maybe_flags = CAST(Parameter(Descriptor::kFlags));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  ThrowIfNotInstanceType(context, maybe_receiver, JS_REGEXP_TYPE,
                         "RegExp.prototype.compile");
  Node* const receiver = maybe_receiver;

  VARIABLE(var_flags, MachineRepresentation::kTagged, maybe_flags);
  VARIABLE(var_pattern, MachineRepresentation::kTagged, maybe_pattern);

  // Handle a JSRegExp pattern.
  {
    Label next(this);

    GotoIf(TaggedIsSmi(maybe_pattern), &next);
    GotoIfNot(IsJSRegExp(CAST(maybe_pattern)), &next);

    Node* const pattern = maybe_pattern;

    // {maybe_flags} must be undefined in this case, otherwise throw.
    {
      Label next(this);
      GotoIf(IsUndefined(maybe_flags), &next);

      ThrowTypeError(context, MessageTemplate::kRegExpFlags);

      BIND(&next);
    }

    Node* const new_flags = FlagsGetter(context, pattern, true);
    Node* const new_pattern = LoadObjectField(pattern, JSRegExp::kSourceOffset);

    var_flags.Bind(new_flags);
    var_pattern.Bind(new_pattern);

    Goto(&next);
    BIND(&next);
  }

  Node* const result = RegExpInitialize(context, receiver, var_pattern.value(),
                                        var_flags.value());
  Return(result);
}

// ES6 21.2.5.10.
// ES #sec-get-regexp.prototype.source
TF_BUILTIN(RegExpPrototypeSourceGetter, RegExpBuiltinsAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Check whether we have an unmodified regexp instance.
  Label if_isjsregexp(this), if_isnotjsregexp(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &if_isnotjsregexp);
  Branch(IsJSRegExp(CAST(receiver)), &if_isjsregexp, &if_isnotjsregexp);

  BIND(&if_isjsregexp);
  Return(LoadObjectField(CAST(receiver), JSRegExp::kSourceOffset));

  BIND(&if_isnotjsregexp);
  {
    Isolate* isolate = this->isolate();
    Node* const native_context = LoadNativeContext(context);
    Node* const regexp_fun =
        LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX);
    Node* const initial_map =
        LoadObjectField(regexp_fun, JSFunction::kPrototypeOrInitialMapOffset);
    Node* const initial_prototype = LoadMapPrototype(initial_map);

    Label if_isprototype(this), if_isnotprototype(this);
    Branch(WordEqual(receiver, initial_prototype), &if_isprototype,
           &if_isnotprototype);

    BIND(&if_isprototype);
    {
      const int counter = v8::Isolate::kRegExpPrototypeSourceGetter;
      Node* const counter_smi = SmiConstant(counter);
      CallRuntime(Runtime::kIncrementUseCounter, context, counter_smi);

      Node* const result =
          HeapConstant(isolate->factory()->NewStringFromAsciiChecked("(?:)"));
      Return(result);
    }

    BIND(&if_isnotprototype);
    {
      ThrowTypeError(context, MessageTemplate::kRegExpNonRegExp,
                     "RegExp.prototype.source");
    }
  }
}

// Fast-path implementation for flag checks on an unmodified JSRegExp instance.
TNode<Int32T> RegExpBuiltinsAssembler::FastFlagGetter(TNode<JSRegExp> regexp,
                                                      JSRegExp::Flag flag) {
  TNode<Smi> flags = CAST(LoadObjectField(regexp, JSRegExp::kFlagsOffset));
  TNode<Smi> mask = SmiConstant(flag);
  return SmiToInt32(SmiShr(SmiAnd(flags, mask), JSRegExp::FlagShiftBits(flag)));
}

// Load through the GetProperty stub.
TNode<Int32T> RegExpBuiltinsAssembler::SlowFlagGetter(TNode<Context> context,
                                                      TNode<Object> regexp,
                                                      JSRegExp::Flag flag) {
  Label out(this);
  TVARIABLE(Int32T, var_result);

  Handle<String> name;
  switch (flag) {
    case JSRegExp::kGlobal:
      name = isolate()->factory()->global_string();
      break;
    case JSRegExp::kIgnoreCase:
      name = isolate()->factory()->ignoreCase_string();
      break;
    case JSRegExp::kMultiline:
      name = isolate()->factory()->multiline_string();
      break;
    case JSRegExp::kDotAll:
      UNREACHABLE();  // Never called for dotAll.
      break;
    case JSRegExp::kSticky:
      name = isolate()->factory()->sticky_string();
      break;
    case JSRegExp::kUnicode:
      name = isolate()->factory()->unicode_string();
      break;
    default:
      UNREACHABLE();
  }

  TNode<Object> value = GetProperty(context, regexp, name);

  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);

  BIND(&if_true);
  var_result = Int32Constant(1);
  Goto(&out);

  BIND(&if_false);
  var_result = Int32Constant(0);
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

TNode<Int32T> RegExpBuiltinsAssembler::FlagGetter(TNode<Context> context,
                                                  TNode<Object> regexp,
                                                  JSRegExp::Flag flag,
                                                  bool is_fastpath) {
  return is_fastpath ? FastFlagGetter(CAST(regexp), flag)
                     : SlowFlagGetter(context, regexp, flag);
}

void RegExpBuiltinsAssembler::FlagGetter(Node* context, Node* receiver,
                                         JSRegExp::Flag flag, int counter,
                                         const char* method_name) {
  // Check whether we have an unmodified regexp instance.
  Label if_isunmodifiedjsregexp(this),
      if_isnotunmodifiedjsregexp(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(receiver), &if_isnotunmodifiedjsregexp);
  Branch(IsJSRegExp(receiver), &if_isunmodifiedjsregexp,
         &if_isnotunmodifiedjsregexp);

  BIND(&if_isunmodifiedjsregexp);
  {
    // Refer to JSRegExp's flag property on the fast-path.
    Node* const is_flag_set = FastFlagGetter(CAST(receiver), flag);
    Return(SelectBooleanConstant(is_flag_set));
  }

  BIND(&if_isnotunmodifiedjsregexp);
  {
    Node* const native_context = LoadNativeContext(context);
    Node* const regexp_fun =
        LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX);
    Node* const initial_map =
        LoadObjectField(regexp_fun, JSFunction::kPrototypeOrInitialMapOffset);
    Node* const initial_prototype = LoadMapPrototype(initial_map);

    Label if_isprototype(this), if_isnotprototype(this);
    Branch(WordEqual(receiver, initial_prototype), &if_isprototype,
           &if_isnotprototype);

    BIND(&if_isprototype);
    {
      if (counter != -1) {
        Node* const counter_smi = SmiConstant(counter);
        CallRuntime(Runtime::kIncrementUseCounter, context, counter_smi);
      }
      Return(UndefinedConstant());
    }

    BIND(&if_isnotprototype);
    { ThrowTypeError(context, MessageTemplate::kRegExpNonRegExp, method_name); }
  }
}

// ES6 21.2.5.4.
// ES #sec-get-regexp.prototype.global
TF_BUILTIN(RegExpPrototypeGlobalGetter, RegExpBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  FlagGetter(context, receiver, JSRegExp::kGlobal,
             v8::Isolate::kRegExpPrototypeOldFlagGetter,
             "RegExp.prototype.global");
}

// ES6 21.2.5.5.
// ES #sec-get-regexp.prototype.ignorecase
TF_BUILTIN(RegExpPrototypeIgnoreCaseGetter, RegExpBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  FlagGetter(context, receiver, JSRegExp::kIgnoreCase,
             v8::Isolate::kRegExpPrototypeOldFlagGetter,
             "RegExp.prototype.ignoreCase");
}

// ES6 21.2.5.7.
// ES #sec-get-regexp.prototype.multiline
TF_BUILTIN(RegExpPrototypeMultilineGetter, RegExpBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  FlagGetter(context, receiver, JSRegExp::kMultiline,
             v8::Isolate::kRegExpPrototypeOldFlagGetter,
             "RegExp.prototype.multiline");
}

// ES #sec-get-regexp.prototype.dotAll
TF_BUILTIN(RegExpPrototypeDotAllGetter, RegExpBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  static const int kNoCounter = -1;
  FlagGetter(context, receiver, JSRegExp::kDotAll, kNoCounter,
             "RegExp.prototype.dotAll");
}

// ES6 21.2.5.12.
// ES #sec-get-regexp.prototype.sticky
TF_BUILTIN(RegExpPrototypeStickyGetter, RegExpBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  FlagGetter(context, receiver, JSRegExp::kSticky,
             v8::Isolate::kRegExpPrototypeStickyGetter,
             "RegExp.prototype.sticky");
}

// ES6 21.2.5.15.
// ES #sec-get-regexp.prototype.unicode
TF_BUILTIN(RegExpPrototypeUnicodeGetter, RegExpBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  FlagGetter(context, receiver, JSRegExp::kUnicode,
             v8::Isolate::kRegExpPrototypeUnicodeGetter,
             "RegExp.prototype.unicode");
}

// ES#sec-regexpexec Runtime Semantics: RegExpExec ( R, S )
Node* RegExpBuiltinsAssembler::RegExpExec(Node* context, Node* regexp,
                                          Node* string) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label out(this);

  // Take the slow path of fetching the exec property, calling it, and
  // verifying its return value.

  // Get the exec property.
  Node* const exec =
      GetProperty(context, regexp, isolate()->factory()->exec_string());

  // Is {exec} callable?
  Label if_iscallable(this), if_isnotcallable(this);

  GotoIf(TaggedIsSmi(exec), &if_isnotcallable);

  Node* const exec_map = LoadMap(exec);
  Branch(IsCallableMap(exec_map), &if_iscallable, &if_isnotcallable);

  BIND(&if_iscallable);
  {
    Callable call_callable = CodeFactory::Call(isolate());
    Node* const result = CallJS(call_callable, context, exec, regexp, string);

    var_result.Bind(result);
    GotoIf(IsNull(result), &out);

    ThrowIfNotJSReceiver(context, result,
                         MessageTemplate::kInvalidRegExpExecResult, "");

    Goto(&out);
  }

  BIND(&if_isnotcallable);
  {
    ThrowIfNotInstanceType(context, regexp, JS_REGEXP_TYPE,
                           "RegExp.prototype.exec");

    Node* const result = CallBuiltin(Builtins::kRegExpPrototypeExecSlow,
                                     context, regexp, string);
    var_result.Bind(result);
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

// ES#sec-regexp.prototype.test
// RegExp.prototype.test ( S )
TF_BUILTIN(RegExpPrototypeTest, RegExpBuiltinsAssembler) {
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_string = CAST(Parameter(Descriptor::kString));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Ensure {maybe_receiver} is a JSReceiver.
  ThrowIfNotJSReceiver(context, maybe_receiver,
                       MessageTemplate::kIncompatibleMethodReceiver,
                       "RegExp.prototype.test");
  TNode<JSReceiver> receiver = CAST(maybe_receiver);

  // Convert {maybe_string} to a String.
  TNode<String> string = ToString_Inline(context, maybe_string);

  Label fast_path(this), slow_path(this);
  BranchIfFastRegExp(context, receiver, &fast_path, &slow_path);

  BIND(&fast_path);
  {
    Label if_didnotmatch(this);
    RegExpPrototypeExecBodyWithoutResult(context, receiver, string,
                                         &if_didnotmatch, true);
    Return(TrueConstant());

    BIND(&if_didnotmatch);
    Return(FalseConstant());
  }

  BIND(&slow_path);
  {
    // Call exec.
    TNode<HeapObject> match_indices =
        CAST(RegExpExec(context, receiver, string));

    // Return true iff exec matched successfully.
    Return(SelectBooleanConstant(IsNotNull(match_indices)));
  }
}

TF_BUILTIN(RegExpPrototypeTestFast, RegExpBuiltinsAssembler) {
  TNode<JSRegExp> regexp = CAST(Parameter(Descriptor::kReceiver));
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label if_didnotmatch(this);
  CSA_ASSERT(this, IsFastRegExpWithOriginalExec(context, regexp));
  RegExpPrototypeExecBodyWithoutResult(context, regexp, string, &if_didnotmatch,
                                       true);
  Return(TrueConstant());

  BIND(&if_didnotmatch);
  Return(FalseConstant());
}

Node* RegExpBuiltinsAssembler::AdvanceStringIndex(Node* const string,
                                                  Node* const index,
                                                  Node* const is_unicode,
                                                  bool is_fastpath) {
  CSA_ASSERT(this, IsString(string));
  CSA_ASSERT(this, IsNumberNormalized(index));
  if (is_fastpath) CSA_ASSERT(this, TaggedIsPositiveSmi(index));

  // Default to last_index + 1.
  // TODO(pwong): Consider using TrySmiAdd for the fast path to reduce generated
  // code.
  Node* const index_plus_one = NumberInc(index);
  VARIABLE(var_result, MachineRepresentation::kTagged, index_plus_one);

  // Advancing the index has some subtle issues involving the distinction
  // between Smis and HeapNumbers. There's three cases:
  // * {index} is a Smi, {index_plus_one} is a Smi. The standard case.
  // * {index} is a Smi, {index_plus_one} overflows into a HeapNumber.
  //   In this case we can return the result early, because
  //   {index_plus_one} > {string}.length.
  // * {index} is a HeapNumber, {index_plus_one} is a HeapNumber. This can only
  //   occur when {index} is outside the Smi range since we normalize
  //   explicitly. Again we can return early.
  if (is_fastpath) {
    // Must be in Smi range on the fast path. We control the value of {index}
    // on all call-sites and can never exceed the length of the string.
    STATIC_ASSERT(String::kMaxLength + 2 < Smi::kMaxValue);
    CSA_ASSERT(this, TaggedIsPositiveSmi(index_plus_one));
  }

  Label if_isunicode(this), out(this);
  GotoIfNot(is_unicode, &out);

  // Keep this unconditional (even on the fast path) just to be safe.
  Branch(TaggedIsPositiveSmi(index_plus_one), &if_isunicode, &out);

  BIND(&if_isunicode);
  {
    TNode<IntPtrT> const string_length = LoadStringLengthAsWord(string);
    TNode<IntPtrT> untagged_plus_one = SmiUntag(index_plus_one);
    GotoIfNot(IntPtrLessThan(untagged_plus_one, string_length), &out);

    Node* const lead = StringCharCodeAt(string, SmiUntag(index));
    GotoIfNot(Word32Equal(Word32And(lead, Int32Constant(0xFC00)),
                          Int32Constant(0xD800)),
              &out);

    Node* const trail = StringCharCodeAt(string, untagged_plus_one);
    GotoIfNot(Word32Equal(Word32And(trail, Int32Constant(0xFC00)),
                          Int32Constant(0xDC00)),
              &out);

    // At a surrogate pair, return index + 2.
    Node* const index_plus_two = NumberInc(index_plus_one);
    var_result.Bind(index_plus_two);

    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

void RegExpBuiltinsAssembler::RegExpPrototypeMatchBody(Node* const context,
                                                       Node* const regexp,
                                                       TNode<String> string,
                                                       const bool is_fastpath) {
  if (is_fastpath) CSA_ASSERT(this, IsFastRegExp(context, regexp));

  Node* const is_global =
      FlagGetter(CAST(context), CAST(regexp), JSRegExp::kGlobal, is_fastpath);

  Label if_isglobal(this), if_isnotglobal(this);
  Branch(is_global, &if_isglobal, &if_isnotglobal);

  BIND(&if_isnotglobal);
  {
    Node* const result =
        is_fastpath
            ? RegExpPrototypeExecBody(CAST(context), CAST(regexp), string, true)
            : RegExpExec(context, regexp, string);
    Return(result);
  }

  BIND(&if_isglobal);
  {
    Node* const is_unicode = FlagGetter(CAST(context), CAST(regexp),
                                        JSRegExp::kUnicode, is_fastpath);

    StoreLastIndex(context, regexp, SmiZero(), is_fastpath);

    // Allocate an array to store the resulting match strings.

    GrowableFixedArray array(state());

    // Loop preparations. Within the loop, collect results from RegExpExec
    // and store match strings in the array.

    Variable* vars[] = {array.var_array(), array.var_length(),
                        array.var_capacity()};
    Label loop(this, 3, vars), out(this);

    // Check if the regexp is an ATOM type. If then, keep the literal string to
    // search for so that we can avoid calling substring in the loop below.
    TVARIABLE(BoolT, var_atom, Int32FalseConstant());
    TVARIABLE(String, var_search_string, EmptyStringConstant());
    if (is_fastpath) {
      TNode<JSRegExp> maybe_atom_regexp = CAST(regexp);
      TNode<FixedArray> data =
          CAST(LoadObjectField(maybe_atom_regexp, JSRegExp::kDataOffset));
      GotoIfNot(SmiEqual(CAST(LoadFixedArrayElement(data, JSRegExp::kTagIndex)),
                         SmiConstant(JSRegExp::ATOM)),
                &loop);
      var_search_string =
          CAST(LoadFixedArrayElement(data, JSRegExp::kAtomPatternIndex));
      var_atom = Int32TrueConstant();
    }
    Goto(&loop);

    BIND(&loop);
    {
      VARIABLE(var_match, MachineRepresentation::kTagged);

      Label if_didmatch(this), if_didnotmatch(this);
      if (is_fastpath) {
        // On the fast path, grab the matching string from the raw match index
        // array.
        TNode<RegExpMatchInfo> match_indices =
            RegExpPrototypeExecBodyWithoutResult(CAST(context), CAST(regexp),
                                                 string, &if_didnotmatch, true);

        Label dosubstring(this), donotsubstring(this);
        Branch(var_atom.value(), &donotsubstring, &dosubstring);

        BIND(&dosubstring);
        {
          Node* const match_from = UnsafeLoadFixedArrayElement(
              match_indices, RegExpMatchInfo::kFirstCaptureIndex);
          Node* const match_to = UnsafeLoadFixedArrayElement(
              match_indices, RegExpMatchInfo::kFirstCaptureIndex + 1);
          var_match.Bind(CallBuiltin(Builtins::kSubString, context, string,
                                     match_from, match_to));
          Goto(&if_didmatch);
        }

        BIND(&donotsubstring);
        var_match.Bind(var_search_string.value());
        Goto(&if_didmatch);
      } else {
        DCHECK(!is_fastpath);
        Node* const result = RegExpExec(context, regexp, string);

        Label load_match(this);
        Branch(IsNull(result), &if_didnotmatch, &load_match);

        BIND(&load_match);
        var_match.Bind(
            ToString_Inline(context, GetProperty(context, result, SmiZero())));
        Goto(&if_didmatch);
      }

      BIND(&if_didnotmatch);
      {
        // Return null if there were no matches, otherwise just exit the loop.
        GotoIfNot(IntPtrEqual(array.length(), IntPtrZero()), &out);
        Return(NullConstant());
      }

      BIND(&if_didmatch);
      {
        Node* match = var_match.value();

        // Store the match, growing the fixed array if needed.

        array.Push(CAST(match));

        // Advance last index if the match is the empty string.

        TNode<Smi> const match_length = LoadStringLengthAsSmi(match);
        GotoIfNot(SmiEqual(match_length, SmiZero()), &loop);

        Node* last_index =
            LoadLastIndex(CAST(context), CAST(regexp), is_fastpath);
        if (is_fastpath) {
          CSA_ASSERT(this, TaggedIsPositiveSmi(last_index));
        } else {
          last_index = ToLength_Inline(context, last_index);
        }

        Node* const new_last_index =
            AdvanceStringIndex(string, last_index, is_unicode, is_fastpath);

        if (is_fastpath) {
          // On the fast path, we can be certain that lastIndex can never be
          // incremented to overflow the Smi range since the maximal string
          // length is less than the maximal Smi value.
          STATIC_ASSERT(String::kMaxLength < Smi::kMaxValue);
          CSA_ASSERT(this, TaggedIsPositiveSmi(new_last_index));
        }

        StoreLastIndex(context, regexp, new_last_index, is_fastpath);

        Goto(&loop);
      }
    }

    BIND(&out);
    {
      // Wrap the match in a JSArray.

      Node* const result = array.ToJSArray(CAST(context));
      Return(result);
    }
  }
}

// ES#sec-regexp.prototype-@@match
// RegExp.prototype [ @@match ] ( string )
TF_BUILTIN(RegExpPrototypeMatch, RegExpBuiltinsAssembler) {
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_string = CAST(Parameter(Descriptor::kString));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Ensure {maybe_receiver} is a JSReceiver.
  ThrowIfNotJSReceiver(context, maybe_receiver,
                       MessageTemplate::kIncompatibleMethodReceiver,
                       "RegExp.prototype.@@match");
  Node* const receiver = maybe_receiver;

  // Convert {maybe_string} to a String.
  TNode<String> const string = ToString_Inline(context, maybe_string);

  Label fast_path(this), slow_path(this);
  BranchIfFastRegExp(context, receiver, &fast_path, &slow_path);

  BIND(&fast_path);
  // TODO(pwong): Could be optimized to remove the overhead of calling the
  //              builtin (at the cost of a larger builtin).
  Return(CallBuiltin(Builtins::kRegExpMatchFast, context, receiver, string));

  BIND(&slow_path);
  RegExpPrototypeMatchBody(context, receiver, string, false);
}

void RegExpMatchAllAssembler::Generate(TNode<Context> context,
                                       TNode<Context> native_context,
                                       TNode<Object> receiver,
                                       TNode<Object> maybe_string) {
  // 1. Let R be the this value.
  // 2. If Type(R) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, receiver,
                       MessageTemplate::kIncompatibleMethodReceiver,
                       "RegExp.prototype.@@matchAll");

  // 3. Let S be ? ToString(O).
  TNode<String> string = ToString_Inline(context, maybe_string);

  TVARIABLE(Object, var_matcher);
  TVARIABLE(Int32T, var_global);
  TVARIABLE(Int32T, var_unicode);
  Label create_iterator(this), if_fast_regexp(this),
      if_slow_regexp(this, Label::kDeferred);

  BranchIfFastRegExp(context, receiver, &if_fast_regexp, &if_slow_regexp);
  BIND(&if_fast_regexp);
  {
    TNode<JSRegExp> fast_regexp = CAST(receiver);
    TNode<Object> source =
        LoadObjectField(fast_regexp, JSRegExp::kSourceOffset);

    // 4. Let C be ? SpeciesConstructor(R, %RegExp%).
    // 5. Let flags be ? ToString(? Get(R, "flags")).
    // 6. Let matcher be ? Construct(C, « R, flags »).
    TNode<String> flags = CAST(FlagsGetter(context, fast_regexp, true));
    var_matcher = RegExpCreate(context, native_context, source, flags);
    CSA_ASSERT(this, IsFastRegExp(context, var_matcher.value()));

    // 7. Let lastIndex be ? ToLength(? Get(R, "lastIndex")).
    // 8. Perform ? Set(matcher, "lastIndex", lastIndex, true).
    FastStoreLastIndex(var_matcher.value(), FastLoadLastIndex(fast_regexp));

    // 9. If flags contains "g", let global be true.
    // 10. Else, let global be false.
    var_global = FastFlagGetter(CAST(var_matcher.value()), JSRegExp::kGlobal);

    // 11. If flags contains "u", let fullUnicode be true.
    // 12. Else, let fullUnicode be false.
    var_unicode = FastFlagGetter(CAST(var_matcher.value()), JSRegExp::kUnicode);
    Goto(&create_iterator);
  }

  BIND(&if_slow_regexp);
  {
    // 4. Let C be ? SpeciesConstructor(R, %RegExp%).
    TNode<JSFunction> regexp_fun = CAST(
        LoadContextElement(native_context, Context::REGEXP_FUNCTION_INDEX));
    TNode<JSReceiver> species_constructor =
        SpeciesConstructor(native_context, receiver, regexp_fun);

    // 5. Let flags be ? ToString(? Get(R, "flags")).
    TNode<Object> flags =
        GetProperty(context, receiver, isolate()->factory()->flags_string());
    TNode<String> flags_string = ToString_Inline(context, flags);

    // 6. Let matcher be ? Construct(C, « R, flags »).
    var_matcher =
        Construct(context, species_constructor, receiver, flags_string);

    // 7. Let lastIndex be ? ToLength(? Get(R, "lastIndex")).
    TNode<Number> last_index =
        ToLength_Inline(context, SlowLoadLastIndex(context, receiver));

    // 8. Perform ? Set(matcher, "lastIndex", lastIndex, true).
    SlowStoreLastIndex(context, var_matcher.value(), last_index);

    // 9. If flags contains "g", let global be true.
    // 10. Else, let global be false.
    TNode<String> global_char_string = StringConstant("g");
    TNode<Smi> global_ix =
        CAST(CallBuiltin(Builtins::kStringIndexOf, context, flags_string,
                         global_char_string, SmiZero()));
    var_global =
        SelectInt32Constant(SmiEqual(global_ix, SmiConstant(-1)), 0, 1);

    // 11. If flags contains "u", let fullUnicode be true.
    // 12. Else, let fullUnicode be false.
    TNode<String> unicode_char_string = StringConstant("u");
    TNode<Smi> unicode_ix =
        CAST(CallBuiltin(Builtins::kStringIndexOf, context, flags_string,
                         unicode_char_string, SmiZero()));
    var_unicode =
        SelectInt32Constant(SmiEqual(unicode_ix, SmiConstant(-1)), 0, 1);
    Goto(&create_iterator);
  }

  BIND(&create_iterator);
  {
    // 13. Return ! CreateRegExpStringIterator(matcher, S, global, fullUnicode).
    TNode<Object> iterator =
        CreateRegExpStringIterator(native_context, var_matcher.value(), string,
                                   var_global.value(), var_unicode.value());
    Return(iterator);
  }
}

// ES#sec-createregexpstringiterator
// CreateRegExpStringIterator ( R, S, global, fullUnicode )
TNode<Object> RegExpMatchAllAssembler::CreateRegExpStringIterator(
    TNode<Context> native_context, TNode<Object> regexp, TNode<String> string,
    TNode<Int32T> global, TNode<Int32T> full_unicode) {
  TNode<Map> map = CAST(LoadContextElement(
      native_context,
      Context::INITIAL_REGEXP_STRING_ITERATOR_PROTOTYPE_MAP_INDEX));

  // 4. Let iterator be ObjectCreate(%RegExpStringIteratorPrototype%, «
  // [[IteratingRegExp]], [[IteratedString]], [[Global]], [[Unicode]],
  // [[Done]] »).
  TNode<Object> iterator = Allocate(JSRegExpStringIterator::kSize);
  StoreMapNoWriteBarrier(iterator, map);
  StoreObjectFieldRoot(iterator,
                       JSRegExpStringIterator::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(iterator, JSRegExpStringIterator::kElementsOffset,
                       RootIndex::kEmptyFixedArray);

  // 5. Set iterator.[[IteratingRegExp]] to R.
  StoreObjectFieldNoWriteBarrier(
      iterator, JSRegExpStringIterator::kIteratingRegExpOffset, regexp);

  // 6. Set iterator.[[IteratedString]] to S.
  StoreObjectFieldNoWriteBarrier(
      iterator, JSRegExpStringIterator::kIteratedStringOffset, string);

#ifdef DEBUG
  // Verify global and full_unicode can be bitwise shifted without masking.
  TNode<Int32T> zero = Int32Constant(0);
  TNode<Int32T> one = Int32Constant(1);
  CSA_ASSERT(this,
             Word32Or(Word32Equal(global, zero), Word32Equal(global, one)));
  CSA_ASSERT(this, Word32Or(Word32Equal(full_unicode, zero),
                            Word32Equal(full_unicode, one)));
#endif  // DEBUG

  // 7. Set iterator.[[Global]] to global.
  // 8. Set iterator.[[Unicode]] to fullUnicode.
  // 9. Set iterator.[[Done]] to false.
  TNode<Word32T> global_flag =
      Word32Shl(global, Int32Constant(JSRegExpStringIterator::kGlobalBit));
  TNode<Word32T> unicode_flag = Word32Shl(
      full_unicode, Int32Constant(JSRegExpStringIterator::kUnicodeBit));
  TNode<Word32T> iterator_flags = Word32Or(global_flag, unicode_flag);
  StoreObjectFieldNoWriteBarrier(iterator, JSRegExpStringIterator::kFlagsOffset,
                                 SmiFromInt32(Signed(iterator_flags)));

  return iterator;
}

// https://tc39.github.io/proposal-string-matchall/
// RegExp.prototype [ @@matchAll ] ( string )
TF_BUILTIN(RegExpPrototypeMatchAll, RegExpMatchAllAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_string = CAST(Parameter(Descriptor::kString));
  Generate(context, native_context, receiver, maybe_string);
}

// Helper that skips a few initial checks. and assumes...
// 1) receiver is a "fast" RegExp
// 2) pattern is a string
TF_BUILTIN(RegExpMatchFast, RegExpBuiltinsAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<String> string = CAST(Parameter(Descriptor::kPattern));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  RegExpPrototypeMatchBody(context, receiver, string, true);
}

void RegExpBuiltinsAssembler::RegExpPrototypeSearchBodyFast(
    Node* const context, Node* const regexp, Node* const string) {
  CSA_ASSERT(this, IsFastRegExp(context, regexp));
  CSA_ASSERT(this, IsString(string));

  // Grab the initial value of last index.
  Node* const previous_last_index = FastLoadLastIndex(CAST(regexp));

  // Ensure last index is 0.
  FastStoreLastIndex(regexp, SmiZero());

  // Call exec.
  Label if_didnotmatch(this);
  TNode<RegExpMatchInfo> match_indices = RegExpPrototypeExecBodyWithoutResult(
      CAST(context), CAST(regexp), CAST(string), &if_didnotmatch, true);

  // Successful match.
  {
    // Reset last index.
    FastStoreLastIndex(regexp, previous_last_index);

    // Return the index of the match.
    Node* const index = LoadFixedArrayElement(
        match_indices, RegExpMatchInfo::kFirstCaptureIndex);
    Return(index);
  }

  BIND(&if_didnotmatch);
  {
    // Reset last index and return -1.
    FastStoreLastIndex(regexp, previous_last_index);
    Return(SmiConstant(-1));
  }
}

void RegExpBuiltinsAssembler::RegExpPrototypeSearchBodySlow(
    Node* const context, Node* const regexp, Node* const string) {
  CSA_ASSERT(this, IsJSReceiver(regexp));
  CSA_ASSERT(this, IsString(string));

  Isolate* const isolate = this->isolate();

  Node* const smi_zero = SmiZero();

  // Grab the initial value of last index.
  Node* const previous_last_index =
      SlowLoadLastIndex(CAST(context), CAST(regexp));

  // Ensure last index is 0.
  {
    Label next(this), slow(this, Label::kDeferred);
    BranchIfSameValue(previous_last_index, smi_zero, &next, &slow);

    BIND(&slow);
    SlowStoreLastIndex(context, regexp, smi_zero);
    Goto(&next);
    BIND(&next);
  }

  // Call exec.
  Node* const exec_result = RegExpExec(context, regexp, string);

  // Reset last index if necessary.
  {
    Label next(this), slow(this, Label::kDeferred);
    Node* const current_last_index =
        SlowLoadLastIndex(CAST(context), CAST(regexp));

    BranchIfSameValue(current_last_index, previous_last_index, &next, &slow);

    BIND(&slow);
    SlowStoreLastIndex(context, regexp, previous_last_index);
    Goto(&next);
    BIND(&next);
  }

  // Return -1 if no match was found.
  {
    Label next(this);
    GotoIfNot(IsNull(exec_result), &next);
    Return(SmiConstant(-1));
    BIND(&next);
  }

  // Return the index of the match.
  {
    Label fast_result(this), slow_result(this, Label::kDeferred);
    BranchIfFastRegExpResult(context, exec_result, &fast_result, &slow_result);

    BIND(&fast_result);
    {
      Node* const index =
          LoadObjectField(exec_result, JSRegExpResult::kIndexOffset);
      Return(index);
    }

    BIND(&slow_result);
    {
      Return(GetProperty(context, exec_result,
                         isolate->factory()->index_string()));
    }
  }
}

// ES#sec-regexp.prototype-@@search
// RegExp.prototype [ @@search ] ( string )
TF_BUILTIN(RegExpPrototypeSearch, RegExpBuiltinsAssembler) {
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> maybe_string = CAST(Parameter(Descriptor::kString));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Ensure {maybe_receiver} is a JSReceiver.
  ThrowIfNotJSReceiver(context, maybe_receiver,
                       MessageTemplate::kIncompatibleMethodReceiver,
                       "RegExp.prototype.@@search");
  Node* const receiver = maybe_receiver;

  // Convert {maybe_string} to a String.
  TNode<String> const string = ToString_Inline(context, maybe_string);

  Label fast_path(this), slow_path(this);
  BranchIfFastRegExp(context, receiver, &fast_path, &slow_path);

  BIND(&fast_path);
  // TODO(pwong): Could be optimized to remove the overhead of calling the
  //              builtin (at the cost of a larger builtin).
  Return(CallBuiltin(Builtins::kRegExpSearchFast, context, receiver, string));

  BIND(&slow_path);
  RegExpPrototypeSearchBodySlow(context, receiver, string);
}

// Helper that skips a few initial checks. and assumes...
// 1) receiver is a "fast" RegExp
// 2) pattern is a string
TF_BUILTIN(RegExpSearchFast, RegExpBuiltinsAssembler) {
  TNode<JSRegExp> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<String> string = CAST(Parameter(Descriptor::kPattern));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  RegExpPrototypeSearchBodyFast(context, receiver, string);
}

// Generates the fast path for @@split. {regexp} is an unmodified, non-sticky
// JSRegExp, {string} is a String, and {limit} is a Smi.
void RegExpBuiltinsAssembler::RegExpPrototypeSplitBody(Node* const context,
                                                       Node* const regexp,
                                                       TNode<String> string,
                                                       TNode<Smi> const limit) {
  CSA_ASSERT(this, IsFastRegExp(context, regexp));
  CSA_ASSERT(this,
             Word32BinaryNot(FastFlagGetter(CAST(regexp), JSRegExp::kSticky)));

  TNode<IntPtrT> const int_limit = SmiUntag(limit);

  const ElementsKind kind = PACKED_ELEMENTS;
  const ParameterMode mode = CodeStubAssembler::INTPTR_PARAMETERS;

  Node* const allocation_site = nullptr;
  Node* const native_context = LoadNativeContext(context);
  TNode<Map> array_map = LoadJSArrayElementsMap(kind, native_context);

  Label return_empty_array(this, Label::kDeferred);

  // If limit is zero, return an empty array.
  {
    Label next(this), if_limitiszero(this, Label::kDeferred);
    Branch(SmiEqual(limit, SmiZero()), &return_empty_array, &next);
    BIND(&next);
  }

  TNode<Smi> const string_length = LoadStringLengthAsSmi(string);

  // If passed the empty {string}, return either an empty array or a singleton
  // array depending on whether the {regexp} matches.
  {
    Label next(this), if_stringisempty(this, Label::kDeferred);
    Branch(SmiEqual(string_length, SmiZero()), &if_stringisempty, &next);

    BIND(&if_stringisempty);
    {
      Node* const last_match_info = LoadContextElement(
          native_context, Context::REGEXP_LAST_MATCH_INFO_INDEX);

      Node* const match_indices =
          CallBuiltin(Builtins::kRegExpExecInternal, context, regexp, string,
                      SmiZero(), last_match_info);

      Label return_singleton_array(this);
      Branch(IsNull(match_indices), &return_singleton_array,
             &return_empty_array);

      BIND(&return_singleton_array);
      {
        TNode<Smi> length = SmiConstant(1);
        TNode<IntPtrT> capacity = IntPtrConstant(1);
        TNode<JSArray> result = AllocateJSArray(kind, array_map, capacity,
                                                length, allocation_site, mode);

        TNode<FixedArray> fixed_array = CAST(LoadElements(result));
        UnsafeStoreFixedArrayElement(fixed_array, 0, string);

        Return(result);
      }
    }

    BIND(&next);
  }

  // Loop preparations.

  GrowableFixedArray array(state());

  TVARIABLE(Smi, var_last_matched_until, SmiZero());
  TVARIABLE(Smi, var_next_search_from, SmiZero());

  Variable* vars[] = {array.var_array(), array.var_length(),
                      array.var_capacity(), &var_last_matched_until,
                      &var_next_search_from};
  const int vars_count = sizeof(vars) / sizeof(vars[0]);
  Label loop(this, vars_count, vars), push_suffix_and_out(this), out(this);
  Goto(&loop);

  BIND(&loop);
  {
    TNode<Smi> const next_search_from = var_next_search_from.value();
    TNode<Smi> const last_matched_until = var_last_matched_until.value();

    // We're done if we've reached the end of the string.
    {
      Label next(this);
      Branch(SmiEqual(next_search_from, string_length), &push_suffix_and_out,
             &next);
      BIND(&next);
    }

    // Search for the given {regexp}.

    Node* const last_match_info = LoadContextElement(
        native_context, Context::REGEXP_LAST_MATCH_INFO_INDEX);

    TNode<HeapObject> const match_indices_ho =
        CAST(CallBuiltin(Builtins::kRegExpExecInternal, context, regexp, string,
                         next_search_from, last_match_info));

    // We're done if no match was found.
    {
      Label next(this);
      Branch(IsNull(match_indices_ho), &push_suffix_and_out, &next);
      BIND(&next);
    }

    TNode<FixedArray> match_indices = CAST(match_indices_ho);
    TNode<Smi> const match_from = CAST(UnsafeLoadFixedArrayElement(
        match_indices, RegExpMatchInfo::kFirstCaptureIndex));

    // We're done if the match starts beyond the string.
    {
      Label next(this);
      Branch(SmiEqual(match_from, string_length), &push_suffix_and_out, &next);
      BIND(&next);
    }

    TNode<Smi> const match_to = CAST(UnsafeLoadFixedArrayElement(
        match_indices, RegExpMatchInfo::kFirstCaptureIndex + 1));

    // Advance index and continue if the match is empty.
    {
      Label next(this);

      GotoIfNot(SmiEqual(match_to, next_search_from), &next);
      GotoIfNot(SmiEqual(match_to, last_matched_until), &next);

      Node* const is_unicode = FastFlagGetter(CAST(regexp), JSRegExp::kUnicode);
      Node* const new_next_search_from =
          AdvanceStringIndex(string, next_search_from, is_unicode, true);
      var_next_search_from = CAST(new_next_search_from);
      Goto(&loop);

      BIND(&next);
    }

    // A valid match was found, add the new substring to the array.
    {
      TNode<Smi> const from = last_matched_until;
      TNode<Smi> const to = match_from;
      array.Push(CallBuiltin(Builtins::kSubString, context, string, from, to));
      GotoIf(WordEqual(array.length(), int_limit), &out);
    }

    // Add all captures to the array.
    {
      Node* const num_registers = LoadFixedArrayElement(
          match_indices, RegExpMatchInfo::kNumberOfCapturesIndex);
      Node* const int_num_registers = SmiUntag(num_registers);

      VARIABLE(var_reg, MachineType::PointerRepresentation());
      var_reg.Bind(IntPtrConstant(2));

      Variable* vars[] = {array.var_array(), array.var_length(),
                          array.var_capacity(), &var_reg};
      const int vars_count = sizeof(vars) / sizeof(vars[0]);
      Label nested_loop(this, vars_count, vars), nested_loop_out(this);
      Branch(IntPtrLessThan(var_reg.value(), int_num_registers), &nested_loop,
             &nested_loop_out);

      BIND(&nested_loop);
      {
        Node* const reg = var_reg.value();
        Node* const from = LoadFixedArrayElement(
            match_indices, reg,
            RegExpMatchInfo::kFirstCaptureIndex * kTaggedSize, mode);
        TNode<Smi> const to = CAST(LoadFixedArrayElement(
            match_indices, reg,
            (RegExpMatchInfo::kFirstCaptureIndex + 1) * kTaggedSize, mode));

        Label select_capture(this), select_undefined(this), store_value(this);
        VARIABLE(var_value, MachineRepresentation::kTagged);
        Branch(SmiEqual(to, SmiConstant(-1)), &select_undefined,
               &select_capture);

        BIND(&select_capture);
        {
          var_value.Bind(
              CallBuiltin(Builtins::kSubString, context, string, from, to));
          Goto(&store_value);
        }

        BIND(&select_undefined);
        {
          var_value.Bind(UndefinedConstant());
          Goto(&store_value);
        }

        BIND(&store_value);
        {
          array.Push(CAST(var_value.value()));
          GotoIf(WordEqual(array.length(), int_limit), &out);

          Node* const new_reg = IntPtrAdd(reg, IntPtrConstant(2));
          var_reg.Bind(new_reg);

          Branch(IntPtrLessThan(new_reg, int_num_registers), &nested_loop,
                 &nested_loop_out);
        }
      }

      BIND(&nested_loop_out);
    }

    var_last_matched_until = match_to;
    var_next_search_from = match_to;
    Goto(&loop);
  }

  BIND(&push_suffix_and_out);
  {
    Node* const from = var_last_matched_until.value();
    Node* const to = string_length;
    array.Push(CallBuiltin(Builtins::kSubString, context, string, from, to));
    Goto(&out);
  }

  BIND(&out);
  {
    Node* const result = array.ToJSArray(CAST(context));
    Return(result);
  }

  BIND(&return_empty_array);
  {
    TNode<Smi> length = SmiZero();
    TNode<IntPtrT> capacity = IntPtrZero();
    TNode<JSArray> result = AllocateJSArray(kind, array_map, capacity, length,
                                            allocation_site, mode);
    Return(result);
  }
}

// Helper that skips a few initial checks.
TF_BUILTIN(RegExpSplit, RegExpBuiltinsAssembler) {
  TNode<JSRegExp> regexp = CAST(Parameter(Descriptor::kRegExp));
  TNode<String> string = CAST(Parameter(Descriptor::kString));
  TNode<Object> maybe_limit = CAST(Parameter(Descriptor::kLimit));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CSA_ASSERT(this, IsFastRegExp(context, regexp));

  // TODO(jgruber): Even if map checks send us to the fast path, we still need
  // to verify the constructor property and jump to the slow path if it has
  // been changed.

  // Verify {maybe_limit}.

  VARIABLE(var_limit, MachineRepresentation::kTagged, maybe_limit);
  Label if_limitissmimax(this), runtime(this, Label::kDeferred);

  {
    Label next(this);

    GotoIf(IsUndefined(maybe_limit), &if_limitissmimax);
    Branch(TaggedIsPositiveSmi(maybe_limit), &next, &runtime);

    // We need to be extra-strict and require the given limit to be either
    // undefined or a positive smi. We can't call ToUint32(maybe_limit) since
    // that might move us onto the slow path, resulting in ordering spec
    // violations (see https://crbug.com/801171).

    BIND(&if_limitissmimax);
    {
      // TODO(jgruber): In this case, we can probably avoid generation of limit
      // checks in Generate_RegExpPrototypeSplitBody.
      var_limit.Bind(SmiConstant(Smi::kMaxValue));
      Goto(&next);
    }

    BIND(&next);
  }

  // Due to specific shortcuts we take on the fast path (specifically, we don't
  // allocate a new regexp instance as specced), we need to ensure that the
  // given regexp is non-sticky to avoid invalid results. See crbug.com/v8/6706.

  GotoIf(FastFlagGetter(regexp, JSRegExp::kSticky), &runtime);

  // We're good to go on the fast path, which is inlined here.

  RegExpPrototypeSplitBody(context, regexp, string, CAST(var_limit.value()));

  BIND(&runtime);
  Return(CallRuntime(Runtime::kRegExpSplit, context, regexp, string,
                     var_limit.value()));
}

// ES#sec-regexp.prototype-@@split
// RegExp.prototype [ @@split ] ( string, limit )
TF_BUILTIN(RegExpPrototypeSplit, RegExpBuiltinsAssembler) {
  const int kStringArg = 0;
  const int kLimitArg = 1;

  TNode<IntPtrT> argc =
      ChangeInt32ToIntPtr(Parameter(Descriptor::kJSActualArgumentsCount));
  CodeStubArguments args(this, argc);

  TNode<Object> maybe_receiver = args.GetReceiver();
  TNode<Object> maybe_string = args.GetOptionalArgumentValue(kStringArg);
  TNode<Object> maybe_limit = args.GetOptionalArgumentValue(kLimitArg);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  // Ensure {maybe_receiver} is a JSReceiver.
  ThrowIfNotJSReceiver(context, maybe_receiver,
                       MessageTemplate::kIncompatibleMethodReceiver,
                       "RegExp.prototype.@@split");
  Node* const receiver = maybe_receiver;

  // Convert {maybe_string} to a String.
  TNode<String> const string = ToString_Inline(context, maybe_string);

  Label stub(this), runtime(this, Label::kDeferred);
  BranchIfFastRegExp(context, receiver, &stub, &runtime);

  BIND(&stub);
  args.PopAndReturn(CallBuiltin(Builtins::kRegExpSplit, context, receiver,
                                string, maybe_limit));

  BIND(&runtime);
  args.PopAndReturn(CallRuntime(Runtime::kRegExpSplit, context, receiver,
                                string, maybe_limit));
}

class RegExpStringIteratorAssembler : public RegExpBuiltinsAssembler {
 public:
  explicit RegExpStringIteratorAssembler(compiler::CodeAssemblerState* state)
      : RegExpBuiltinsAssembler(state) {}

 protected:
  TNode<Smi> LoadFlags(TNode<HeapObject> iterator) {
    return LoadObjectField<Smi>(iterator, JSRegExpStringIterator::kFlagsOffset);
  }

  TNode<BoolT> HasDoneFlag(TNode<Smi> flags) {
    return UncheckedCast<BoolT>(
        IsSetSmi(flags, 1 << JSRegExpStringIterator::kDoneBit));
  }

  TNode<BoolT> HasGlobalFlag(TNode<Smi> flags) {
    return UncheckedCast<BoolT>(
        IsSetSmi(flags, 1 << JSRegExpStringIterator::kGlobalBit));
  }

  TNode<BoolT> HasUnicodeFlag(TNode<Smi> flags) {
    return UncheckedCast<BoolT>(
        IsSetSmi(flags, 1 << JSRegExpStringIterator::kUnicodeBit));
  }

  void SetDoneFlag(TNode<HeapObject> iterator, TNode<Smi> flags) {
    TNode<Smi> new_flags =
        SmiOr(flags, SmiConstant(1 << JSRegExpStringIterator::kDoneBit));
    StoreObjectFieldNoWriteBarrier(
        iterator, JSRegExpStringIterator::kFlagsOffset, new_flags);
  }
};

// https://tc39.github.io/proposal-string-matchall/
// %RegExpStringIteratorPrototype%.next ( )
TF_BUILTIN(RegExpStringIteratorPrototypeNext, RegExpStringIteratorAssembler) {
  const char* method_name = "%RegExpStringIterator%.prototype.next";
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> maybe_receiver = CAST(Parameter(Descriptor::kReceiver));

  Label if_match(this), if_no_match(this, Label::kDeferred),
      return_empty_done_result(this, Label::kDeferred);

  // 1. Let O be the this value.
  // 2. If Type(O) is not Object, throw a TypeError exception.
  // 3. If O does not have all of the internal slots of a RegExp String Iterator
  // Object Instance (see 5.3), throw a TypeError exception.
  ThrowIfNotInstanceType(context, maybe_receiver,
                         JS_REGEXP_STRING_ITERATOR_TYPE, method_name);
  TNode<HeapObject> receiver = CAST(maybe_receiver);

  // 4. If O.[[Done]] is true, then
  //   a. Return ! CreateIterResultObject(undefined, true).
  TNode<Smi> flags = LoadFlags(receiver);
  GotoIf(HasDoneFlag(flags), &return_empty_done_result);

  // 5. Let R be O.[[IteratingRegExp]].
  TNode<Object> iterating_regexp =
      LoadObjectField(receiver, JSRegExpStringIterator::kIteratingRegExpOffset);

  // TODO(jgruber): Verify that this is guaranteed.
  CSA_CHECK(this, TaggedIsNotSmi(iterating_regexp));
  CSA_CHECK(this, IsJSReceiver(CAST(iterating_regexp)));

  // 6. Let S be O.[[IteratedString]].
  TNode<String> iterating_string = CAST(
      LoadObjectField(receiver, JSRegExpStringIterator::kIteratedStringOffset));

  // 7. Let global be O.[[Global]].
  // See if_match.

  // 8. Let fullUnicode be O.[[Unicode]].
  // See if_global.

  // 9. Let match be ? RegExpExec(R, S).
  TVARIABLE(Object, var_match);
  TVARIABLE(BoolT, var_is_fast_regexp);
  {
    Label if_fast(this), if_slow(this, Label::kDeferred);
    BranchIfFastRegExp(context, iterating_regexp, &if_fast, &if_slow);

    BIND(&if_fast);
    {
      TNode<RegExpMatchInfo> match_indices =
          RegExpPrototypeExecBodyWithoutResult(context, CAST(iterating_regexp),
                                               iterating_string, &if_no_match,
                                               true);
      var_match = ConstructNewResultFromMatchInfo(
          context, CAST(iterating_regexp), match_indices, iterating_string);
      var_is_fast_regexp = Int32TrueConstant();
      Goto(&if_match);
    }

    BIND(&if_slow);
    {
      var_match = CAST(RegExpExec(context, iterating_regexp, iterating_string));
      var_is_fast_regexp = Int32FalseConstant();
      Branch(IsNull(var_match.value()), &if_no_match, &if_match);
    }
  }

  // 10. If match is null, then
  BIND(&if_no_match);
  {
    // a. Set O.[[Done]] to true.
    SetDoneFlag(receiver, flags);

    // b. Return ! CreateIterResultObject(undefined, true).
    Goto(&return_empty_done_result);
  }
  // 11. Else,
  BIND(&if_match);
  {
    Label if_global(this), if_not_global(this, Label::kDeferred),
        return_result(this);

    // a. If global is true,
    Branch(HasGlobalFlag(flags), &if_global, &if_not_global);
    BIND(&if_global);
    {
      Label if_fast(this), if_slow(this, Label::kDeferred);

      // ii. If matchStr is the empty string,
      Branch(var_is_fast_regexp.value(), &if_fast, &if_slow);
      BIND(&if_fast);
      {
        // i. Let matchStr be ? ToString(? Get(match, "0")).
        CSA_ASSERT_BRANCH(this, [&](Label* ok, Label* not_ok) {
          BranchIfFastRegExpResult(context, var_match.value(), ok, not_ok);
        });
        CSA_ASSERT(this,
                   SmiNotEqual(LoadFastJSArrayLength(CAST(var_match.value())),
                               SmiZero()));
        TNode<FixedArray> result_fixed_array =
            CAST(LoadElements(CAST(var_match.value())));
        TNode<String> match_str =
            CAST(LoadFixedArrayElement(result_fixed_array, 0));

        // When iterating_regexp is fast, we assume it stays fast even after
        // accessing the first match from the RegExp result.
        CSA_ASSERT(this, IsFastRegExp(context, iterating_regexp));
        GotoIfNot(IsEmptyString(match_str), &return_result);

        // 1. Let thisIndex be ? ToLength(? Get(R, "lastIndex")).
        TNode<Smi> this_index = CAST(FastLoadLastIndex(CAST(iterating_regexp)));
        CSA_ASSERT(this, TaggedIsSmi(this_index));

        // 2. Let nextIndex be ! AdvanceStringIndex(S, thisIndex, fullUnicode).
        TNode<Smi> next_index = CAST(AdvanceStringIndex(
            iterating_string, this_index, HasUnicodeFlag(flags), true));
        CSA_ASSERT(this, TaggedIsSmi(next_index));

        // 3. Perform ? Set(R, "lastIndex", nextIndex, true).
        FastStoreLastIndex(iterating_regexp, next_index);

        // iii. Return ! CreateIterResultObject(match, false).
        Goto(&return_result);
      }
      BIND(&if_slow);
      {
        // i. Let matchStr be ? ToString(? Get(match, "0")).
        TNode<String> match_str = ToString_Inline(
            context, GetProperty(context, var_match.value(), SmiZero()));

        GotoIfNot(IsEmptyString(match_str), &return_result);

        // 1. Let thisIndex be ? ToLength(? Get(R, "lastIndex")).
        TNode<Object> last_index = SlowLoadLastIndex(context, iterating_regexp);
        TNode<Number> this_index = ToLength_Inline(context, last_index);

        // 2. Let nextIndex be ! AdvanceStringIndex(S, thisIndex, fullUnicode).
        TNode<Object> next_index = CAST(AdvanceStringIndex(
            iterating_string, this_index, HasUnicodeFlag(flags), false));

        // 3. Perform ? Set(R, "lastIndex", nextIndex, true).
        SlowStoreLastIndex(context, iterating_regexp, next_index);

        // iii. Return ! CreateIterResultObject(match, false).
        Goto(&return_result);
      }
    }
    // b. Else,
    BIND(&if_not_global);
    {
      // i. Set O.[[Done]] to true.
      SetDoneFlag(receiver, flags);

      // ii. Return ! CreateIterResultObject(match, false).
      Goto(&return_result);
    }
    BIND(&return_result);
    {
      Return(AllocateJSIteratorResult(context, var_match.value(),
                                      FalseConstant()));
    }
  }
  BIND(&return_empty_done_result);
  Return(
      AllocateJSIteratorResult(context, UndefinedConstant(), TrueConstant()));
}

}  // namespace internal
}  // namespace v8
