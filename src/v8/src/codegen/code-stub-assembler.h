// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_CODE_STUB_ASSEMBLER_H_
#define V8_CODEGEN_CODE_STUB_ASSEMBLER_H_

#include <functional>

#include "src/base/macros.h"
#include "src/codegen/bailout-reason.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/compiler/code-assembler.h"
#include "src/objects/arguments.h"
#include "src/objects/bigint.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"

#include "torque-generated/exported-macros-assembler-tq.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class CodeStubArguments;
class CodeStubAssembler;
class StatsCounter;
class StubCache;

enum class PrimitiveType { kBoolean, kNumber, kString, kSymbol };

#define HEAP_MUTABLE_IMMOVABLE_OBJECT_LIST(V)                              \
  V(ArraySpeciesProtector, array_species_protector, ArraySpeciesProtector) \
  V(PromiseSpeciesProtector, promise_species_protector,                    \
    PromiseSpeciesProtector)                                               \
  V(TypedArraySpeciesProtector, typed_array_species_protector,             \
    TypedArraySpeciesProtector)                                            \

#define HEAP_IMMUTABLE_IMMOVABLE_OBJECT_LIST(V)                                \
  V(AccessorInfoMap, accessor_info_map, AccessorInfoMap)                       \
  V(AccessorPairMap, accessor_pair_map, AccessorPairMap)                       \
  V(AllocationSiteWithWeakNextMap, allocation_site_map, AllocationSiteMap)     \
  V(AllocationSiteWithoutWeakNextMap, allocation_site_without_weaknext_map,    \
    AllocationSiteWithoutWeakNextMap)                                          \
  V(BooleanMap, boolean_map, BooleanMap)                                       \
  V(CodeMap, code_map, CodeMap)                                                \
  V(EmptyFixedArray, empty_fixed_array, EmptyFixedArray)                       \
  V(EmptyPropertyDictionary, empty_property_dictionary,                        \
    EmptyPropertyDictionary)                                                   \
  V(EmptySlowElementDictionary, empty_slow_element_dictionary,                 \
    EmptySlowElementDictionary)                                                \
  V(empty_string, empty_string, EmptyString)                                   \
  V(FalseValue, false_value, False)                                            \
  V(FeedbackVectorMap, feedback_vector_map, FeedbackVectorMap)                 \
  V(FixedArrayMap, fixed_array_map, FixedArrayMap)                             \
  V(FixedCOWArrayMap, fixed_cow_array_map, FixedCOWArrayMap)                   \
  V(FixedDoubleArrayMap, fixed_double_array_map, FixedDoubleArrayMap)          \
  V(FunctionTemplateInfoMap, function_template_info_map,                       \
    FunctionTemplateInfoMap)                                                   \
  V(GlobalPropertyCellMap, global_property_cell_map, PropertyCellMap)          \
  V(has_instance_symbol, has_instance_symbol, HasInstanceSymbol)               \
  V(HeapNumberMap, heap_number_map, HeapNumberMap)                             \
  V(iterator_symbol, iterator_symbol, IteratorSymbol)                          \
  V(length_string, length_string, LengthString)                                \
  V(ManyClosuresCellMap, many_closures_cell_map, ManyClosuresCellMap)          \
  V(MetaMap, meta_map, MetaMap)                                                \
  V(MinusZeroValue, minus_zero_value, MinusZero)                               \
  V(MutableHeapNumberMap, mutable_heap_number_map, MutableHeapNumberMap)       \
  V(NanValue, nan_value, Nan)                                                  \
  V(NoClosuresCellMap, no_closures_cell_map, NoClosuresCellMap)                \
  V(NullValue, null_value, Null)                                               \
  V(OneClosureCellMap, one_closure_cell_map, OneClosureCellMap)                \
  V(PreparseDataMap, preparse_data_map, PreparseDataMap)                       \
  V(prototype_string, prototype_string, PrototypeString)                       \
  V(SharedFunctionInfoMap, shared_function_info_map, SharedFunctionInfoMap)    \
  V(StoreHandler0Map, store_handler0_map, StoreHandler0Map)                    \
  V(SymbolMap, symbol_map, SymbolMap)                                          \
  V(TheHoleValue, the_hole_value, TheHole)                                     \
  V(TransitionArrayMap, transition_array_map, TransitionArrayMap)              \
  V(TrueValue, true_value, True)                                               \
  V(Tuple2Map, tuple2_map, Tuple2Map)                                          \
  V(Tuple3Map, tuple3_map, Tuple3Map)                                          \
  V(ArrayBoilerplateDescriptionMap, array_boilerplate_description_map,         \
    ArrayBoilerplateDescriptionMap)                                            \
  V(UncompiledDataWithoutPreparseDataMap,                                      \
    uncompiled_data_without_preparse_data_map,                                 \
    UncompiledDataWithoutPreparseDataMap)                                      \
  V(UncompiledDataWithPreparseDataMap, uncompiled_data_with_preparse_data_map, \
    UncompiledDataWithPreparseDataMap)                                         \
  V(UndefinedValue, undefined_value, Undefined)                                \
  V(WeakFixedArrayMap, weak_fixed_array_map, WeakFixedArrayMap)

#define HEAP_IMMOVABLE_OBJECT_LIST(V)   \
  HEAP_MUTABLE_IMMOVABLE_OBJECT_LIST(V) \
  HEAP_IMMUTABLE_IMMOVABLE_OBJECT_LIST(V)

#ifdef DEBUG
#define CSA_CHECK(csa, x)                                        \
  (csa)->Check(                                                  \
      [&]() -> compiler::Node* {                                 \
        return implicit_cast<compiler::SloppyTNode<Word32T>>(x); \
      },                                                         \
      #x, __FILE__, __LINE__)
#else
#define CSA_CHECK(csa, x) (csa)->FastCheck(x)
#endif

#ifdef DEBUG
// CSA_ASSERT_ARGS generates an
// std::initializer_list<CodeStubAssembler::ExtraNode> from __VA_ARGS__. It
// currently supports between 0 and 2 arguments.

// clang-format off
#define CSA_ASSERT_0_ARGS(...) {}
#define CSA_ASSERT_1_ARG(a, ...) {{a, #a}}
#define CSA_ASSERT_2_ARGS(a, b, ...) {{a, #a}, {b, #b}}
// clang-format on
#define SWITCH_CSA_ASSERT_ARGS(dummy, a, b, FUNC, ...) FUNC(a, b)
#define CSA_ASSERT_ARGS(...)                                      \
  SWITCH_CSA_ASSERT_ARGS(dummy, ##__VA_ARGS__, CSA_ASSERT_2_ARGS, \
                         CSA_ASSERT_1_ARG, CSA_ASSERT_0_ARGS)

// CSA_ASSERT(csa, <condition>, <extra values to print...>)

#define CSA_ASSERT(csa, condition_node, ...)                                  \
  (csa)->Assert(                                                              \
      [&]() -> compiler::Node* {                                              \
        return implicit_cast<compiler::SloppyTNode<Word32T>>(condition_node); \
      },                                                                      \
      #condition_node, __FILE__, __LINE__, CSA_ASSERT_ARGS(__VA_ARGS__))

// CSA_ASSERT_BRANCH(csa, [](Label* ok, Label* not_ok) {...},
//     <extra values to print...>)

#define CSA_ASSERT_BRANCH(csa, gen, ...) \
  (csa)->Assert(gen, #gen, __FILE__, __LINE__, CSA_ASSERT_ARGS(__VA_ARGS__))

#define CSA_ASSERT_JS_ARGC_OP(csa, Op, op, expected)                         \
  (csa)->Assert(                                                             \
      [&]() -> compiler::Node* {                                             \
        compiler::Node* const argc =                                         \
            (csa)->Parameter(Descriptor::kJSActualArgumentsCount);           \
        return (csa)->Op(argc, (csa)->Int32Constant(expected));              \
      },                                                                     \
      "argc " #op " " #expected, __FILE__, __LINE__,                         \
      {{SmiFromInt32((csa)->Parameter(Descriptor::kJSActualArgumentsCount)), \
        "argc"}})

#define CSA_ASSERT_JS_ARGC_EQ(csa, expected) \
  CSA_ASSERT_JS_ARGC_OP(csa, Word32Equal, ==, expected)

#define CSA_DEBUG_INFO(name) \
  { #name, __FILE__, __LINE__ }
#define BIND(label) Bind(label, CSA_DEBUG_INFO(label))
#define VARIABLE(name, ...) \
  Variable name(this, CSA_DEBUG_INFO(name), __VA_ARGS__)
#define VARIABLE_CONSTRUCTOR(name, ...) \
  name(this, CSA_DEBUG_INFO(name), __VA_ARGS__)
#define TYPED_VARIABLE_DEF(type, name, ...) \
  TVariable<type> name(CSA_DEBUG_INFO(name), __VA_ARGS__)
#else  // DEBUG
#define CSA_ASSERT(csa, ...) ((void)0)
#define CSA_ASSERT_BRANCH(csa, ...) ((void)0)
#define CSA_ASSERT_JS_ARGC_EQ(csa, expected) ((void)0)
#define BIND(label) Bind(label)
#define VARIABLE(name, ...) Variable name(this, __VA_ARGS__)
#define VARIABLE_CONSTRUCTOR(name, ...) name(this, __VA_ARGS__)
#define TYPED_VARIABLE_DEF(type, name, ...) TVariable<type> name(__VA_ARGS__)
#endif  // DEBUG

#define TVARIABLE(...) EXPAND(TYPED_VARIABLE_DEF(__VA_ARGS__, this))

#ifdef ENABLE_SLOW_DCHECKS
#define CSA_SLOW_ASSERT(csa, ...) \
  if (FLAG_enable_slow_asserts) { \
    CSA_ASSERT(csa, __VA_ARGS__); \
  }
#else
#define CSA_SLOW_ASSERT(csa, ...) ((void)0)
#endif

// Provides JavaScript-specific "macro-assembler" functionality on top of the
// CodeAssembler. By factoring the JavaScript-isms out of the CodeAssembler,
// it's possible to add JavaScript-specific useful CodeAssembler "macros"
// without modifying files in the compiler directory (and requiring a review
// from a compiler directory OWNER).
class V8_EXPORT_PRIVATE CodeStubAssembler
    : public compiler::CodeAssembler,
      public TorqueGeneratedExportedMacrosAssembler {
 public:
  using Node = compiler::Node;
  template <class T>
  using TNode = compiler::TNode<T>;
  template <class T>
  using SloppyTNode = compiler::SloppyTNode<T>;

  template <typename T>
  using LazyNode = std::function<TNode<T>()>;

  explicit CodeStubAssembler(compiler::CodeAssemblerState* state);

  enum AllocationFlag : uint8_t {
    kNone = 0,
    kDoubleAlignment = 1,
    kPretenured = 1 << 1,
    kAllowLargeObjectAllocation = 1 << 2,
  };

  enum SlackTrackingMode { kWithSlackTracking, kNoSlackTracking };

  using AllocationFlags = base::Flags<AllocationFlag>;

  enum ParameterMode { SMI_PARAMETERS, INTPTR_PARAMETERS };

  // On 32-bit platforms, there is a slight performance advantage to doing all
  // of the array offset/index arithmetic with SMIs, since it's possible
  // to save a few tag/untag operations without paying an extra expense when
  // calculating array offset (the smi math can be folded away) and there are
  // fewer live ranges. Thus only convert indices to untagged value on 64-bit
  // platforms.
  ParameterMode OptimalParameterMode() const {
    return Is64() ? INTPTR_PARAMETERS : SMI_PARAMETERS;
  }

  MachineRepresentation ParameterRepresentation(ParameterMode mode) const {
    return mode == INTPTR_PARAMETERS ? MachineType::PointerRepresentation()
                                     : MachineRepresentation::kTaggedSigned;
  }

  MachineRepresentation OptimalParameterRepresentation() const {
    return ParameterRepresentation(OptimalParameterMode());
  }

  TNode<IntPtrT> ParameterToIntPtr(Node* value, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) value = SmiUntag(value);
    return UncheckedCast<IntPtrT>(value);
  }

  Node* IntPtrToParameter(SloppyTNode<IntPtrT> value, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) return SmiTag(value);
    return value;
  }

  Node* Int32ToParameter(SloppyTNode<Int32T> value, ParameterMode mode) {
    return IntPtrToParameter(ChangeInt32ToIntPtr(value), mode);
  }

  TNode<Smi> ParameterToTagged(Node* value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) return SmiTag(value);
    return UncheckedCast<Smi>(value);
  }

  Node* TaggedToParameter(SloppyTNode<Smi> value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) return SmiUntag(value);
    return value;
  }

  bool ToParameterConstant(Node* node, intptr_t* out, ParameterMode mode) {
    if (mode == ParameterMode::SMI_PARAMETERS) {
      Smi constant;
      if (ToSmiConstant(node, &constant)) {
        *out = static_cast<intptr_t>(constant.value());
        return true;
      }
    } else {
      DCHECK_EQ(mode, ParameterMode::INTPTR_PARAMETERS);
      intptr_t constant;
      if (ToIntPtrConstant(node, constant)) {
        *out = constant;
        return true;
      }
    }

    return false;
  }

#if defined(V8_HOST_ARCH_32_BIT)
  TNode<Smi> BIntToSmi(TNode<BInt> source) { return source; }
  TNode<IntPtrT> BIntToIntPtr(TNode<BInt> source) {
    return SmiToIntPtr(source);
  }
  TNode<BInt> SmiToBInt(TNode<Smi> source) { return source; }
  TNode<BInt> IntPtrToBInt(TNode<IntPtrT> source) {
    return SmiFromIntPtr(source);
  }
#elif defined(V8_HOST_ARCH_64_BIT)
  TNode<Smi> BIntToSmi(TNode<BInt> source) { return SmiFromIntPtr(source); }
  TNode<IntPtrT> BIntToIntPtr(TNode<BInt> source) { return source; }
  TNode<BInt> SmiToBInt(TNode<Smi> source) { return SmiToIntPtr(source); }
  TNode<BInt> IntPtrToBInt(TNode<IntPtrT> source) { return source; }
#else
#error Unknown architecture.
#endif

  TNode<Smi> TaggedToSmi(TNode<Object> value, Label* fail) {
    GotoIf(TaggedIsNotSmi(value), fail);
    return UncheckedCast<Smi>(value);
  }

  TNode<Smi> TaggedToPositiveSmi(TNode<Object> value, Label* fail) {
    GotoIfNot(TaggedIsPositiveSmi(value), fail);
    return UncheckedCast<Smi>(value);
  }

  TNode<String> TaggedToDirectString(TNode<Object> value, Label* fail);

  TNode<Number> TaggedToNumber(TNode<Object> value, Label* fail) {
    GotoIfNot(IsNumber(value), fail);
    return UncheckedCast<Number>(value);
  }

  TNode<HeapObject> TaggedToHeapObject(TNode<Object> value, Label* fail) {
    GotoIf(TaggedIsSmi(value), fail);
    return UncheckedCast<HeapObject>(value);
  }

  TNode<JSArray> HeapObjectToJSArray(TNode<HeapObject> heap_object,
                                     Label* fail) {
    GotoIfNot(IsJSArray(heap_object), fail);
    return UncheckedCast<JSArray>(heap_object);
  }

  TNode<JSArrayBuffer> HeapObjectToJSArrayBuffer(TNode<HeapObject> heap_object,
                                                 Label* fail) {
    GotoIfNot(IsJSArrayBuffer(heap_object), fail);
    return UncheckedCast<JSArrayBuffer>(heap_object);
  }

  TNode<JSArray> TaggedToFastJSArray(TNode<Context> context,
                                     TNode<Object> value, Label* fail) {
    GotoIf(TaggedIsSmi(value), fail);
    TNode<HeapObject> heap_object = CAST(value);
    GotoIfNot(IsFastJSArray(heap_object, context), fail);
    return UncheckedCast<JSArray>(heap_object);
  }

  TNode<JSDataView> HeapObjectToJSDataView(TNode<HeapObject> heap_object,
                                           Label* fail) {
    GotoIfNot(IsJSDataView(heap_object), fail);
    return CAST(heap_object);
  }

  TNode<JSProxy> HeapObjectToJSProxy(TNode<HeapObject> heap_object,
                                     Label* fail) {
    GotoIfNot(IsJSProxy(heap_object), fail);
    return CAST(heap_object);
  }

  TNode<JSStringIterator> HeapObjectToJSStringIterator(
      TNode<HeapObject> heap_object, Label* fail) {
    GotoIfNot(IsJSStringIterator(heap_object), fail);
    return CAST(heap_object);
  }

  TNode<JSReceiver> HeapObjectToCallable(TNode<HeapObject> heap_object,
                                         Label* fail) {
    GotoIfNot(IsCallable(heap_object), fail);
    return CAST(heap_object);
  }

  TNode<String> HeapObjectToString(TNode<HeapObject> heap_object, Label* fail) {
    GotoIfNot(IsString(heap_object), fail);
    return CAST(heap_object);
  }

  TNode<JSReceiver> HeapObjectToConstructor(TNode<HeapObject> heap_object,
                                            Label* fail) {
    GotoIfNot(IsConstructor(heap_object), fail);
    return CAST(heap_object);
  }

  Node* MatchesParameterMode(Node* value, ParameterMode mode);

#define PARAMETER_BINOP(OpName, IntPtrOpName, SmiOpName) \
  Node* OpName(Node* a, Node* b, ParameterMode mode) {   \
    if (mode == SMI_PARAMETERS) {                        \
      return SmiOpName(CAST(a), CAST(b));                \
    } else {                                             \
      DCHECK_EQ(INTPTR_PARAMETERS, mode);                \
      return IntPtrOpName(a, b);                         \
    }                                                    \
  }
  PARAMETER_BINOP(IntPtrOrSmiMin, IntPtrMin, SmiMin)
  PARAMETER_BINOP(IntPtrOrSmiAdd, IntPtrAdd, SmiAdd)
  PARAMETER_BINOP(IntPtrOrSmiSub, IntPtrSub, SmiSub)
  PARAMETER_BINOP(IntPtrOrSmiLessThan, IntPtrLessThan, SmiLessThan)
  PARAMETER_BINOP(IntPtrOrSmiLessThanOrEqual, IntPtrLessThanOrEqual,
                  SmiLessThanOrEqual)
  PARAMETER_BINOP(IntPtrOrSmiGreaterThan, IntPtrGreaterThan, SmiGreaterThan)
  PARAMETER_BINOP(IntPtrOrSmiGreaterThanOrEqual, IntPtrGreaterThanOrEqual,
                  SmiGreaterThanOrEqual)
  PARAMETER_BINOP(UintPtrOrSmiLessThan, UintPtrLessThan, SmiBelow)
  PARAMETER_BINOP(UintPtrOrSmiGreaterThanOrEqual, UintPtrGreaterThanOrEqual,
                  SmiAboveOrEqual)
#undef PARAMETER_BINOP

  uintptr_t ConstexprUintPtrShl(uintptr_t a, int32_t b) { return a << b; }
  uintptr_t ConstexprUintPtrShr(uintptr_t a, int32_t b) { return a >> b; }
  intptr_t ConstexprIntPtrAdd(intptr_t a, intptr_t b) { return a + b; }
  uintptr_t ConstexprUintPtrAdd(uintptr_t a, uintptr_t b) { return a + b; }
  intptr_t ConstexprWordNot(intptr_t a) { return ~a; }
  uintptr_t ConstexprWordNot(uintptr_t a) { return ~a; }

  TNode<Object> NoContextConstant();

#define HEAP_CONSTANT_ACCESSOR(rootIndexName, rootAccessorName, name)  \
  compiler::TNode<std::remove_pointer<std::remove_reference<decltype(  \
      std::declval<ReadOnlyRoots>().rootAccessorName())>::type>::type> \
      name##Constant();
  HEAP_IMMUTABLE_IMMOVABLE_OBJECT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_ACCESSOR(rootIndexName, rootAccessorName, name) \
  compiler::TNode<std::remove_pointer<std::remove_reference<decltype( \
      std::declval<Heap>().rootAccessorName())>::type>::type>         \
      name##Constant();
  HEAP_MUTABLE_IMMOVABLE_OBJECT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootIndexName, rootAccessorName, name) \
  TNode<BoolT> Is##name(SloppyTNode<Object> value);               \
  TNode<BoolT> IsNot##name(SloppyTNode<Object> value);
  HEAP_IMMOVABLE_OBJECT_LIST(HEAP_CONSTANT_TEST)
#undef HEAP_CONSTANT_TEST

  Node* IntPtrOrSmiConstant(int value, ParameterMode mode);

  bool IsIntPtrOrSmiConstantZero(Node* test, ParameterMode mode);
  bool TryGetIntPtrOrSmiConstantValue(Node* maybe_constant, int* value,
                                      ParameterMode mode);

  // Round the 32bits payload of the provided word up to the next power of two.
  TNode<IntPtrT> IntPtrRoundUpToPowerOfTwo32(TNode<IntPtrT> value);
  // Select the maximum of the two provided IntPtr values.
  TNode<IntPtrT> IntPtrMax(SloppyTNode<IntPtrT> left,
                           SloppyTNode<IntPtrT> right);
  // Select the minimum of the two provided IntPtr values.
  TNode<IntPtrT> IntPtrMin(SloppyTNode<IntPtrT> left,
                           SloppyTNode<IntPtrT> right);

  // Float64 operations.
  TNode<Float64T> Float64Ceil(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64Floor(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64Round(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64RoundToEven(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64Trunc(SloppyTNode<Float64T> x);
  // Select the minimum of the two provided Number values.
  TNode<Number> NumberMax(SloppyTNode<Number> left, SloppyTNode<Number> right);
  // Select the minimum of the two provided Number values.
  TNode<Number> NumberMin(SloppyTNode<Number> left, SloppyTNode<Number> right);

  // After converting an index to an integer, calculate a relative index: if
  // index < 0, max(length + index, 0); else min(index, length)
  TNode<IntPtrT> ConvertToRelativeIndex(TNode<Context> context,
                                        TNode<Object> index,
                                        TNode<IntPtrT> length);

  // Returns true iff the given value fits into smi range and is >= 0.
  TNode<BoolT> IsValidPositiveSmi(TNode<IntPtrT> value);

  // Tag an IntPtr as a Smi value.
  TNode<Smi> SmiTag(SloppyTNode<IntPtrT> value);
  // Untag a Smi value as an IntPtr.
  TNode<IntPtrT> SmiUntag(SloppyTNode<Smi> value);

  // Smi conversions.
  TNode<Float64T> SmiToFloat64(SloppyTNode<Smi> value);
  TNode<Smi> SmiFromIntPtr(SloppyTNode<IntPtrT> value) { return SmiTag(value); }
  TNode<Smi> SmiFromInt32(SloppyTNode<Int32T> value);
  TNode<IntPtrT> SmiToIntPtr(SloppyTNode<Smi> value) { return SmiUntag(value); }
  TNode<Int32T> SmiToInt32(SloppyTNode<Smi> value);

  // Smi operations.
#define SMI_ARITHMETIC_BINOP(SmiOpName, IntPtrOpName, Int32OpName)            \
  TNode<Smi> SmiOpName(TNode<Smi> a, TNode<Smi> b) {                          \
    if (SmiValuesAre32Bits()) {                                               \
      return BitcastWordToTaggedSigned(IntPtrOpName(                          \
          BitcastTaggedSignedToWord(a), BitcastTaggedSignedToWord(b)));       \
    } else {                                                                  \
      DCHECK(SmiValuesAre31Bits());                                           \
      if (kSystemPointerSize == kInt64Size) {                                 \
        CSA_ASSERT(this, IsValidSmi(a));                                      \
        CSA_ASSERT(this, IsValidSmi(b));                                      \
      }                                                                       \
      return BitcastWordToTaggedSigned(ChangeInt32ToIntPtr(                   \
          Int32OpName(TruncateIntPtrToInt32(BitcastTaggedSignedToWord(a)),    \
                      TruncateIntPtrToInt32(BitcastTaggedSignedToWord(b))))); \
    }                                                                         \
  }
  SMI_ARITHMETIC_BINOP(SmiAdd, IntPtrAdd, Int32Add)
  SMI_ARITHMETIC_BINOP(SmiSub, IntPtrSub, Int32Sub)
  SMI_ARITHMETIC_BINOP(SmiAnd, WordAnd, Word32And)
  SMI_ARITHMETIC_BINOP(SmiOr, WordOr, Word32Or)
#undef SMI_ARITHMETIC_BINOP
  TNode<Smi> SmiInc(TNode<Smi> value) { return SmiAdd(value, SmiConstant(1)); }

  TNode<IntPtrT> TryIntPtrAdd(TNode<IntPtrT> a, TNode<IntPtrT> b,
                              Label* if_overflow);
  TNode<IntPtrT> TryIntPtrSub(TNode<IntPtrT> a, TNode<IntPtrT> b,
                              Label* if_overflow);
  TNode<Int32T> TryInt32Mul(TNode<Int32T> a, TNode<Int32T> b,
                            Label* if_overflow);
  TNode<Smi> TrySmiAdd(TNode<Smi> a, TNode<Smi> b, Label* if_overflow);
  TNode<Smi> TrySmiSub(TNode<Smi> a, TNode<Smi> b, Label* if_overflow);

  TNode<Smi> SmiShl(TNode<Smi> a, int shift) {
    return BitcastWordToTaggedSigned(
        WordShl(BitcastTaggedSignedToWord(a), shift));
  }

  TNode<Smi> SmiShr(TNode<Smi> a, int shift) {
    return BitcastWordToTaggedSigned(
        WordAnd(WordShr(BitcastTaggedSignedToWord(a), shift),
                BitcastTaggedSignedToWord(SmiConstant(-1))));
  }

  TNode<Smi> SmiSar(TNode<Smi> a, int shift) {
    return BitcastWordToTaggedSigned(
        WordAnd(WordSar(BitcastTaggedSignedToWord(a), shift),
                BitcastTaggedSignedToWord(SmiConstant(-1))));
  }

  Node* WordOrSmiShl(Node* a, int shift, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) {
      return SmiShl(CAST(a), shift);
    } else {
      DCHECK_EQ(INTPTR_PARAMETERS, mode);
      return WordShl(a, shift);
    }
  }

  Node* WordOrSmiShr(Node* a, int shift, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) {
      return SmiShr(CAST(a), shift);
    } else {
      DCHECK_EQ(INTPTR_PARAMETERS, mode);
      return WordShr(a, shift);
    }
  }

#define SMI_COMPARISON_OP(SmiOpName, IntPtrOpName, Int32OpName)                \
  TNode<BoolT> SmiOpName(TNode<Smi> a, TNode<Smi> b) {                         \
    if (SmiValuesAre32Bits()) {                                                \
      return IntPtrOpName(BitcastTaggedSignedToWord(a),                        \
                          BitcastTaggedSignedToWord(b));                       \
    } else {                                                                   \
      DCHECK(SmiValuesAre31Bits());                                            \
      if (kSystemPointerSize == kInt64Size) {                                  \
        CSA_ASSERT(this, IsValidSmi(a));                                       \
        CSA_ASSERT(this, IsValidSmi(b));                                       \
      }                                                                        \
      return Int32OpName(TruncateIntPtrToInt32(BitcastTaggedSignedToWord(a)),  \
                         TruncateIntPtrToInt32(BitcastTaggedSignedToWord(b))); \
    }                                                                          \
  }
  SMI_COMPARISON_OP(SmiEqual, WordEqual, Word32Equal)
  SMI_COMPARISON_OP(SmiNotEqual, WordNotEqual, Word32NotEqual)
  SMI_COMPARISON_OP(SmiAbove, UintPtrGreaterThan, Uint32GreaterThan)
  SMI_COMPARISON_OP(SmiAboveOrEqual, UintPtrGreaterThanOrEqual,
                    Uint32GreaterThanOrEqual)
  SMI_COMPARISON_OP(SmiBelow, UintPtrLessThan, Uint32LessThan)
  SMI_COMPARISON_OP(SmiLessThan, IntPtrLessThan, Int32LessThan)
  SMI_COMPARISON_OP(SmiLessThanOrEqual, IntPtrLessThanOrEqual,
                    Int32LessThanOrEqual)
  SMI_COMPARISON_OP(SmiGreaterThan, IntPtrGreaterThan, Int32GreaterThan)
  SMI_COMPARISON_OP(SmiGreaterThanOrEqual, IntPtrGreaterThanOrEqual,
                    Int32GreaterThanOrEqual)
#undef SMI_COMPARISON_OP
  TNode<Smi> SmiMax(TNode<Smi> a, TNode<Smi> b);
  TNode<Smi> SmiMin(TNode<Smi> a, TNode<Smi> b);
  // Computes a % b for Smi inputs a and b; result is not necessarily a Smi.
  TNode<Number> SmiMod(TNode<Smi> a, TNode<Smi> b);
  // Computes a * b for Smi inputs a and b; result is not necessarily a Smi.
  TNode<Number> SmiMul(TNode<Smi> a, TNode<Smi> b);
  // Tries to compute dividend / divisor for Smi inputs; branching to bailout
  // if the division needs to be performed as a floating point operation.
  TNode<Smi> TrySmiDiv(TNode<Smi> dividend, TNode<Smi> divisor, Label* bailout);

  // Compares two Smis a and b as if they were converted to strings and then
  // compared lexicographically. Returns:
  // -1 iff x < y.
  //  0 iff x == y.
  //  1 iff x > y.
  TNode<Smi> SmiLexicographicCompare(TNode<Smi> x, TNode<Smi> y);

  // Smi | HeapNumber operations.
  TNode<Number> NumberInc(SloppyTNode<Number> value);
  TNode<Number> NumberDec(SloppyTNode<Number> value);
  TNode<Number> NumberAdd(SloppyTNode<Number> a, SloppyTNode<Number> b);
  TNode<Number> NumberSub(SloppyTNode<Number> a, SloppyTNode<Number> b);
  void GotoIfNotNumber(Node* value, Label* is_not_number);
  void GotoIfNumber(Node* value, Label* is_number);
  TNode<Number> SmiToNumber(TNode<Smi> v) { return v; }

  TNode<Number> BitwiseOp(Node* left32, Node* right32, Operation bitwise_op);

  // Allocate an object of the given size.
  TNode<HeapObject> AllocateInNewSpace(TNode<IntPtrT> size,
                                       AllocationFlags flags = kNone);
  TNode<HeapObject> AllocateInNewSpace(int size, AllocationFlags flags = kNone);
  TNode<HeapObject> Allocate(TNode<IntPtrT> size,
                             AllocationFlags flags = kNone);
  TNode<HeapObject> Allocate(int size, AllocationFlags flags = kNone);
  TNode<HeapObject> InnerAllocate(TNode<HeapObject> previous, int offset);
  TNode<HeapObject> InnerAllocate(TNode<HeapObject> previous,
                                  TNode<IntPtrT> offset);

  TNode<BoolT> IsRegularHeapObjectSize(TNode<IntPtrT> size);

  using BranchGenerator = std::function<void(Label*, Label*)>;
  using NodeGenerator = std::function<Node*()>;
  using ExtraNode = std::pair<Node*, const char*>;

  void Assert(const BranchGenerator& branch, const char* message,
              const char* file, int line,
              std::initializer_list<ExtraNode> extra_nodes = {});
  void Assert(const NodeGenerator& condition_body, const char* message,
              const char* file, int line,
              std::initializer_list<ExtraNode> extra_nodes = {});
  void Check(const BranchGenerator& branch, const char* message,
             const char* file, int line,
             std::initializer_list<ExtraNode> extra_nodes = {});
  void Check(const NodeGenerator& condition_body, const char* message,
             const char* file, int line,
             std::initializer_list<ExtraNode> extra_nodes = {});
  void FailAssert(const char* message, const char* file, int line,
                  std::initializer_list<ExtraNode> extra_nodes = {});

  void FastCheck(TNode<BoolT> condition);

  // The following Call wrappers call an object according to the semantics that
  // one finds in the EcmaScript spec, operating on an Callable (e.g. a
  // JSFunction or proxy) rather than a Code object.
  template <class... TArgs>
  TNode<Object> Call(TNode<Context> context, TNode<Object> callable,
                     TNode<JSReceiver> receiver, TArgs... args) {
    return UncheckedCast<Object>(CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        context, callable, receiver, args...));
  }
  template <class... TArgs>
  TNode<Object> Call(TNode<Context> context, TNode<Object> callable,
                     TNode<Object> receiver, TArgs... args) {
    if (IsUndefinedConstant(receiver) || IsNullConstant(receiver)) {
      return UncheckedCast<Object>(CallJS(
          CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
          context, callable, receiver, args...));
    }
    return UncheckedCast<Object>(CallJS(CodeFactory::Call(isolate()), context,
                                        callable, receiver, args...));
  }

  template <class... TArgs>
  TNode<JSReceiver> ConstructWithTarget(TNode<Context> context,
                                        TNode<JSReceiver> target,
                                        TNode<JSReceiver> new_target,
                                        TArgs... args) {
    return CAST(ConstructJSWithTarget(CodeFactory::Construct(isolate()),
                                      context, target, new_target,
                                      implicit_cast<TNode<Object>>(args)...));
  }
  template <class... TArgs>
  TNode<JSReceiver> Construct(TNode<Context> context,
                              TNode<JSReceiver> new_target, TArgs... args) {
    return ConstructWithTarget(context, new_target, new_target, args...);
  }

  template <class A, class F, class G>
  TNode<A> Select(SloppyTNode<BoolT> condition, const F& true_body,
                  const G& false_body) {
    return UncheckedCast<A>(SelectImpl(
        condition,
        [&]() -> Node* { return implicit_cast<TNode<A>>(true_body()); },
        [&]() -> Node* { return implicit_cast<TNode<A>>(false_body()); },
        MachineRepresentationOf<A>::value));
  }

  template <class A>
  TNode<A> SelectConstant(TNode<BoolT> condition, TNode<A> true_value,
                          TNode<A> false_value) {
    return Select<A>(
        condition, [=] { return true_value; }, [=] { return false_value; });
  }

  TNode<Int32T> SelectInt32Constant(SloppyTNode<BoolT> condition,
                                    int true_value, int false_value);
  TNode<IntPtrT> SelectIntPtrConstant(SloppyTNode<BoolT> condition,
                                      int true_value, int false_value);
  TNode<Oddball> SelectBooleanConstant(SloppyTNode<BoolT> condition);
  TNode<Smi> SelectSmiConstant(SloppyTNode<BoolT> condition, Smi true_value,
                               Smi false_value);
  TNode<Smi> SelectSmiConstant(SloppyTNode<BoolT> condition, int true_value,
                               Smi false_value) {
    return SelectSmiConstant(condition, Smi::FromInt(true_value), false_value);
  }
  TNode<Smi> SelectSmiConstant(SloppyTNode<BoolT> condition, Smi true_value,
                               int false_value) {
    return SelectSmiConstant(condition, true_value, Smi::FromInt(false_value));
  }
  TNode<Smi> SelectSmiConstant(SloppyTNode<BoolT> condition, int true_value,
                               int false_value) {
    return SelectSmiConstant(condition, Smi::FromInt(true_value),
                             Smi::FromInt(false_value));
  }

  TNode<String> SingleCharacterStringConstant(char const* single_char) {
    DCHECK_EQ(strlen(single_char), 1);
    return HeapConstant(
        isolate()->factory()->LookupSingleCharacterStringFromCode(
            single_char[0]));
  }

  TNode<Int32T> TruncateIntPtrToInt32(SloppyTNode<IntPtrT> value);

  // Check a value for smi-ness
  TNode<BoolT> TaggedIsSmi(SloppyTNode<Object> a);
  TNode<BoolT> TaggedIsSmi(TNode<MaybeObject> a);
  TNode<BoolT> TaggedIsNotSmi(SloppyTNode<Object> a);
  // Check that the value is a non-negative smi.
  TNode<BoolT> TaggedIsPositiveSmi(SloppyTNode<Object> a);
  // Check that a word has a word-aligned address.
  TNode<BoolT> WordIsAligned(SloppyTNode<WordT> word, size_t alignment);
  TNode<BoolT> WordIsPowerOfTwo(SloppyTNode<IntPtrT> value);

#if DEBUG
  void Bind(Label* label, AssemblerDebugInfo debug_info);
#endif  // DEBUG
  void Bind(Label* label);

  template <class... T>
  void Bind(compiler::CodeAssemblerParameterizedLabel<T...>* label,
            TNode<T>*... phis) {
    CodeAssembler::Bind(label, phis...);
  }

  void BranchIfSmiEqual(TNode<Smi> a, TNode<Smi> b, Label* if_true,
                        Label* if_false) {
    Branch(SmiEqual(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThan(TNode<Smi> a, TNode<Smi> b, Label* if_true,
                           Label* if_false) {
    Branch(SmiLessThan(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThanOrEqual(TNode<Smi> a, TNode<Smi> b, Label* if_true,
                                  Label* if_false) {
    Branch(SmiLessThanOrEqual(a, b), if_true, if_false);
  }

  void BranchIfFloat64IsNaN(Node* value, Label* if_true, Label* if_false) {
    Branch(Float64Equal(value, value), if_false, if_true);
  }

  // Branches to {if_true} if ToBoolean applied to {value} yields true,
  // otherwise goes to {if_false}.
  void BranchIfToBooleanIsTrue(Node* value, Label* if_true, Label* if_false);

  // Branches to {if_false} if ToBoolean applied to {value} yields false,
  // otherwise goes to {if_true}.
  void BranchIfToBooleanIsFalse(Node* value, Label* if_false, Label* if_true) {
    BranchIfToBooleanIsTrue(value, if_true, if_false);
  }

  void BranchIfJSReceiver(Node* object, Label* if_true, Label* if_false);

  // Branches to {if_true} when --force-slow-path flag has been passed.
  // It's used for testing to ensure that slow path implementation behave
  // equivalent to corresponding fast paths (where applicable).
  //
  // Works only with V8_ENABLE_FORCE_SLOW_PATH compile time flag. Nop otherwise.
  void GotoIfForceSlowPath(Label* if_true);

  // Branches to {if_true} when Debug::ExecutionMode is DebugInfo::kSideEffect.
  void GotoIfDebugExecutionModeChecksSideEffects(Label* if_true);

  // Load value from current parent frame by given offset in bytes.
  Node* LoadFromParentFrame(int offset,
                            MachineType type = MachineType::AnyTagged());

  // Load an object pointer from a buffer that isn't in the heap.
  Node* LoadBufferObject(Node* buffer, int offset, MachineType type);
  TNode<Object> LoadBufferObject(TNode<RawPtrT> buffer, int offset) {
    return CAST(LoadBufferObject(buffer, offset, MachineType::AnyTagged()));
  }
  TNode<RawPtrT> LoadBufferPointer(TNode<RawPtrT> buffer, int offset) {
    return UncheckedCast<RawPtrT>(
        LoadBufferObject(buffer, offset, MachineType::Pointer()));
  }
  TNode<Smi> LoadBufferSmi(TNode<RawPtrT> buffer, int offset) {
    return CAST(LoadBufferObject(buffer, offset, MachineType::TaggedSigned()));
  }
  // Load a field from an object on the heap.
  Node* LoadObjectField(SloppyTNode<HeapObject> object, int offset,
                        MachineType type);
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<Object>>::value,
                         int>::type = 0>
  TNode<T> LoadObjectField(TNode<HeapObject> object, int offset) {
    return CAST(LoadObjectField(object, offset, MachineTypeOf<T>::value));
  }
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<UntaggedT>>::value,
                         int>::type = 0>
  TNode<T> LoadObjectField(TNode<HeapObject> object, int offset) {
    return UncheckedCast<T>(
        LoadObjectField(object, offset, MachineTypeOf<T>::value));
  }
  TNode<Object> LoadObjectField(SloppyTNode<HeapObject> object, int offset) {
    return UncheckedCast<Object>(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
  Node* LoadObjectField(SloppyTNode<HeapObject> object,
                        SloppyTNode<IntPtrT> offset, MachineType type);
  TNode<Object> LoadObjectField(SloppyTNode<HeapObject> object,
                                SloppyTNode<IntPtrT> offset) {
    return UncheckedCast<Object>(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<UntaggedT>>::value,
                         int>::type = 0>
  TNode<T> LoadObjectField(TNode<HeapObject> object, TNode<IntPtrT> offset) {
    return UncheckedCast<T>(
        LoadObjectField(object, offset, MachineTypeOf<T>::value));
  }
  // Load a SMI field and untag it.
  TNode<IntPtrT> LoadAndUntagObjectField(SloppyTNode<HeapObject> object,
                                         int offset);
  // Load a SMI field, untag it, and convert to Word32.
  TNode<Int32T> LoadAndUntagToWord32ObjectField(Node* object, int offset);
  // Load a SMI and untag it.
  TNode<IntPtrT> LoadAndUntagSmi(Node* base, int index);

  TNode<MaybeObject> LoadMaybeWeakObjectField(SloppyTNode<HeapObject> object,
                                              int offset) {
    return UncheckedCast<MaybeObject>(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }

  TNode<Object> LoadConstructorOrBackPointer(TNode<Map> map) {
    return LoadObjectField(map, Map::kConstructorOrBackPointerOffset);
  }

  // Reference is the CSA-equivalent of a Torque reference value,
  // representing an inner pointer into a HeapObject.
  struct Reference {
    TNode<HeapObject> object;
    TNode<IntPtrT> offset;

    std::tuple<TNode<HeapObject>, TNode<IntPtrT>> Flatten() const {
      return std::make_tuple(object, offset);
    }
  };

  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<Object>>::value,
                         int>::type = 0>
  TNode<T> LoadReference(Reference reference) {
    TNode<IntPtrT> offset =
        IntPtrSub(reference.offset, IntPtrConstant(kHeapObjectTag));
    return CAST(
        LoadFromObject(MachineTypeOf<T>::value, reference.object, offset));
  }
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<UntaggedT>>::value,
                         int>::type = 0>
  TNode<T> LoadReference(Reference reference) {
    TNode<IntPtrT> offset =
        IntPtrSub(reference.offset, IntPtrConstant(kHeapObjectTag));
    return UncheckedCast<T>(
        LoadFromObject(MachineTypeOf<T>::value, reference.object, offset));
  }
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<Object>>::value,
                         int>::type = 0>
  void StoreReference(Reference reference, TNode<T> value) {
    MachineRepresentation rep = MachineRepresentationOf<T>::value;
    StoreToObjectWriteBarrier write_barrier = StoreToObjectWriteBarrier::kFull;
    if (std::is_same<T, Smi>::value) {
      write_barrier = StoreToObjectWriteBarrier::kNone;
    } else if (std::is_same<T, Map>::value) {
      write_barrier = StoreToObjectWriteBarrier::kMap;
    }
    TNode<IntPtrT> offset =
        IntPtrSub(reference.offset, IntPtrConstant(kHeapObjectTag));
    StoreToObject(rep, reference.object, offset, value, write_barrier);
  }
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<UntaggedT>>::value,
                         int>::type = 0>
  void StoreReference(Reference reference, TNode<T> value) {
    TNode<IntPtrT> offset =
        IntPtrSub(reference.offset, IntPtrConstant(kHeapObjectTag));
    StoreToObject(MachineRepresentationOf<T>::value, reference.object, offset,
                  value, StoreToObjectWriteBarrier::kNone);
  }

  // Tag a smi and store it.
  void StoreAndTagSmi(Node* base, int offset, Node* value);

  // Load the floating point value of a HeapNumber.
  TNode<Float64T> LoadHeapNumberValue(SloppyTNode<HeapNumber> object);
  // Load the Map of an HeapObject.
  TNode<Map> LoadMap(SloppyTNode<HeapObject> object);
  // Load the instance type of an HeapObject.
  TNode<Uint16T> LoadInstanceType(SloppyTNode<HeapObject> object);
  // Compare the instance the type of the object against the provided one.
  TNode<BoolT> HasInstanceType(SloppyTNode<HeapObject> object,
                               InstanceType type);
  TNode<BoolT> DoesntHaveInstanceType(SloppyTNode<HeapObject> object,
                                      InstanceType type);
  TNode<BoolT> TaggedDoesntHaveInstanceType(SloppyTNode<HeapObject> any_tagged,
                                            InstanceType type);
  // Load the properties backing store of a JSObject.
  TNode<HeapObject> LoadSlowProperties(SloppyTNode<JSObject> object);
  TNode<HeapObject> LoadFastProperties(SloppyTNode<JSObject> object);
  // Load the elements backing store of a JSObject.
  TNode<FixedArrayBase> LoadElements(SloppyTNode<JSObject> object) {
    return LoadJSObjectElements(object);
  }
  // Load the length of a JSArray instance.
  TNode<Object> LoadJSArgumentsObjectWithLength(
      SloppyTNode<JSArgumentsObjectWithLength> array);
  // Load the length of a JSArray instance.
  TNode<Number> LoadJSArrayLength(SloppyTNode<JSArray> array);
  // Load the length of a fast JSArray instance. Returns a positive Smi.
  TNode<Smi> LoadFastJSArrayLength(SloppyTNode<JSArray> array);
  // Load the length of a fixed array base instance.
  TNode<Smi> LoadFixedArrayBaseLength(SloppyTNode<FixedArrayBase> array);
  // Load the length of a fixed array base instance.
  TNode<IntPtrT> LoadAndUntagFixedArrayBaseLength(
      SloppyTNode<FixedArrayBase> array);
  // Load the length of a WeakFixedArray.
  TNode<Smi> LoadWeakFixedArrayLength(TNode<WeakFixedArray> array);
  TNode<IntPtrT> LoadAndUntagWeakFixedArrayLength(
      SloppyTNode<WeakFixedArray> array);
  // Load the number of descriptors in DescriptorArray.
  TNode<Int32T> LoadNumberOfDescriptors(TNode<DescriptorArray> array);
  // Load the bit field of a Map.
  TNode<Int32T> LoadMapBitField(SloppyTNode<Map> map);
  // Load bit field 2 of a map.
  TNode<Int32T> LoadMapBitField2(SloppyTNode<Map> map);
  // Load bit field 3 of a map.
  TNode<Uint32T> LoadMapBitField3(SloppyTNode<Map> map);
  // Load the instance type of a map.
  TNode<Uint16T> LoadMapInstanceType(SloppyTNode<Map> map);
  // Load the ElementsKind of a map.
  TNode<Int32T> LoadMapElementsKind(SloppyTNode<Map> map);
  TNode<Int32T> LoadElementsKind(SloppyTNode<HeapObject> object);
  // Load the instance descriptors of a map.
  TNode<DescriptorArray> LoadMapDescriptors(SloppyTNode<Map> map);
  // Load the prototype of a map.
  TNode<HeapObject> LoadMapPrototype(SloppyTNode<Map> map);
  // Load the prototype info of a map. The result has to be checked if it is a
  // prototype info object or not.
  TNode<PrototypeInfo> LoadMapPrototypeInfo(SloppyTNode<Map> map,
                                            Label* if_has_no_proto_info);
  // Load the instance size of a Map.
  TNode<IntPtrT> LoadMapInstanceSizeInWords(SloppyTNode<Map> map);
  // Load the inobject properties start of a Map (valid only for JSObjects).
  TNode<IntPtrT> LoadMapInobjectPropertiesStartInWords(SloppyTNode<Map> map);
  // Load the constructor function index of a Map (only for primitive maps).
  TNode<IntPtrT> LoadMapConstructorFunctionIndex(SloppyTNode<Map> map);
  // Load the constructor of a Map (equivalent to Map::GetConstructor()).
  TNode<Object> LoadMapConstructor(SloppyTNode<Map> map);
  // Load the EnumLength of a Map.
  Node* LoadMapEnumLength(SloppyTNode<Map> map);
  // Load the back-pointer of a Map.
  TNode<Object> LoadMapBackPointer(SloppyTNode<Map> map);
  // Checks that |map| has only simple properties, returns bitfield3.
  TNode<Uint32T> EnsureOnlyHasSimpleProperties(TNode<Map> map,
                                               TNode<Int32T> instance_type,
                                               Label* bailout);
  // Load the identity hash of a JSRececiver.
  TNode<IntPtrT> LoadJSReceiverIdentityHash(SloppyTNode<Object> receiver,
                                            Label* if_no_hash = nullptr);

  // This is only used on a newly allocated PropertyArray which
  // doesn't have an existing hash.
  void InitializePropertyArrayLength(Node* property_array, Node* length,
                                     ParameterMode mode);

  // Check if the map is set for slow properties.
  TNode<BoolT> IsDictionaryMap(SloppyTNode<Map> map);

  // Load the hash field of a name as an uint32 value.
  TNode<Uint32T> LoadNameHashField(SloppyTNode<Name> name);
  // Load the hash value of a name as an uint32 value.
  // If {if_hash_not_computed} label is specified then it also checks if
  // hash is actually computed.
  TNode<Uint32T> LoadNameHash(SloppyTNode<Name> name,
                              Label* if_hash_not_computed = nullptr);

  // Load length field of a String object as Smi value.
  TNode<Smi> LoadStringLengthAsSmi(SloppyTNode<String> string);
  // Load length field of a String object as intptr_t value.
  TNode<IntPtrT> LoadStringLengthAsWord(SloppyTNode<String> string);
  // Load length field of a String object as uint32_t value.
  TNode<Uint32T> LoadStringLengthAsWord32(SloppyTNode<String> string);
  // Loads a pointer to the sequential String char array.
  Node* PointerToSeqStringData(Node* seq_string);
  // Load value field of a JSPrimitiveWrapper object.
  Node* LoadJSPrimitiveWrapperValue(Node* object);

  // Figures out whether the value of maybe_object is:
  // - a SMI (jump to "if_smi", "extracted" will be the SMI value)
  // - a cleared weak reference (jump to "if_cleared", "extracted" will be
  // untouched)
  // - a weak reference (jump to "if_weak", "extracted" will be the object
  // pointed to)
  // - a strong reference (jump to "if_strong", "extracted" will be the object
  // pointed to)
  void DispatchMaybeObject(TNode<MaybeObject> maybe_object, Label* if_smi,
                           Label* if_cleared, Label* if_weak, Label* if_strong,
                           TVariable<Object>* extracted);
  // See MaybeObject for semantics of these functions.
  TNode<BoolT> IsStrong(TNode<MaybeObject> value);
  // This variant is for overzealous checking.
  TNode<BoolT> IsStrong(TNode<Object> value) {
    return IsStrong(ReinterpretCast<MaybeObject>(value));
  }
  TNode<HeapObject> GetHeapObjectIfStrong(TNode<MaybeObject> value,
                                          Label* if_not_strong);

  TNode<BoolT> IsWeakOrCleared(TNode<MaybeObject> value);
  TNode<BoolT> IsCleared(TNode<MaybeObject> value);
  TNode<BoolT> IsNotCleared(TNode<MaybeObject> value);

  // Removes the weak bit + asserts it was set.
  TNode<HeapObject> GetHeapObjectAssumeWeak(TNode<MaybeObject> value);

  TNode<HeapObject> GetHeapObjectAssumeWeak(TNode<MaybeObject> value,
                                            Label* if_cleared);

  TNode<BoolT> IsWeakReferenceTo(TNode<MaybeObject> object,
                                 TNode<Object> value);
  TNode<BoolT> IsNotWeakReferenceTo(TNode<MaybeObject> object,
                                    TNode<Object> value);
  TNode<BoolT> IsStrongReferenceTo(TNode<MaybeObject> object,
                                   TNode<Object> value);

  TNode<MaybeObject> MakeWeak(TNode<HeapObject> value);

  void FixedArrayBoundsCheck(TNode<FixedArrayBase> array, Node* index,
                             int additional_offset = 0,
                             ParameterMode parameter_mode = INTPTR_PARAMETERS);

  // Array is any array-like type that has a fixed header followed by
  // tagged elements.
  template <typename Array>
  TNode<IntPtrT> LoadArrayLength(TNode<Array> array);

  // Array is any array-like type that has a fixed header followed by
  // tagged elements.
  template <typename Array, typename T = MaybeObject>
  TNode<T> LoadArrayElement(
      TNode<Array> array, int array_header_size, Node* index,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);

  TNode<Object> LoadFixedArrayElement(
      TNode<FixedArray> object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe,
      CheckBounds check_bounds = CheckBounds::kAlways);

  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  TNode<Object> UnsafeLoadFixedArrayElement(
      TNode<FixedArray> object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe) {
    return LoadFixedArrayElement(object, index, additional_offset,
                                 parameter_mode, needs_poisoning,
                                 CheckBounds::kDebugOnly);
  }

  TNode<Object> LoadFixedArrayElement(
      TNode<FixedArray> object, TNode<IntPtrT> index,
      LoadSensitivity needs_poisoning,
      CheckBounds check_bounds = CheckBounds::kAlways) {
    return LoadFixedArrayElement(object, index, 0, INTPTR_PARAMETERS,
                                 needs_poisoning, check_bounds);
  }
  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  TNode<Object> UnsafeLoadFixedArrayElement(TNode<FixedArray> object,
                                            TNode<IntPtrT> index,
                                            LoadSensitivity needs_poisoning) {
    return LoadFixedArrayElement(object, index, needs_poisoning,
                                 CheckBounds::kDebugOnly);
  }

  TNode<Object> LoadFixedArrayElement(
      TNode<FixedArray> object, TNode<IntPtrT> index, int additional_offset = 0,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe) {
    return LoadFixedArrayElement(object, index, additional_offset,
                                 INTPTR_PARAMETERS, needs_poisoning);
  }

  TNode<Object> LoadFixedArrayElement(
      TNode<FixedArray> object, int index, int additional_offset = 0,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe) {
    return LoadFixedArrayElement(object, IntPtrConstant(index),
                                 additional_offset, INTPTR_PARAMETERS,
                                 needs_poisoning);
  }
  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  TNode<Object> UnsafeLoadFixedArrayElement(
      TNode<FixedArray> object, int index, int additional_offset = 0,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe) {
    return LoadFixedArrayElement(object, IntPtrConstant(index),
                                 additional_offset, INTPTR_PARAMETERS,
                                 needs_poisoning, CheckBounds::kDebugOnly);
  }
  TNode<Object> LoadFixedArrayElement(TNode<FixedArray> object,
                                      TNode<Smi> index) {
    return LoadFixedArrayElement(object, index, 0, SMI_PARAMETERS);
  }

  TNode<Object> LoadPropertyArrayElement(TNode<PropertyArray> object,
                                         SloppyTNode<IntPtrT> index);
  TNode<IntPtrT> LoadPropertyArrayLength(TNode<PropertyArray> object);

  // Load an element from an array and untag it and return it as Word32.
  // Array is any array-like type that has a fixed header followed by
  // tagged elements.
  template <typename Array>
  TNode<Int32T> LoadAndUntagToWord32ArrayElement(
      TNode<Array> array, int array_header_size, Node* index,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  // Load an array element from a FixedArray, untag it and return it as Word32.
  TNode<Int32T> LoadAndUntagToWord32FixedArrayElement(
      TNode<FixedArray> object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  TNode<Int32T> LoadAndUntagToWord32FixedArrayElement(
      TNode<FixedArray> object, int index, int additional_offset = 0) {
    return LoadAndUntagToWord32FixedArrayElement(
        object, IntPtrConstant(index), additional_offset, INTPTR_PARAMETERS);
  }

  // Load an array element from a WeakFixedArray.
  TNode<MaybeObject> LoadWeakFixedArrayElement(
      TNode<WeakFixedArray> object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);

  TNode<MaybeObject> LoadWeakFixedArrayElement(
      TNode<WeakFixedArray> object, int index, int additional_offset = 0,
      LoadSensitivity needs_poisoning = LoadSensitivity::kSafe) {
    return LoadWeakFixedArrayElement(object, IntPtrConstant(index),
                                     additional_offset, INTPTR_PARAMETERS,
                                     needs_poisoning);
  }

  // Load an array element from a FixedDoubleArray.
  TNode<Float64T> LoadFixedDoubleArrayElement(
      SloppyTNode<FixedDoubleArray> object, Node* index,
      MachineType machine_type, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      Label* if_hole = nullptr);

  Node* LoadFixedDoubleArrayElement(TNode<FixedDoubleArray> object,
                                    TNode<Smi> index,
                                    Label* if_hole = nullptr) {
    return LoadFixedDoubleArrayElement(object, index, MachineType::Float64(), 0,
                                       SMI_PARAMETERS, if_hole);
  }

  Node* LoadFixedDoubleArrayElement(TNode<FixedDoubleArray> object,
                                    TNode<IntPtrT> index,
                                    Label* if_hole = nullptr) {
    return LoadFixedDoubleArrayElement(object, index, MachineType::Float64(), 0,
                                       INTPTR_PARAMETERS, if_hole);
  }

  // Load an array element from a FixedArray, FixedDoubleArray or a
  // NumberDictionary (depending on the |elements_kind|) and return
  // it as a tagged value. Assumes that the |index| passed a length
  // check before. Bails out to |if_accessor| if the element that
  // was found is an accessor, or to |if_hole| if the element at
  // the given |index| is not found in |elements|.
  TNode<Object> LoadFixedArrayBaseElementAsTagged(
      TNode<FixedArrayBase> elements, TNode<IntPtrT> index,
      TNode<Int32T> elements_kind, Label* if_accessor, Label* if_hole);

  // Load a feedback slot from a FeedbackVector.
  TNode<MaybeObject> LoadFeedbackVectorSlot(
      Node* object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  TNode<IntPtrT> LoadFeedbackVectorLength(TNode<FeedbackVector>);
  TNode<Float64T> LoadDoubleWithHoleCheck(TNode<FixedDoubleArray> array,
                                          TNode<Smi> index,
                                          Label* if_hole = nullptr);
  TNode<Float64T> LoadDoubleWithHoleCheck(TNode<FixedDoubleArray> array,
                                          TNode<IntPtrT> index,
                                          Label* if_hole = nullptr);

  // Load Float64 value by |base| + |offset| address. If the value is a double
  // hole then jump to |if_hole|. If |machine_type| is None then only the hole
  // check is generated.
  TNode<Float64T> LoadDoubleWithHoleCheck(
      SloppyTNode<Object> base, SloppyTNode<IntPtrT> offset, Label* if_hole,
      MachineType machine_type = MachineType::Float64());
  TNode<Numeric> LoadFixedTypedArrayElementAsTagged(
      TNode<RawPtrT> data_pointer, Node* index_node, ElementsKind elements_kind,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);
  TNode<Numeric> LoadFixedTypedArrayElementAsTagged(
      TNode<RawPtrT> data_pointer, TNode<Smi> index_node,
      ElementsKind elements_kind) {
    return LoadFixedTypedArrayElementAsTagged(data_pointer, index_node,
                                              elements_kind, SMI_PARAMETERS);
  }
  TNode<Numeric> LoadFixedTypedArrayElementAsTagged(
      TNode<RawPtrT> data_pointer, TNode<Smi> index,
      TNode<Int32T> elements_kind);
  // Parts of the above, factored out for readability:
  TNode<BigInt> LoadFixedBigInt64ArrayElementAsTagged(
      SloppyTNode<RawPtrT> data_pointer, SloppyTNode<IntPtrT> offset);
  TNode<BigInt> LoadFixedBigUint64ArrayElementAsTagged(
      SloppyTNode<RawPtrT> data_pointer, SloppyTNode<IntPtrT> offset);
  // 64-bit platforms only:
  TNode<BigInt> BigIntFromInt64(TNode<IntPtrT> value);
  TNode<BigInt> BigIntFromUint64(TNode<UintPtrT> value);
  // 32-bit platforms only:
  TNode<BigInt> BigIntFromInt32Pair(TNode<IntPtrT> low, TNode<IntPtrT> high);
  TNode<BigInt> BigIntFromUint32Pair(TNode<UintPtrT> low, TNode<UintPtrT> high);

  void StoreJSTypedArrayElementFromTagged(TNode<Context> context,
                                          TNode<JSTypedArray> typed_array,
                                          TNode<Smi> index_node,
                                          TNode<Object> value,
                                          ElementsKind elements_kind);

  // Context manipulation
  TNode<Object> LoadContextElement(SloppyTNode<Context> context,
                                   int slot_index);
  TNode<Object> LoadContextElement(SloppyTNode<Context> context,
                                   SloppyTNode<IntPtrT> slot_index);
  TNode<Object> LoadContextElement(TNode<Context> context,
                                   TNode<Smi> slot_index);
  void StoreContextElement(SloppyTNode<Context> context, int slot_index,
                           SloppyTNode<Object> value);
  void StoreContextElement(SloppyTNode<Context> context,
                           SloppyTNode<IntPtrT> slot_index,
                           SloppyTNode<Object> value);
  void StoreContextElementNoWriteBarrier(SloppyTNode<Context> context,
                                         int slot_index,
                                         SloppyTNode<Object> value);
  TNode<Context> LoadNativeContext(SloppyTNode<Context> context);
  // Calling this is only valid if there's a module context in the chain.
  TNode<Context> LoadModuleContext(SloppyTNode<Context> context);

  void GotoIfContextElementEqual(Node* value, Node* native_context,
                                 int slot_index, Label* if_equal) {
    GotoIf(WordEqual(value, LoadContextElement(native_context, slot_index)),
           if_equal);
  }

  TNode<Map> LoadJSArrayElementsMap(ElementsKind kind,
                                    SloppyTNode<Context> native_context);
  TNode<Map> LoadJSArrayElementsMap(SloppyTNode<Int32T> kind,
                                    SloppyTNode<Context> native_context);

  TNode<BoolT> HasPrototypeSlot(TNode<JSFunction> function);
  TNode<BoolT> IsGeneratorFunction(TNode<JSFunction> function);
  TNode<BoolT> HasPrototypeProperty(TNode<JSFunction> function, TNode<Map> map);
  void GotoIfPrototypeRequiresRuntimeLookup(TNode<JSFunction> function,
                                            TNode<Map> map, Label* runtime);
  // Load the "prototype" property of a JSFunction.
  Node* LoadJSFunctionPrototype(Node* function, Label* if_bailout);

  TNode<BytecodeArray> LoadSharedFunctionInfoBytecodeArray(
      SloppyTNode<SharedFunctionInfo> shared);

  void StoreObjectByteNoWriteBarrier(TNode<HeapObject> object, int offset,
                                     TNode<Word32T> value);

  // Store the floating point value of a HeapNumber.
  void StoreHeapNumberValue(SloppyTNode<HeapNumber> object,
                            SloppyTNode<Float64T> value);
  void StoreMutableHeapNumberValue(SloppyTNode<MutableHeapNumber> object,
                                   SloppyTNode<Float64T> value);
  // Store a field to an object on the heap.
  void StoreObjectField(Node* object, int offset, Node* value);
  void StoreObjectField(Node* object, Node* offset, Node* value);
  void StoreObjectFieldNoWriteBarrier(
      Node* object, int offset, Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);
  void UnsafeStoreObjectFieldNoWriteBarrier(TNode<HeapObject> object,
                                            int offset, TNode<Object> value);
  void StoreObjectFieldNoWriteBarrier(
      Node* object, SloppyTNode<IntPtrT> offset, Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);

  template <class T = Object>
  void StoreObjectFieldNoWriteBarrier(Node* object, SloppyTNode<IntPtrT> offset,
                                      TNode<T> value) {
    StoreObjectFieldNoWriteBarrier(object, offset, value,
                                   MachineRepresentationOf<T>::value);
  }
  template <class T = Object>
  void StoreObjectFieldNoWriteBarrier(Node* object, int offset,
                                      TNode<T> value) {
    StoreObjectFieldNoWriteBarrier(object, offset, value,
                                   MachineRepresentationOf<T>::value);
  }

  // Store the Map of an HeapObject.
  void StoreMap(Node* object, Node* map);
  void StoreMapNoWriteBarrier(Node* object, RootIndex map_root_index);
  void StoreMapNoWriteBarrier(Node* object, Node* map);
  void StoreObjectFieldRoot(Node* object, int offset, RootIndex root);
  // Store an array element to a FixedArray.
  void StoreFixedArrayElement(
      TNode<FixedArray> object, int index, SloppyTNode<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      CheckBounds check_bounds = CheckBounds::kAlways) {
    return StoreFixedArrayElement(object, IntPtrConstant(index), value,
                                  barrier_mode, 0, INTPTR_PARAMETERS,
                                  check_bounds);
  }
  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  void UnsafeStoreFixedArrayElement(
      TNode<FixedArray> object, int index, SloppyTNode<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER) {
    return StoreFixedArrayElement(object, index, value, barrier_mode,
                                  CheckBounds::kDebugOnly);
  }
  void UnsafeStoreFixedArrayElement(
      TNode<FixedArray> object, int index, TNode<Smi> value,
      WriteBarrierMode barrier_mode = SKIP_WRITE_BARRIER) {
    DCHECK_EQ(SKIP_WRITE_BARRIER, barrier_mode);
    return StoreFixedArrayElement(object, index, value,
                                  UNSAFE_SKIP_WRITE_BARRIER,
                                  CheckBounds::kDebugOnly);
  }
  void StoreFixedArrayElement(TNode<FixedArray> object, int index,
                              TNode<Smi> value,
                              CheckBounds check_bounds = CheckBounds::kAlways) {
    return StoreFixedArrayElement(object, IntPtrConstant(index), value,
                                  UNSAFE_SKIP_WRITE_BARRIER, 0,
                                  INTPTR_PARAMETERS, check_bounds);
  }
  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  void UnsafeStoreFixedArrayElement(TNode<FixedArray> object, int index,
                                    TNode<Smi> value) {
    return StoreFixedArrayElement(object, index, value,
                                  CheckBounds::kDebugOnly);
  }

  void StoreJSArrayLength(TNode<JSArray> array, TNode<Smi> length);
  void StoreElements(TNode<Object> object, TNode<FixedArrayBase> elements);

  void StoreFixedArrayOrPropertyArrayElement(
      Node* array, Node* index, Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  void StoreFixedArrayElement(
      TNode<FixedArray> array, Node* index, SloppyTNode<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      CheckBounds check_bounds = CheckBounds::kAlways) {
    if (NeedsBoundsCheck(check_bounds)) {
      FixedArrayBoundsCheck(array, index, additional_offset, parameter_mode);
    }
    StoreFixedArrayOrPropertyArrayElement(array, index, value, barrier_mode,
                                          additional_offset, parameter_mode);
  }

  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  void UnsafeStoreFixedArrayElement(
      TNode<FixedArray> array, Node* index, SloppyTNode<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS) {
    return StoreFixedArrayElement(array, index, value, barrier_mode,
                                  additional_offset, parameter_mode,
                                  CheckBounds::kDebugOnly);
  }
  void UnsafeStoreFixedArrayElement(
      TNode<FixedArray> array, Node* index, TNode<Smi> value,
      WriteBarrierMode barrier_mode = SKIP_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS) {
    DCHECK_EQ(SKIP_WRITE_BARRIER, barrier_mode);
    return StoreFixedArrayElement(array, index, value,
                                  UNSAFE_SKIP_WRITE_BARRIER, additional_offset,
                                  parameter_mode, CheckBounds::kDebugOnly);
  }

  void StorePropertyArrayElement(
      TNode<PropertyArray> array, Node* index, SloppyTNode<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS) {
    StoreFixedArrayOrPropertyArrayElement(array, index, value, barrier_mode,
                                          additional_offset, parameter_mode);
  }

  void StoreFixedArrayElement(
      TNode<FixedArray> array, TNode<Smi> index, TNode<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER) {
    StoreFixedArrayElement(array, index, value, barrier_mode, 0,
                           SMI_PARAMETERS);
  }
  void StoreFixedArrayElement(
      TNode<FixedArray> array, TNode<IntPtrT> index, TNode<Smi> value,
      WriteBarrierMode barrier_mode = SKIP_WRITE_BARRIER,
      int additional_offset = 0) {
    DCHECK_EQ(SKIP_WRITE_BARRIER, barrier_mode);
    StoreFixedArrayElement(array, index, TNode<Object>{value},
                           UNSAFE_SKIP_WRITE_BARRIER, additional_offset);
  }
  void StoreFixedArrayElement(
      TNode<FixedArray> array, TNode<Smi> index, TNode<Smi> value,
      WriteBarrierMode barrier_mode = SKIP_WRITE_BARRIER,
      int additional_offset = 0) {
    DCHECK_EQ(SKIP_WRITE_BARRIER, barrier_mode);
    StoreFixedArrayElement(array, index, TNode<Object>{value},
                           UNSAFE_SKIP_WRITE_BARRIER, additional_offset,
                           SMI_PARAMETERS);
  }

  void StoreFixedDoubleArrayElement(
      TNode<FixedDoubleArray> object, Node* index, TNode<Float64T> value,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      CheckBounds check_bounds = CheckBounds::kAlways);
  // This doesn't emit a bounds-check. As part of the security-performance
  // tradeoff, only use it if it is performance critical.
  void UnsafeStoreFixedDoubleArrayElement(
      TNode<FixedDoubleArray> object, Node* index, TNode<Float64T> value,
      ParameterMode parameter_mode = INTPTR_PARAMETERS) {
    return StoreFixedDoubleArrayElement(object, index, value, parameter_mode,
                                        CheckBounds::kDebugOnly);
  }

  void StoreFixedDoubleArrayElementSmi(TNode<FixedDoubleArray> object,
                                       TNode<Smi> index,
                                       TNode<Float64T> value) {
    StoreFixedDoubleArrayElement(object, index, value, SMI_PARAMETERS);
  }

  void StoreFixedDoubleArrayHole(TNode<FixedDoubleArray> array, Node* index,
                                 ParameterMode mode = INTPTR_PARAMETERS);
  void StoreFixedDoubleArrayHoleSmi(TNode<FixedDoubleArray> array,
                                    TNode<Smi> index) {
    StoreFixedDoubleArrayHole(array, index, SMI_PARAMETERS);
  }

  void StoreFeedbackVectorSlot(
      Node* object, Node* index, Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  void EnsureArrayLengthWritable(TNode<Map> map, Label* bailout);

  // EnsureArrayPushable verifies that receiver with this map is:
  //   1. Is not a prototype.
  //   2. Is not a dictionary.
  //   3. Has a writeable length property.
  // It returns ElementsKind as a node for further division into cases.
  TNode<Int32T> EnsureArrayPushable(TNode<Map> map, Label* bailout);

  void TryStoreArrayElement(ElementsKind kind, ParameterMode mode,
                            Label* bailout, Node* elements, Node* index,
                            Node* value);
  // Consumes args into the array, and returns tagged new length.
  TNode<Smi> BuildAppendJSArray(ElementsKind kind, SloppyTNode<JSArray> array,
                                CodeStubArguments* args,
                                TVariable<IntPtrT>* arg_index, Label* bailout);
  // Pushes value onto the end of array.
  void BuildAppendJSArray(ElementsKind kind, Node* array, Node* value,
                          Label* bailout);

  void StoreFieldsNoWriteBarrier(Node* start_address, Node* end_address,
                                 Node* value);

  Node* AllocateCellWithValue(Node* value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  Node* AllocateSmiCell(int value = 0) {
    return AllocateCellWithValue(SmiConstant(value), SKIP_WRITE_BARRIER);
  }

  Node* LoadCellValue(Node* cell);

  void StoreCellValue(Node* cell, Node* value,
                      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Allocate a HeapNumber without initializing its value.
  TNode<HeapNumber> AllocateHeapNumber();
  // Allocate a HeapNumber with a specific value.
  TNode<HeapNumber> AllocateHeapNumberWithValue(SloppyTNode<Float64T> value);
  TNode<HeapNumber> AllocateHeapNumberWithValue(double value) {
    return AllocateHeapNumberWithValue(Float64Constant(value));
  }

  // Allocate a MutableHeapNumber with a specific value.
  TNode<MutableHeapNumber> AllocateMutableHeapNumberWithValue(
      SloppyTNode<Float64T> value);

  // Allocate a BigInt with {length} digits. Sets the sign bit to {false}.
  // Does not initialize the digits.
  TNode<BigInt> AllocateBigInt(TNode<IntPtrT> length);
  // Like above, but allowing custom bitfield initialization.
  TNode<BigInt> AllocateRawBigInt(TNode<IntPtrT> length);
  void StoreBigIntBitfield(TNode<BigInt> bigint, TNode<Word32T> bitfield);
  void StoreBigIntDigit(TNode<BigInt> bigint, intptr_t digit_index,
                        TNode<UintPtrT> digit);
  void StoreBigIntDigit(TNode<BigInt> bigint, TNode<IntPtrT> digit_index,
                        TNode<UintPtrT> digit);

  TNode<Word32T> LoadBigIntBitfield(TNode<BigInt> bigint);
  TNode<UintPtrT> LoadBigIntDigit(TNode<BigInt> bigint, intptr_t digit_index);
  TNode<UintPtrT> LoadBigIntDigit(TNode<BigInt> bigint,
                                  TNode<IntPtrT> digit_index);

  // Allocate a ByteArray with the given length.
  TNode<ByteArray> AllocateByteArray(TNode<UintPtrT> length,
                                     AllocationFlags flags = kNone);

  // Allocate a SeqOneByteString with the given length.
  TNode<String> AllocateSeqOneByteString(uint32_t length,
                                         AllocationFlags flags = kNone);
  TNode<String> AllocateSeqOneByteString(Node* context, TNode<Uint32T> length,
                                         AllocationFlags flags = kNone);
  // Allocate a SeqTwoByteString with the given length.
  TNode<String> AllocateSeqTwoByteString(uint32_t length,
                                         AllocationFlags flags = kNone);
  TNode<String> AllocateSeqTwoByteString(Node* context, TNode<Uint32T> length,
                                         AllocationFlags flags = kNone);

  // Allocate a SlicedOneByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.

  TNode<String> AllocateSlicedOneByteString(TNode<Uint32T> length,
                                            TNode<String> parent,
                                            TNode<Smi> offset);
  // Allocate a SlicedTwoByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  TNode<String> AllocateSlicedTwoByteString(TNode<Uint32T> length,
                                            TNode<String> parent,
                                            TNode<Smi> offset);

  // Allocate an appropriate one- or two-byte ConsString with the first and
  // second parts specified by |left| and |right|.
  TNode<String> AllocateConsString(TNode<Uint32T> length, TNode<String> left,
                                   TNode<String> right);

  TNode<NameDictionary> AllocateNameDictionary(int at_least_space_for);
  TNode<NameDictionary> AllocateNameDictionary(
      TNode<IntPtrT> at_least_space_for, AllocationFlags = kNone);
  TNode<NameDictionary> AllocateNameDictionaryWithCapacity(
      TNode<IntPtrT> capacity, AllocationFlags = kNone);
  TNode<NameDictionary> CopyNameDictionary(TNode<NameDictionary> dictionary,
                                           Label* large_object_fallback);

  template <typename CollectionType>
  Node* AllocateOrderedHashTable();

  // Builds code that finds OrderedHashTable entry for a key with hash code
  // {hash} with using the comparison code generated by {key_compare}. The code
  // jumps to {entry_found} if the key is found, or to {not_found} if the key
  // was not found. In the {entry_found} branch, the variable
  // entry_start_position will be bound to the index of the entry (relative to
  // OrderedHashTable::kHashTableStartIndex).
  //
  // The {CollectionType} template parameter stands for the particular instance
  // of OrderedHashTable, it should be OrderedHashMap or OrderedHashSet.
  template <typename CollectionType>
  void FindOrderedHashTableEntry(
      Node* table, Node* hash,
      const std::function<void(Node*, Label*, Label*)>& key_compare,
      Variable* entry_start_position, Label* entry_found, Label* not_found);

  template <typename CollectionType>
  TNode<CollectionType> AllocateSmallOrderedHashTable(TNode<IntPtrT> capacity);

  Node* AllocateStruct(Node* map, AllocationFlags flags = kNone);
  void InitializeStructBody(Node* object, Node* map, Node* size,
                            int start_offset = Struct::kHeaderSize);

  TNode<JSObject> AllocateJSObjectFromMap(
      SloppyTNode<Map> map, SloppyTNode<HeapObject> properties = nullptr,
      SloppyTNode<FixedArray> elements = nullptr, AllocationFlags flags = kNone,
      SlackTrackingMode slack_tracking_mode = kNoSlackTracking);

  void InitializeJSObjectFromMap(
      Node* object, Node* map, Node* instance_size, Node* properties = nullptr,
      Node* elements = nullptr,
      SlackTrackingMode slack_tracking_mode = kNoSlackTracking);

  void InitializeJSObjectBodyWithSlackTracking(Node* object, Node* map,
                                               Node* instance_size);
  void InitializeJSObjectBodyNoSlackTracking(
      Node* object, Node* map, Node* instance_size,
      int start_offset = JSObject::kHeaderSize);

  TNode<BoolT> IsValidFastJSArrayCapacity(Node* capacity,
                                          ParameterMode capacity_mode);

  //
  // Allocate and return a JSArray with initialized header fields and its
  // uninitialized elements.
  // The ParameterMode argument is only used for the capacity parameter.
  std::pair<TNode<JSArray>, TNode<FixedArrayBase>>
  AllocateUninitializedJSArrayWithElements(
      ElementsKind kind, TNode<Map> array_map, TNode<Smi> length,
      Node* allocation_site, Node* capacity,
      ParameterMode capacity_mode = INTPTR_PARAMETERS,
      AllocationFlags allocation_flags = kNone,
      int array_header_size = JSArray::kSize);

  // Allocate a JSArray and fill elements with the hole.
  // The ParameterMode argument is only used for the capacity parameter.
  TNode<JSArray> AllocateJSArray(
      ElementsKind kind, TNode<Map> array_map, Node* capacity,
      TNode<Smi> length, Node* allocation_site = nullptr,
      ParameterMode capacity_mode = INTPTR_PARAMETERS,
      AllocationFlags allocation_flags = kNone);

  TNode<JSArray> AllocateJSArray(ElementsKind kind, TNode<Map> array_map,
                                 TNode<Smi> capacity, TNode<Smi> length) {
    return AllocateJSArray(kind, array_map, capacity, length, nullptr,
                           SMI_PARAMETERS);
  }

  TNode<JSArray> AllocateJSArray(ElementsKind kind, TNode<Map> array_map,
                                 TNode<IntPtrT> capacity, TNode<Smi> length,
                                 AllocationFlags allocation_flags = kNone) {
    return AllocateJSArray(kind, array_map, capacity, length, nullptr,
                           INTPTR_PARAMETERS, allocation_flags);
  }

  // Allocate a JSArray and initialize the header fields.
  TNode<JSArray> AllocateJSArray(TNode<Map> array_map,
                                 TNode<FixedArrayBase> elements,
                                 TNode<Smi> length,
                                 Node* allocation_site = nullptr,
                                 int array_header_size = JSArray::kSize);

  enum class HoleConversionMode { kDontConvert, kConvertToUndefined };
  // Clone a fast JSArray |array| into a new fast JSArray.
  // |convert_holes| tells the function to convert holes into undefined or not.
  // If |convert_holes| is set to kConvertToUndefined, but the function did not
  // find any hole in |array|, the resulting array will have the same elements
  // kind as |array|. If the function did find a hole, it will convert holes in
  // |array| to undefined in the resulting array, who will now have
  // PACKED_ELEMENTS kind.
  // If |convert_holes| is set kDontConvert, holes are also copied to the
  // resulting array, who will have the same elements kind as |array|. The
  // function generates significantly less code in this case.
  Node* CloneFastJSArray(
      Node* context, Node* array, ParameterMode mode = INTPTR_PARAMETERS,
      Node* allocation_site = nullptr,
      HoleConversionMode convert_holes = HoleConversionMode::kDontConvert);

  Node* ExtractFastJSArray(Node* context, Node* array, Node* begin, Node* count,
                           ParameterMode mode = INTPTR_PARAMETERS,
                           Node* capacity = nullptr,
                           Node* allocation_site = nullptr);

  TNode<FixedArrayBase> AllocateFixedArray(
      ElementsKind kind, Node* capacity, ParameterMode mode = INTPTR_PARAMETERS,
      AllocationFlags flags = kNone,
      SloppyTNode<Map> fixed_array_map = nullptr);

  TNode<FixedArrayBase> AllocateFixedArray(
      ElementsKind kind, TNode<IntPtrT> capacity, AllocationFlags flags,
      SloppyTNode<Map> fixed_array_map = nullptr) {
    return AllocateFixedArray(kind, capacity, INTPTR_PARAMETERS, flags,
                              fixed_array_map);
  }

  TNode<Map> GetStructMap(InstanceType instance_type);

  TNode<FixedArray> AllocateUninitializedFixedArray(intptr_t capacity) {
    return UncheckedCast<FixedArray>(AllocateFixedArray(
        PACKED_ELEMENTS, IntPtrConstant(capacity), AllocationFlag::kNone));
  }

  TNode<FixedArray> AllocateZeroedFixedArray(TNode<IntPtrT> capacity) {
    TNode<FixedArray> result = UncheckedCast<FixedArray>(
        AllocateFixedArray(PACKED_ELEMENTS, capacity,
                           AllocationFlag::kAllowLargeObjectAllocation));
    FillFixedArrayWithSmiZero(result, capacity);
    return result;
  }

  TNode<FixedDoubleArray> AllocateZeroedFixedDoubleArray(
      TNode<IntPtrT> capacity) {
    TNode<FixedDoubleArray> result = UncheckedCast<FixedDoubleArray>(
        AllocateFixedArray(PACKED_DOUBLE_ELEMENTS, capacity,
                           AllocationFlag::kAllowLargeObjectAllocation));
    FillFixedDoubleArrayWithZero(result, capacity);
    return result;
  }

  TNode<FixedArray> AllocateFixedArrayWithHoles(TNode<IntPtrT> capacity,
                                                AllocationFlags flags) {
    TNode<FixedArray> result = UncheckedCast<FixedArray>(
        AllocateFixedArray(PACKED_ELEMENTS, capacity, flags));
    FillFixedArrayWithValue(PACKED_ELEMENTS, result, IntPtrConstant(0),
                            capacity, RootIndex::kTheHoleValue);
    return result;
  }

  TNode<FixedDoubleArray> AllocateFixedDoubleArrayWithHoles(
      TNode<IntPtrT> capacity, AllocationFlags flags) {
    TNode<FixedDoubleArray> result = UncheckedCast<FixedDoubleArray>(
        AllocateFixedArray(PACKED_DOUBLE_ELEMENTS, capacity, flags));
    FillFixedArrayWithValue(PACKED_DOUBLE_ELEMENTS, result, IntPtrConstant(0),
                            capacity, RootIndex::kTheHoleValue);
    return result;
  }

  Node* AllocatePropertyArray(Node* capacity,
                              ParameterMode mode = INTPTR_PARAMETERS,
                              AllocationFlags flags = kNone);

  // Perform CreateArrayIterator (ES #sec-createarrayiterator).
  TNode<JSArrayIterator> CreateArrayIterator(TNode<Context> context,
                                             TNode<Object> object,
                                             IterationKind mode);

  TNode<JSObject> AllocateJSIteratorResult(SloppyTNode<Context> context,
                                           SloppyTNode<Object> value,
                                           SloppyTNode<Oddball> done);
  Node* AllocateJSIteratorResultForEntry(Node* context, Node* key, Node* value);

  TNode<JSReceiver> ArraySpeciesCreate(TNode<Context> context,
                                       TNode<Object> originalArray,
                                       TNode<Number> len);

  void FillFixedArrayWithValue(ElementsKind kind, Node* array, Node* from_index,
                               Node* to_index, RootIndex value_root_index,
                               ParameterMode mode = INTPTR_PARAMETERS);

  // Uses memset to effectively initialize the given FixedArray with zeroes.
  void FillFixedArrayWithSmiZero(TNode<FixedArray> array,
                                 TNode<IntPtrT> length);
  void FillFixedDoubleArrayWithZero(TNode<FixedDoubleArray> array,
                                    TNode<IntPtrT> length);

  void FillPropertyArrayWithUndefined(Node* array, Node* from_index,
                                      Node* to_index,
                                      ParameterMode mode = INTPTR_PARAMETERS);

  enum class DestroySource { kNo, kYes };

  // Specify DestroySource::kYes if {from_array} is being supplanted by
  // {to_array}. This offers a slight performance benefit by simply copying the
  // array word by word. The source may be destroyed at the end of this macro.
  //
  // Otherwise, specify DestroySource::kNo for operations where an Object is
  // being cloned, to ensure that MutableHeapNumbers are unique between the
  // source and cloned object.
  void CopyPropertyArrayValues(Node* from_array, Node* to_array, Node* length,
                               WriteBarrierMode barrier_mode,
                               ParameterMode mode,
                               DestroySource destroy_source);

  // Copies all elements from |from_array| of |length| size to
  // |to_array| of the same size respecting the elements kind.
  void CopyFixedArrayElements(
      ElementsKind kind, Node* from_array, Node* to_array, Node* length,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS) {
    CopyFixedArrayElements(kind, from_array, kind, to_array,
                           IntPtrOrSmiConstant(0, mode), length, length,
                           barrier_mode, mode);
  }

  // Copies |element_count| elements from |from_array| starting from element
  // zero to |to_array| of |capacity| size respecting both array's elements
  // kinds.
  void CopyFixedArrayElements(
      ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
      Node* to_array, Node* element_count, Node* capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS) {
    CopyFixedArrayElements(from_kind, from_array, to_kind, to_array,
                           IntPtrOrSmiConstant(0, mode), element_count,
                           capacity, barrier_mode, mode);
  }

  // Copies |element_count| elements from |from_array| starting from element
  // |first_element| to |to_array| of |capacity| size respecting both array's
  // elements kinds.
  // |convert_holes| tells the function whether to convert holes to undefined.
  // |var_holes_converted| can be used to signify that the conversion happened
  // (i.e. that there were holes). If |convert_holes_to_undefined| is
  // HoleConversionMode::kConvertToUndefined, then it must not be the case that
  // IsDoubleElementsKind(to_kind).
  void CopyFixedArrayElements(
      ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
      Node* to_array, Node* first_element, Node* element_count, Node* capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS,
      HoleConversionMode convert_holes = HoleConversionMode::kDontConvert,
      TVariable<BoolT>* var_holes_converted = nullptr);

  void CopyFixedArrayElements(
      ElementsKind from_kind, TNode<FixedArrayBase> from_array,
      ElementsKind to_kind, TNode<FixedArrayBase> to_array,
      TNode<Smi> first_element, TNode<Smi> element_count, TNode<Smi> capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER) {
    CopyFixedArrayElements(from_kind, from_array, to_kind, to_array,
                           first_element, element_count, capacity, barrier_mode,
                           SMI_PARAMETERS);
  }

  void JumpIfPointersFromHereAreInteresting(TNode<Object> object,
                                            Label* interesting);

  // Efficiently copy elements within a single array. The regions
  // [src_index, src_index + length) and [dst_index, dst_index + length)
  // can be overlapping.
  void MoveElements(ElementsKind kind, TNode<FixedArrayBase> elements,
                    TNode<IntPtrT> dst_index, TNode<IntPtrT> src_index,
                    TNode<IntPtrT> length);

  // Efficiently copy elements from one array to another. The ElementsKind
  // needs to be the same. Copy from src_elements at
  // [src_index, src_index + length) to dst_elements at
  // [dst_index, dst_index + length).
  // The function decides whether it can use memcpy. In case it cannot,
  // |write_barrier| can help it to skip write barrier. SKIP_WRITE_BARRIER is
  // only safe when copying to new space, or when copying to old space and the
  // array does not contain object pointers.
  void CopyElements(ElementsKind kind, TNode<FixedArrayBase> dst_elements,
                    TNode<IntPtrT> dst_index,
                    TNode<FixedArrayBase> src_elements,
                    TNode<IntPtrT> src_index, TNode<IntPtrT> length,
                    WriteBarrierMode write_barrier = UPDATE_WRITE_BARRIER);

  TNode<FixedArray> HeapObjectToFixedArray(TNode<HeapObject> base,
                                           Label* cast_fail);

  TNode<FixedDoubleArray> HeapObjectToFixedDoubleArray(TNode<HeapObject> base,
                                                       Label* cast_fail) {
    GotoIf(
        WordNotEqual(LoadMap(base), LoadRoot(RootIndex::kFixedDoubleArrayMap)),
        cast_fail);
    return UncheckedCast<FixedDoubleArray>(base);
  }

  TNode<SloppyArgumentsElements> HeapObjectToSloppyArgumentsElements(
      TNode<HeapObject> base, Label* cast_fail) {
    GotoIf(WordNotEqual(LoadMap(base),
                        LoadRoot(RootIndex::kSloppyArgumentsElementsMap)),
           cast_fail);
    return UncheckedCast<SloppyArgumentsElements>(base);
  }

  TNode<Int32T> ConvertElementsKindToInt(TNode<Int32T> elements_kind) {
    return UncheckedCast<Int32T>(elements_kind);
  }

  enum class ExtractFixedArrayFlag {
    kFixedArrays = 1,
    kFixedDoubleArrays = 2,
    kDontCopyCOW = 4,
    kNewSpaceAllocationOnly = 8,
    kAllFixedArrays = kFixedArrays | kFixedDoubleArrays,
    kAllFixedArraysDontCopyCOW = kAllFixedArrays | kDontCopyCOW
  };

  using ExtractFixedArrayFlags = base::Flags<ExtractFixedArrayFlag>;

  // Copy a portion of an existing FixedArray or FixedDoubleArray into a new
  // array, including special appropriate handling for empty arrays and COW
  // arrays. The result array will be of the same type as the original array.
  //
  // * |source| is either a FixedArray or FixedDoubleArray from which to copy
  // elements.
  // * |first| is the starting element index to copy from, if nullptr is passed
  // then index zero is used by default.
  // * |count| is the number of elements to copy out of the source array
  // starting from and including the element indexed by |start|. If |count| is
  // nullptr, then all of the elements from |start| to the end of |source| are
  // copied.
  // * |capacity| determines the size of the allocated result array, with
  // |capacity| >= |count|. If |capacity| is nullptr, then |count| is used as
  // the destination array's capacity.
  // * |extract_flags| determines whether FixedArrays, FixedDoubleArrays or both
  // are detected and copied. Although it's always correct to pass
  // kAllFixedArrays, the generated code is more compact and efficient if the
  // caller can specify whether only FixedArrays or FixedDoubleArrays will be
  // passed as the |source| parameter.
  // * |parameter_mode| determines the parameter mode of |first|, |count| and
  // |capacity|.
  // * If |var_holes_converted| is given, any holes will be converted to
  // undefined and the variable will be set according to whether or not there
  // were any hole.
  // * If |source_elements_kind| is given, the function will try to use the
  // runtime elements kind of source to make copy faster. More specifically, it
  // can skip write barriers.
  TNode<FixedArrayBase> ExtractFixedArray(
      Node* source, Node* first, Node* count = nullptr,
      Node* capacity = nullptr,
      ExtractFixedArrayFlags extract_flags =
          ExtractFixedArrayFlag::kAllFixedArrays,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      TVariable<BoolT>* var_holes_converted = nullptr,
      Node* source_elements_kind = nullptr);

  TNode<FixedArrayBase> ExtractFixedArray(
      TNode<FixedArrayBase> source, TNode<Smi> first, TNode<Smi> count,
      TNode<Smi> capacity,
      ExtractFixedArrayFlags extract_flags =
          ExtractFixedArrayFlag::kAllFixedArrays) {
    return ExtractFixedArray(source, first, count, capacity, extract_flags,
                             SMI_PARAMETERS);
  }

  TNode<FixedArray> ExtractFixedArray(
      TNode<FixedArray> source, TNode<IntPtrT> first, TNode<IntPtrT> count,
      TNode<IntPtrT> capacity,
      ExtractFixedArrayFlags extract_flags =
          ExtractFixedArrayFlag::kAllFixedArrays) {
    return CAST(ExtractFixedArray(source, first, count, capacity, extract_flags,
                                  INTPTR_PARAMETERS));
  }

  // Copy a portion of an existing FixedArray or FixedDoubleArray into a new
  // FixedArray, including special appropriate handling for COW arrays.
  // * |source| is either a FixedArray or FixedDoubleArray from which to copy
  // elements. |source| is assumed to be non-empty.
  // * |first| is the starting element index to copy from.
  // * |count| is the number of elements to copy out of the source array
  // starting from and including the element indexed by |start|.
  // * |capacity| determines the size of the allocated result array, with
  // |capacity| >= |count|.
  // * |source_map| is the map of the |source|.
  // * |from_kind| is the elements kind that is consistent with |source| being
  // a FixedArray or FixedDoubleArray. This function only cares about double vs.
  // non-double, so as to distinguish FixedDoubleArray vs. FixedArray. It does
  // not care about holeyness. For example, when |source| is a FixedArray,
  // PACKED/HOLEY_ELEMENTS can be used, but not PACKED_DOUBLE_ELEMENTS.
  // * |allocation_flags| and |extract_flags| influence how the target
  // FixedArray is allocated.
  // * |parameter_mode| determines the parameter mode of |first|, |count| and
  // |capacity|.
  // * |convert_holes| is used to signify that the target array should use
  // undefined in places of holes.
  // * If |convert_holes| is true and |var_holes_converted| not nullptr, then
  // |var_holes_converted| is used to signal whether any holes were found and
  // converted. The caller should use this information to decide which map is
  // compatible with the result array. For example, if the input was of
  // HOLEY_SMI_ELEMENTS kind, and a conversion took place, the result will be
  // compatible only with HOLEY_ELEMENTS and PACKED_ELEMENTS.
  TNode<FixedArray> ExtractToFixedArray(
      Node* source, Node* first, Node* count, Node* capacity, Node* source_map,
      ElementsKind from_kind = PACKED_ELEMENTS,
      AllocationFlags allocation_flags = AllocationFlag::kNone,
      ExtractFixedArrayFlags extract_flags =
          ExtractFixedArrayFlag::kAllFixedArrays,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      HoleConversionMode convert_holes = HoleConversionMode::kDontConvert,
      TVariable<BoolT>* var_holes_converted = nullptr,
      Node* source_runtime_kind = nullptr);

  // Attempt to copy a FixedDoubleArray to another FixedDoubleArray. In the case
  // where the source array has a hole, produce a FixedArray instead where holes
  // are replaced with undefined.
  // * |source| is a FixedDoubleArray from which to copy elements.
  // * |first| is the starting element index to copy from.
  // * |count| is the number of elements to copy out of the source array
  // starting from and including the element indexed by |start|.
  // * |capacity| determines the size of the allocated result array, with
  // |capacity| >= |count|.
  // * |source_map| is the map of |source|. It will be used as the map of the
  // target array if the target can stay a FixedDoubleArray. Otherwise if the
  // target array needs to be a FixedArray, the FixedArrayMap will be used.
  // * |var_holes_converted| is used to signal whether a FixedAray
  // is produced or not.
  // * |allocation_flags| and |extract_flags| influence how the target array is
  // allocated.
  // * |parameter_mode| determines the parameter mode of |first|, |count| and
  // |capacity|.
  TNode<FixedArrayBase> ExtractFixedDoubleArrayFillingHoles(
      Node* source, Node* first, Node* count, Node* capacity, Node* source_map,
      TVariable<BoolT>* var_holes_converted, AllocationFlags allocation_flags,
      ExtractFixedArrayFlags extract_flags =
          ExtractFixedArrayFlag::kAllFixedArrays,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  // Copy the entire contents of a FixedArray or FixedDoubleArray to a new
  // array, including special appropriate handling for empty arrays and COW
  // arrays.
  //
  // * |source| is either a FixedArray or FixedDoubleArray from which to copy
  // elements.
  // * |extract_flags| determines whether FixedArrays, FixedDoubleArrays or both
  // are detected and copied. Although it's always correct to pass
  // kAllFixedArrays, the generated code is more compact and efficient if the
  // caller can specify whether only FixedArrays or FixedDoubleArrays will be
  // passed as the |source| parameter.
  Node* CloneFixedArray(Node* source,
                        ExtractFixedArrayFlags flags =
                            ExtractFixedArrayFlag::kAllFixedArraysDontCopyCOW) {
    ParameterMode mode = OptimalParameterMode();
    return ExtractFixedArray(source, IntPtrOrSmiConstant(0, mode), nullptr,
                             nullptr, flags, mode);
  }

  // Copies |character_count| elements from |from_string| to |to_string|
  // starting at the |from_index|'th character. |from_string| and |to_string|
  // can either be one-byte strings or two-byte strings, although if
  // |from_string| is two-byte, then |to_string| must be two-byte.
  // |from_index|, |to_index| and |character_count| must be intptr_ts s.t. 0 <=
  // |from_index| <= |from_index| + |character_count| <= from_string.length and
  // 0 <= |to_index| <= |to_index| + |character_count| <= to_string.length.
  void CopyStringCharacters(Node* from_string, Node* to_string,
                            TNode<IntPtrT> from_index, TNode<IntPtrT> to_index,
                            TNode<IntPtrT> character_count,
                            String::Encoding from_encoding,
                            String::Encoding to_encoding);

  // Loads an element from |array| of |from_kind| elements by given |offset|
  // (NOTE: not index!), does a hole check if |if_hole| is provided and
  // converts the value so that it becomes ready for storing to array of
  // |to_kind| elements.
  Node* LoadElementAndPrepareForStore(Node* array, Node* offset,
                                      ElementsKind from_kind,
                                      ElementsKind to_kind, Label* if_hole);

  Node* CalculateNewElementsCapacity(Node* old_capacity,
                                     ParameterMode mode = INTPTR_PARAMETERS);

  TNode<Smi> CalculateNewElementsCapacity(TNode<Smi> old_capacity) {
    return CAST(CalculateNewElementsCapacity(old_capacity, SMI_PARAMETERS));
  }
  TNode<IntPtrT> CalculateNewElementsCapacity(TNode<IntPtrT> old_capacity) {
    return UncheckedCast<IntPtrT>(
        CalculateNewElementsCapacity(old_capacity, INTPTR_PARAMETERS));
  }

  // Tries to grow the |elements| array of given |object| to store the |key|
  // or bails out if the growing gap is too big. Returns new elements.
  Node* TryGrowElementsCapacity(Node* object, Node* elements, ElementsKind kind,
                                Node* key, Label* bailout);

  // Tries to grow the |capacity|-length |elements| array of given |object|
  // to store the |key| or bails out if the growing gap is too big. Returns
  // new elements.
  Node* TryGrowElementsCapacity(Node* object, Node* elements, ElementsKind kind,
                                Node* key, Node* capacity, ParameterMode mode,
                                Label* bailout);

  // Grows elements capacity of given object. Returns new elements.
  Node* GrowElementsCapacity(Node* object, Node* elements,
                             ElementsKind from_kind, ElementsKind to_kind,
                             Node* capacity, Node* new_capacity,
                             ParameterMode mode, Label* bailout);

  // Given a need to grow by |growth|, allocate an appropriate new capacity
  // if necessary, and return a new elements FixedArray object. Label |bailout|
  // is followed for allocation failure.
  void PossiblyGrowElementsCapacity(ParameterMode mode, ElementsKind kind,
                                    Node* array, Node* length,
                                    Variable* var_elements, Node* growth,
                                    Label* bailout);

  // Allocation site manipulation
  void InitializeAllocationMemento(Node* base_allocation,
                                   Node* base_allocation_size,
                                   Node* allocation_site);

  Node* TryTaggedToFloat64(Node* value, Label* if_valueisnotnumber);
  Node* TruncateTaggedToFloat64(Node* context, Node* value);
  Node* TruncateTaggedToWord32(Node* context, Node* value);
  void TaggedToWord32OrBigInt(Node* context, Node* value, Label* if_number,
                              Variable* var_word32, Label* if_bigint,
                              Variable* var_bigint);
  void TaggedToWord32OrBigIntWithFeedback(
      Node* context, Node* value, Label* if_number, Variable* var_word32,
      Label* if_bigint, Variable* var_bigint, Variable* var_feedback);

  // Truncate the floating point value of a HeapNumber to an Int32.
  TNode<Int32T> TruncateHeapNumberValueToWord32(TNode<HeapNumber> object);

  // Conversions.
  void TryHeapNumberToSmi(TNode<HeapNumber> number,
                          TVariable<Smi>& output,  // NOLINT(runtime/references)
                          Label* if_smi);
  void TryFloat64ToSmi(TNode<Float64T> number,
                       TVariable<Smi>& output,  // NOLINT(runtime/references)
                       Label* if_smi);
  TNode<Number> ChangeFloat64ToTagged(SloppyTNode<Float64T> value);
  TNode<Number> ChangeInt32ToTagged(SloppyTNode<Int32T> value);
  TNode<Number> ChangeUint32ToTagged(SloppyTNode<Uint32T> value);
  TNode<Number> ChangeUintPtrToTagged(TNode<UintPtrT> value);
  TNode<Uint32T> ChangeNumberToUint32(TNode<Number> value);
  TNode<Float64T> ChangeNumberToFloat64(TNode<Number> value);
  TNode<UintPtrT> TryNumberToUintPtr(TNode<Number> value, Label* if_negative);
  TNode<UintPtrT> ChangeNonnegativeNumberToUintPtr(TNode<Number> value) {
    return TryNumberToUintPtr(value, nullptr);
  }

  void TaggedToNumeric(Node* context, Node* value, Label* done,
                       Variable* var_numeric);
  void TaggedToNumericWithFeedback(Node* context, Node* value, Label* done,
                                   Variable* var_numeric,
                                   Variable* var_feedback);

  TNode<WordT> TimesSystemPointerSize(SloppyTNode<WordT> value);
  TNode<IntPtrT> TimesSystemPointerSize(TNode<IntPtrT> value) {
    return Signed(TimesSystemPointerSize(implicit_cast<TNode<WordT>>(value)));
  }
  TNode<UintPtrT> TimesSystemPointerSize(TNode<UintPtrT> value) {
    return Unsigned(TimesSystemPointerSize(implicit_cast<TNode<WordT>>(value)));
  }

  TNode<WordT> TimesTaggedSize(SloppyTNode<WordT> value);
  TNode<IntPtrT> TimesTaggedSize(TNode<IntPtrT> value) {
    return Signed(TimesTaggedSize(implicit_cast<TNode<WordT>>(value)));
  }
  TNode<UintPtrT> TimesTaggedSize(TNode<UintPtrT> value) {
    return Unsigned(TimesTaggedSize(implicit_cast<TNode<WordT>>(value)));
  }

  TNode<WordT> TimesDoubleSize(SloppyTNode<WordT> value);
  TNode<UintPtrT> TimesDoubleSize(TNode<UintPtrT> value) {
    return Unsigned(TimesDoubleSize(implicit_cast<TNode<WordT>>(value)));
  }
  TNode<IntPtrT> TimesDoubleSize(TNode<IntPtrT> value) {
    return Signed(TimesDoubleSize(implicit_cast<TNode<WordT>>(value)));
  }

  // Type conversions.
  // Throws a TypeError for {method_name} if {value} is not coercible to Object,
  // or returns the {value} converted to a String otherwise.
  TNode<String> ToThisString(TNode<Context> context, TNode<Object> value,
                             TNode<String> method_name);
  TNode<String> ToThisString(TNode<Context> context, TNode<Object> value,
                             char const* method_name) {
    return ToThisString(context, value, StringConstant(method_name));
  }

  // Throws a TypeError for {method_name} if {value} is neither of the given
  // {primitive_type} nor a JSPrimitiveWrapper wrapping a value of
  // {primitive_type}, or returns the {value} (or wrapped value) otherwise.
  TNode<Object> ToThisValue(TNode<Context> context, TNode<Object> value,
                            PrimitiveType primitive_type,
                            char const* method_name);

  // Throws a TypeError for {method_name} if {value} is not of the given
  // instance type. Returns {value}'s map.
  Node* ThrowIfNotInstanceType(Node* context, Node* value,
                               InstanceType instance_type,
                               char const* method_name);
  // Throws a TypeError for {method_name} if {value} is not a JSReceiver.
  // Returns the {value}'s map.
  Node* ThrowIfNotJSReceiver(Node* context, Node* value,
                             MessageTemplate msg_template,
                             const char* method_name = nullptr);
  void ThrowIfNotCallable(TNode<Context> context, TNode<Object> value,
                          const char* method_name);

  void ThrowRangeError(Node* context, MessageTemplate message,
                       Node* arg0 = nullptr, Node* arg1 = nullptr,
                       Node* arg2 = nullptr);
  void ThrowTypeError(Node* context, MessageTemplate message,
                      char const* arg0 = nullptr, char const* arg1 = nullptr);
  void ThrowTypeError(Node* context, MessageTemplate message, Node* arg0,
                      Node* arg1 = nullptr, Node* arg2 = nullptr);

  // Type checks.
  // Check whether the map is for an object with special properties, such as a
  // JSProxy or an object with interceptors.
  TNode<BoolT> InstanceTypeEqual(SloppyTNode<Int32T> instance_type, int type);
  TNode<BoolT> IsAccessorInfo(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsAccessorPair(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsAllocationSite(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsAnyHeapNumber(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsNoElementsProtectorCellInvalid();
  TNode<BoolT> IsArrayIteratorProtectorCellInvalid();
  TNode<BoolT> IsBigIntInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsBigInt(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsBoolean(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsCallableMap(SloppyTNode<Map> map);
  TNode<BoolT> IsCallable(SloppyTNode<HeapObject> object);
  TNode<BoolT> TaggedIsCallable(TNode<Object> object);
  TNode<BoolT> IsCell(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsCode(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsConsStringInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsConstructorMap(SloppyTNode<Map> map);
  TNode<BoolT> IsConstructor(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsDebugInfo(TNode<HeapObject> object);
  TNode<BoolT> IsDeprecatedMap(SloppyTNode<Map> map);
  TNode<BoolT> IsNameDictionary(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsGlobalDictionary(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsExtensibleMap(SloppyTNode<Map> map);
  TNode<BoolT> IsFrozenOrSealedElementsKindMap(SloppyTNode<Map> map);
  TNode<BoolT> IsExtensibleNonPrototypeMap(TNode<Map> map);
  TNode<BoolT> IsExternalStringInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsFeedbackCell(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsFeedbackVector(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsContext(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsFixedArray(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsFixedArraySubclass(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsFixedArrayWithKind(SloppyTNode<HeapObject> object,
                                    ElementsKind kind);
  TNode<BoolT> IsFixedArrayWithKindOrEmpty(SloppyTNode<HeapObject> object,
                                           ElementsKind kind);
  TNode<BoolT> IsFixedDoubleArray(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsFunctionWithPrototypeSlotMap(SloppyTNode<Map> map);
  TNode<BoolT> IsHashTable(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsEphemeronHashTable(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsHeapNumber(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsHeapNumberInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsOddball(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsOddballInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsIndirectStringInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSArrayBuffer(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSDataView(TNode<HeapObject> object);
  TNode<BoolT> IsJSArrayInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSArrayMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSArray(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSArrayIterator(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSAsyncGeneratorObject(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSFunctionInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsAllocationSiteInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSFunctionMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSFunction(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSGeneratorObject(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSGlobalProxyInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSGlobalProxyMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSGlobalProxy(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSObjectInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSObjectMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSObject(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSPromiseMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSPromise(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSProxy(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSStringIterator(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSReceiverInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSReceiverMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSReceiver(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSRegExp(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSTypedArrayInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSTypedArrayMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSTypedArray(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsJSPrimitiveWrapperInstanceType(
      SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsJSPrimitiveWrapperMap(SloppyTNode<Map> map);
  TNode<BoolT> IsJSPrimitiveWrapper(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsMap(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsMutableHeapNumber(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsName(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsNameInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsNativeContext(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsNullOrJSReceiver(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsNullOrUndefined(SloppyTNode<Object> object);
  TNode<BoolT> IsNumberDictionary(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsOneByteStringInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsPrimitiveInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsPrivateSymbol(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsPrivateName(SloppyTNode<Symbol> symbol);
  TNode<BoolT> IsPromiseCapability(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsPropertyArray(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsPropertyCell(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsPrototypeInitialArrayPrototype(SloppyTNode<Context> context,
                                                SloppyTNode<Map> map);
  TNode<BoolT> IsPrototypeTypedArrayPrototype(SloppyTNode<Context> context,
                                              SloppyTNode<Map> map);

  TNode<BoolT> IsFastAliasedArgumentsMap(TNode<Context> context,
                                         TNode<Map> map);
  TNode<BoolT> IsSlowAliasedArgumentsMap(TNode<Context> context,
                                         TNode<Map> map);
  TNode<BoolT> IsSloppyArgumentsMap(TNode<Context> context, TNode<Map> map);
  TNode<BoolT> IsStrictArgumentsMap(TNode<Context> context, TNode<Map> map);

  TNode<BoolT> IsSequentialStringInstanceType(
      SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsUncachedExternalStringInstanceType(
      SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsSpecialReceiverInstanceType(TNode<Int32T> instance_type);
  TNode<BoolT> IsCustomElementsReceiverInstanceType(
      TNode<Int32T> instance_type);
  TNode<BoolT> IsSpecialReceiverMap(SloppyTNode<Map> map);
  // Returns true if the map corresponds to non-special fast or dictionary
  // object.
  TNode<BoolT> IsSimpleObjectMap(TNode<Map> map);
  TNode<BoolT> IsStringInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsString(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsSymbolInstanceType(SloppyTNode<Int32T> instance_type);
  TNode<BoolT> IsSymbol(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsInternalizedStringInstanceType(TNode<Int32T> instance_type);
  TNode<BoolT> IsUniqueName(TNode<HeapObject> object);
  TNode<BoolT> IsUniqueNameNoIndex(TNode<HeapObject> object);
  TNode<BoolT> IsUndetectableMap(SloppyTNode<Map> map);
  TNode<BoolT> IsNotWeakFixedArraySubclass(SloppyTNode<HeapObject> object);
  TNode<BoolT> IsZeroOrContext(SloppyTNode<Object> object);

  inline Node* IsSharedFunctionInfo(Node* object) {
    return IsSharedFunctionInfoMap(LoadMap(object));
  }

  TNode<BoolT> IsPromiseResolveProtectorCellInvalid();
  TNode<BoolT> IsPromiseThenProtectorCellInvalid();
  TNode<BoolT> IsArraySpeciesProtectorCellInvalid();
  TNode<BoolT> IsTypedArraySpeciesProtectorCellInvalid();
  TNode<BoolT> IsRegExpSpeciesProtectorCellInvalid(
      TNode<Context> native_context);
  TNode<BoolT> IsPromiseSpeciesProtectorCellInvalid();

  TNode<BoolT> IsMockArrayBufferAllocatorFlag() {
    TNode<Word32T> flag_value = UncheckedCast<Word32T>(Load(
        MachineType::Uint8(),
        ExternalConstant(
            ExternalReference::address_of_mock_arraybuffer_allocator_flag())));
    return Word32NotEqual(Word32And(flag_value, Int32Constant(0xFF)),
                          Int32Constant(0));
  }

  // True iff |object| is a Smi or a HeapNumber.
  TNode<BoolT> IsNumber(SloppyTNode<Object> object);
  // True iff |object| is a Smi or a HeapNumber or a BigInt.
  TNode<BoolT> IsNumeric(SloppyTNode<Object> object);

  // True iff |number| is either a Smi, or a HeapNumber whose value is not
  // within Smi range.
  TNode<BoolT> IsNumberNormalized(SloppyTNode<Number> number);
  TNode<BoolT> IsNumberPositive(SloppyTNode<Number> number);
  TNode<BoolT> IsHeapNumberPositive(TNode<HeapNumber> number);

  // True iff {number} is non-negative and less or equal than 2**53-1.
  TNode<BoolT> IsNumberNonNegativeSafeInteger(TNode<Number> number);

  // True iff {number} represents an integer value.
  TNode<BoolT> IsInteger(TNode<Object> number);
  TNode<BoolT> IsInteger(TNode<HeapNumber> number);

  // True iff abs({number}) <= 2**53 -1
  TNode<BoolT> IsSafeInteger(TNode<Object> number);
  TNode<BoolT> IsSafeInteger(TNode<HeapNumber> number);

  // True iff {number} represents a valid uint32t value.
  TNode<BoolT> IsHeapNumberUint32(TNode<HeapNumber> number);

  // True iff {number} is a positive number and a valid array index in the range
  // [0, 2^32-1).
  TNode<BoolT> IsNumberArrayIndex(TNode<Number> number);

  Node* FixedArraySizeDoesntFitInNewSpace(
      Node* element_count, int base_size = FixedArray::kHeaderSize,
      ParameterMode mode = INTPTR_PARAMETERS);

  // ElementsKind helpers:
  TNode<BoolT> ElementsKindEqual(TNode<Int32T> a, TNode<Int32T> b) {
    return Word32Equal(a, b);
  }
  bool ElementsKindEqual(ElementsKind a, ElementsKind b) { return a == b; }
  TNode<BoolT> IsFastElementsKind(TNode<Int32T> elements_kind);
  bool IsFastElementsKind(ElementsKind kind) {
    return v8::internal::IsFastElementsKind(kind);
  }
  TNode<BoolT> IsDictionaryElementsKind(TNode<Int32T> elements_kind) {
    return ElementsKindEqual(elements_kind, Int32Constant(DICTIONARY_ELEMENTS));
  }
  TNode<BoolT> IsDoubleElementsKind(TNode<Int32T> elements_kind);
  bool IsDoubleElementsKind(ElementsKind kind) {
    return v8::internal::IsDoubleElementsKind(kind);
  }
  TNode<BoolT> IsFastSmiOrTaggedElementsKind(TNode<Int32T> elements_kind);
  TNode<BoolT> IsFastSmiElementsKind(SloppyTNode<Int32T> elements_kind);
  TNode<BoolT> IsHoleyFastElementsKind(TNode<Int32T> elements_kind);
  TNode<BoolT> IsHoleyFastElementsKindForRead(TNode<Int32T> elements_kind);
  TNode<BoolT> IsElementsKindGreaterThan(TNode<Int32T> target_kind,
                                         ElementsKind reference_kind);
  TNode<BoolT> IsElementsKindLessThanOrEqual(TNode<Int32T> target_kind,
                                             ElementsKind reference_kind);
  // Check if reference_kind_a <= target_kind <= reference_kind_b
  TNode<BoolT> IsElementsKindInRange(TNode<Int32T> target_kind,
                                     ElementsKind lower_reference_kind,
                                     ElementsKind higher_reference_kind);

  // String helpers.
  // Load a character from a String (might flatten a ConsString).
  TNode<Int32T> StringCharCodeAt(SloppyTNode<String> string,
                                 SloppyTNode<IntPtrT> index);
  // Return the single character string with only {code}.
  TNode<String> StringFromSingleCharCode(TNode<Int32T> code);

  // Return a new string object which holds a substring containing the range
  // [from,to[ of string.
  TNode<String> SubString(TNode<String> string, TNode<IntPtrT> from,
                          TNode<IntPtrT> to);

  // Return a new string object produced by concatenating |first| with |second|.
  TNode<String> StringAdd(Node* context, TNode<String> first,
                          TNode<String> second);

  // Check if |string| is an indirect (thin or flat cons) string type that can
  // be dereferenced by DerefIndirectString.
  void BranchIfCanDerefIndirectString(Node* string, Node* instance_type,
                                      Label* can_deref, Label* cannot_deref);
  // Unpack an indirect (thin or flat cons) string type.
  void DerefIndirectString(Variable* var_string, Node* instance_type);
  // Check if |var_string| has an indirect (thin or flat cons) string type,
  // and unpack it if so.
  void MaybeDerefIndirectString(Variable* var_string, Node* instance_type,
                                Label* did_deref, Label* cannot_deref);
  // Check if |var_left| or |var_right| has an indirect (thin or flat cons)
  // string type, and unpack it/them if so. Fall through if nothing was done.
  void MaybeDerefIndirectStrings(Variable* var_left, Node* left_instance_type,
                                 Variable* var_right, Node* right_instance_type,
                                 Label* did_something);
  Node* DerefIndirectString(TNode<String> string, TNode<Int32T> instance_type,
                            Label* cannot_deref);

  TNode<String> StringFromSingleUTF16EncodedCodePoint(TNode<Int32T> codepoint);

  // Type conversion helpers.
  enum class BigIntHandling { kConvertToNumber, kThrow };
  // Convert a String to a Number.
  TNode<Number> StringToNumber(TNode<String> input);
  // Convert a Number to a String.
  TNode<String> NumberToString(TNode<Number> input);
  // Convert a Non-Number object to a Number.
  TNode<Number> NonNumberToNumber(
      SloppyTNode<Context> context, SloppyTNode<HeapObject> input,
      BigIntHandling bigint_handling = BigIntHandling::kThrow);
  // Convert a Non-Number object to a Numeric.
  TNode<Numeric> NonNumberToNumeric(SloppyTNode<Context> context,
                                    SloppyTNode<HeapObject> input);
  // Convert any object to a Number.
  // Conforms to ES#sec-tonumber if {bigint_handling} == kThrow.
  // With {bigint_handling} == kConvertToNumber, matches behavior of
  // tc39.github.io/proposal-bigint/#sec-number-constructor-number-value.
  TNode<Number> ToNumber(
      SloppyTNode<Context> context, SloppyTNode<Object> input,
      BigIntHandling bigint_handling = BigIntHandling::kThrow);
  TNode<Number> ToNumber_Inline(SloppyTNode<Context> context,
                                SloppyTNode<Object> input);

  // Try to convert an object to a BigInt. Throws on failure (e.g. for Numbers).
  // https://tc39.github.io/proposal-bigint/#sec-to-bigint
  TNode<BigInt> ToBigInt(SloppyTNode<Context> context,
                         SloppyTNode<Object> input);

  // Converts |input| to one of 2^32 integer values in the range 0 through
  // 2^32-1, inclusive.
  // ES#sec-touint32
  TNode<Number> ToUint32(SloppyTNode<Context> context,
                         SloppyTNode<Object> input);

  // Convert any object to a String.
  TNode<String> ToString_Inline(SloppyTNode<Context> context,
                                SloppyTNode<Object> input);

  // Convert any object to a Primitive.
  Node* JSReceiverToPrimitive(Node* context, Node* input);

  TNode<JSReceiver> ToObject(SloppyTNode<Context> context,
                             SloppyTNode<Object> input);

  // Same as ToObject but avoids the Builtin call if |input| is already a
  // JSReceiver.
  TNode<JSReceiver> ToObject_Inline(TNode<Context> context,
                                    TNode<Object> input);

  enum ToIntegerTruncationMode {
    kNoTruncation,
    kTruncateMinusZero,
  };

  // ES6 7.1.17 ToIndex, but jumps to range_error if the result is not a Smi.
  TNode<Smi> ToSmiIndex(TNode<Context> context, TNode<Object> input,
                        Label* range_error);

  // ES6 7.1.15 ToLength, but jumps to range_error if the result is not a Smi.
  TNode<Smi> ToSmiLength(TNode<Context> context, TNode<Object> input,
                         Label* range_error);

  // ES6 7.1.15 ToLength, but with inlined fast path.
  TNode<Number> ToLength_Inline(SloppyTNode<Context> context,
                                SloppyTNode<Object> input);

  // ES6 7.1.4 ToInteger ( argument )
  TNode<Number> ToInteger_Inline(SloppyTNode<Context> context,
                                 SloppyTNode<Object> input,
                                 ToIntegerTruncationMode mode = kNoTruncation);
  TNode<Number> ToInteger(SloppyTNode<Context> context,
                          SloppyTNode<Object> input,
                          ToIntegerTruncationMode mode = kNoTruncation);

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word32|. Returns result as an uint32 node.
  template <typename BitField>
  TNode<Uint32T> DecodeWord32(SloppyTNode<Word32T> word32) {
    return DecodeWord32(word32, BitField::kShift, BitField::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word|. Returns result as a word-size node.
  template <typename BitField>
  TNode<UintPtrT> DecodeWord(SloppyTNode<WordT> word) {
    return DecodeWord(word, BitField::kShift, BitField::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word32|. Returns result as a word-size node.
  template <typename BitField>
  TNode<UintPtrT> DecodeWordFromWord32(SloppyTNode<Word32T> word32) {
    return DecodeWord<BitField>(ChangeUint32ToWord(word32));
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word|. Returns result as an uint32 node.
  template <typename BitField>
  TNode<Uint32T> DecodeWord32FromWord(SloppyTNode<WordT> word) {
    return UncheckedCast<Uint32T>(
        TruncateIntPtrToInt32(Signed(DecodeWord<BitField>(word))));
  }

  // Decodes an unsigned (!) value from |word32| to an uint32 node.
  TNode<Uint32T> DecodeWord32(SloppyTNode<Word32T> word32, uint32_t shift,
                              uint32_t mask);

  // Decodes an unsigned (!) value from |word| to a word-size node.
  TNode<UintPtrT> DecodeWord(SloppyTNode<WordT> word, uint32_t shift,
                             uint32_t mask);

  // Returns a node that contains the updated values of a |BitField|.
  template <typename BitField>
  TNode<WordT> UpdateWord(TNode<WordT> word, TNode<WordT> value) {
    return UpdateWord(word, value, BitField::kShift, BitField::kMask);
  }

  // Returns a node that contains the updated {value} inside {word} starting
  // at {shift} and fitting in {mask}.
  TNode<WordT> UpdateWord(TNode<WordT> word, TNode<WordT> value, uint32_t shift,
                          uint32_t mask);

  // Returns true if any of the |T|'s bits in given |word32| are set.
  template <typename T>
  TNode<BoolT> IsSetWord32(SloppyTNode<Word32T> word32) {
    return IsSetWord32(word32, T::kMask);
  }

  // Returns true if any of the mask's bits in given |word32| are set.
  TNode<BoolT> IsSetWord32(SloppyTNode<Word32T> word32, uint32_t mask) {
    return Word32NotEqual(Word32And(word32, Int32Constant(mask)),
                          Int32Constant(0));
  }

  // Returns true if none of the mask's bits in given |word32| are set.
  TNode<BoolT> IsNotSetWord32(SloppyTNode<Word32T> word32, uint32_t mask) {
    return Word32Equal(Word32And(word32, Int32Constant(mask)),
                       Int32Constant(0));
  }

  // Returns true if all of the mask's bits in a given |word32| are set.
  TNode<BoolT> IsAllSetWord32(SloppyTNode<Word32T> word32, uint32_t mask) {
    TNode<Int32T> const_mask = Int32Constant(mask);
    return Word32Equal(Word32And(word32, const_mask), const_mask);
  }

  // Returns true if any of the |T|'s bits in given |word| are set.
  template <typename T>
  TNode<BoolT> IsSetWord(SloppyTNode<WordT> word) {
    return IsSetWord(word, T::kMask);
  }

  // Returns true if any of the mask's bits in given |word| are set.
  TNode<BoolT> IsSetWord(SloppyTNode<WordT> word, uint32_t mask) {
    return WordNotEqual(WordAnd(word, IntPtrConstant(mask)), IntPtrConstant(0));
  }

  // Returns true if any of the mask's bit are set in the given Smi.
  // Smi-encoding of the mask is performed implicitly!
  TNode<BoolT> IsSetSmi(SloppyTNode<Smi> smi, int untagged_mask) {
    intptr_t mask_word = bit_cast<intptr_t>(Smi::FromInt(untagged_mask));
    return WordNotEqual(
        WordAnd(BitcastTaggedSignedToWord(smi), IntPtrConstant(mask_word)),
        IntPtrConstant(0));
  }

  // Returns true if all of the |T|'s bits in given |word32| are clear.
  template <typename T>
  TNode<BoolT> IsClearWord32(SloppyTNode<Word32T> word32) {
    return IsClearWord32(word32, T::kMask);
  }

  // Returns true if all of the mask's bits in given |word32| are clear.
  TNode<BoolT> IsClearWord32(SloppyTNode<Word32T> word32, uint32_t mask) {
    return Word32Equal(Word32And(word32, Int32Constant(mask)),
                       Int32Constant(0));
  }

  // Returns true if all of the |T|'s bits in given |word| are clear.
  template <typename T>
  TNode<BoolT> IsClearWord(SloppyTNode<WordT> word) {
    return IsClearWord(word, T::kMask);
  }

  // Returns true if all of the mask's bits in given |word| are clear.
  TNode<BoolT> IsClearWord(SloppyTNode<WordT> word, uint32_t mask) {
    return WordEqual(WordAnd(word, IntPtrConstant(mask)), IntPtrConstant(0));
  }

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int delta);
  void DecrementCounter(StatsCounter* counter, int delta);

  void Increment(Variable* variable, int value = 1,
                 ParameterMode mode = INTPTR_PARAMETERS);
  void Decrement(Variable* variable, int value = 1,
                 ParameterMode mode = INTPTR_PARAMETERS) {
    Increment(variable, -value, mode);
  }

  // Generates "if (false) goto label" code. Useful for marking a label as
  // "live" to avoid assertion failures during graph building. In the resulting
  // code this check will be eliminated.
  void Use(Label* label);

  // Various building blocks for stubs doing property lookups.

  // |if_notinternalized| is optional; |if_bailout| will be used by default.
  // Note: If |key| does not yet have a hash, |if_notinternalized| will be taken
  // even if |key| is an array index. |if_keyisunique| will never
  // be taken for array indices.
  void TryToName(Node* key, Label* if_keyisindex, Variable* var_index,
                 Label* if_keyisunique, Variable* var_unique, Label* if_bailout,
                 Label* if_notinternalized = nullptr);

  // Performs a hash computation and string table lookup for the given string,
  // and jumps to:
  // - |if_index| if the string is an array index like "123"; |var_index|
  //              will contain the intptr representation of that index.
  // - |if_internalized| if the string exists in the string table; the
  //                     internalized version will be in |var_internalized|.
  // - |if_not_internalized| if the string is not in the string table (but
  //                         does not add it).
  // - |if_bailout| for unsupported cases (e.g. uncachable array index).
  void TryInternalizeString(Node* string, Label* if_index, Variable* var_index,
                            Label* if_internalized, Variable* var_internalized,
                            Label* if_not_internalized, Label* if_bailout);

  // Calculates array index for given dictionary entry and entry field.
  // See Dictionary::EntryToIndex().
  template <typename Dictionary>
  V8_EXPORT_PRIVATE TNode<IntPtrT> EntryToIndex(TNode<IntPtrT> entry,
                                                int field_index);
  template <typename Dictionary>
  V8_EXPORT_PRIVATE TNode<IntPtrT> EntryToIndex(TNode<IntPtrT> entry) {
    return EntryToIndex<Dictionary>(entry, Dictionary::kEntryKeyIndex);
  }

  // Loads the details for the entry with the given key_index.
  // Returns an untagged int32.
  template <class ContainerType>
  TNode<Uint32T> LoadDetailsByKeyIndex(Node* container, Node* key_index) {
    static_assert(!std::is_same<ContainerType, DescriptorArray>::value,
                  "Use the non-templatized version for DescriptorArray");
    const int kKeyToDetailsOffset =
        (ContainerType::kEntryDetailsIndex - ContainerType::kEntryKeyIndex) *
        kTaggedSize;
    return Unsigned(LoadAndUntagToWord32FixedArrayElement(
        CAST(container), key_index, kKeyToDetailsOffset));
  }

  // Loads the value for the entry with the given key_index.
  // Returns a tagged value.
  template <class ContainerType>
  TNode<Object> LoadValueByKeyIndex(Node* container, Node* key_index) {
    static_assert(!std::is_same<ContainerType, DescriptorArray>::value,
                  "Use the non-templatized version for DescriptorArray");
    const int kKeyToValueOffset =
        (ContainerType::kEntryValueIndex - ContainerType::kEntryKeyIndex) *
        kTaggedSize;
    return LoadFixedArrayElement(CAST(container), key_index, kKeyToValueOffset);
  }

  // Stores the details for the entry with the given key_index.
  // |details| must be a Smi.
  template <class ContainerType>
  void StoreDetailsByKeyIndex(TNode<ContainerType> container,
                              TNode<IntPtrT> key_index, TNode<Smi> details) {
    const int kKeyToDetailsOffset =
        (ContainerType::kEntryDetailsIndex - ContainerType::kEntryKeyIndex) *
        kTaggedSize;
    StoreFixedArrayElement(container, key_index, details, SKIP_WRITE_BARRIER,
                           kKeyToDetailsOffset);
  }

  // Stores the value for the entry with the given key_index.
  template <class ContainerType>
  void StoreValueByKeyIndex(
      TNode<ContainerType> container, TNode<IntPtrT> key_index,
      TNode<Object> value,
      WriteBarrierMode write_barrier = UPDATE_WRITE_BARRIER) {
    const int kKeyToValueOffset =
        (ContainerType::kEntryValueIndex - ContainerType::kEntryKeyIndex) *
        kTaggedSize;
    StoreFixedArrayElement(container, key_index, value, write_barrier,
                           kKeyToValueOffset);
  }

  // Calculate a valid size for the a hash table.
  TNode<IntPtrT> HashTableComputeCapacity(TNode<IntPtrT> at_least_space_for);

  template <class Dictionary>
  TNode<Smi> GetNumberOfElements(TNode<Dictionary> dictionary) {
    return CAST(
        LoadFixedArrayElement(dictionary, Dictionary::kNumberOfElementsIndex));
  }

  TNode<Smi> GetNumberDictionaryNumberOfElements(
      TNode<NumberDictionary> dictionary) {
    return GetNumberOfElements<NumberDictionary>(dictionary);
  }

  template <class Dictionary>
  void SetNumberOfElements(TNode<Dictionary> dictionary,
                           TNode<Smi> num_elements_smi) {
    StoreFixedArrayElement(dictionary, Dictionary::kNumberOfElementsIndex,
                           num_elements_smi, SKIP_WRITE_BARRIER);
  }

  template <class Dictionary>
  TNode<Smi> GetNumberOfDeletedElements(TNode<Dictionary> dictionary) {
    return CAST(LoadFixedArrayElement(
        dictionary, Dictionary::kNumberOfDeletedElementsIndex));
  }

  template <class Dictionary>
  void SetNumberOfDeletedElements(TNode<Dictionary> dictionary,
                                  TNode<Smi> num_deleted_smi) {
    StoreFixedArrayElement(dictionary,
                           Dictionary::kNumberOfDeletedElementsIndex,
                           num_deleted_smi, SKIP_WRITE_BARRIER);
  }

  template <class Dictionary>
  TNode<Smi> GetCapacity(TNode<Dictionary> dictionary) {
    return CAST(
        UnsafeLoadFixedArrayElement(dictionary, Dictionary::kCapacityIndex));
  }

  template <class Dictionary>
  TNode<Smi> GetNextEnumerationIndex(TNode<Dictionary> dictionary) {
    return CAST(LoadFixedArrayElement(dictionary,
                                      Dictionary::kNextEnumerationIndexIndex));
  }

  template <class Dictionary>
  void SetNextEnumerationIndex(TNode<Dictionary> dictionary,
                               TNode<Smi> next_enum_index_smi) {
    StoreFixedArrayElement(dictionary, Dictionary::kNextEnumerationIndexIndex,
                           next_enum_index_smi, SKIP_WRITE_BARRIER);
  }

  // Looks up an entry in a NameDictionaryBase successor. If the entry is found
  // control goes to {if_found} and {var_name_index} contains an index of the
  // key field of the entry found. If the key is not found control goes to
  // {if_not_found}.
  enum LookupMode { kFindExisting, kFindInsertionIndex };

  template <typename Dictionary>
  TNode<HeapObject> LoadName(TNode<HeapObject> key);

  template <typename Dictionary>
  void NameDictionaryLookup(TNode<Dictionary> dictionary,
                            TNode<Name> unique_name, Label* if_found,
                            TVariable<IntPtrT>* var_name_index,
                            Label* if_not_found,
                            LookupMode mode = kFindExisting);

  Node* ComputeUnseededHash(Node* key);
  Node* ComputeSeededHash(Node* key);

  void NumberDictionaryLookup(TNode<NumberDictionary> dictionary,
                              TNode<IntPtrT> intptr_index, Label* if_found,
                              TVariable<IntPtrT>* var_entry,
                              Label* if_not_found);

  TNode<Object> BasicLoadNumberDictionaryElement(
      TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
      Label* not_data, Label* if_hole);
  void BasicStoreNumberDictionaryElement(TNode<NumberDictionary> dictionary,
                                         TNode<IntPtrT> intptr_index,
                                         TNode<Object> value, Label* not_data,
                                         Label* if_hole, Label* read_only);

  template <class Dictionary>
  void FindInsertionEntry(TNode<Dictionary> dictionary, TNode<Name> key,
                          TVariable<IntPtrT>* var_key_index);

  template <class Dictionary>
  void InsertEntry(TNode<Dictionary> dictionary, TNode<Name> key,
                   TNode<Object> value, TNode<IntPtrT> index,
                   TNode<Smi> enum_index);

  template <class Dictionary>
  void Add(TNode<Dictionary> dictionary, TNode<Name> key, TNode<Object> value,
           Label* bailout);

  // Tries to check if {object} has own {unique_name} property.
  void TryHasOwnProperty(Node* object, Node* map, Node* instance_type,
                         Node* unique_name, Label* if_found,
                         Label* if_not_found, Label* if_bailout);

  // Operating mode for TryGetOwnProperty and CallGetterIfAccessor
  // kReturnAccessorPair is used when we're only getting the property descriptor
  enum GetOwnPropertyMode { kCallJSGetter, kReturnAccessorPair };
  // Tries to get {object}'s own {unique_name} property value. If the property
  // is an accessor then it also calls a getter. If the property is a double
  // field it re-wraps value in an immutable heap number. {unique_name} must be
  // a unique name (Symbol or InternalizedString) that is not an array index.
  void TryGetOwnProperty(Node* context, Node* receiver, Node* object, Node* map,
                         Node* instance_type, Node* unique_name,
                         Label* if_found, Variable* var_value,
                         Label* if_not_found, Label* if_bailout);
  void TryGetOwnProperty(Node* context, Node* receiver, Node* object, Node* map,
                         Node* instance_type, Node* unique_name,
                         Label* if_found, Variable* var_value,
                         Variable* var_details, Variable* var_raw_value,
                         Label* if_not_found, Label* if_bailout,
                         GetOwnPropertyMode mode);

  TNode<Object> GetProperty(SloppyTNode<Context> context,
                            SloppyTNode<Object> receiver, Handle<Name> name) {
    return GetProperty(context, receiver, HeapConstant(name));
  }

  TNode<Object> GetProperty(SloppyTNode<Context> context,
                            SloppyTNode<Object> receiver,
                            SloppyTNode<Object> name) {
    return CallBuiltin(Builtins::kGetProperty, context, receiver, name);
  }

  TNode<Object> SetPropertyStrict(TNode<Context> context,
                                  TNode<Object> receiver, TNode<Object> key,
                                  TNode<Object> value) {
    return CallBuiltin(Builtins::kSetProperty, context, receiver, key, value);
  }

  TNode<Object> SetPropertyInLiteral(TNode<Context> context,
                                     TNode<JSObject> receiver,
                                     TNode<Object> key, TNode<Object> value) {
    return CallBuiltin(Builtins::kSetPropertyInLiteral, context, receiver, key,
                       value);
  }

  Node* GetMethod(Node* context, Node* object, Handle<Name> name,
                  Label* if_null_or_undefined);

  TNode<Object> GetIteratorMethod(TNode<Context> context,
                                  TNode<HeapObject> heap_obj,
                                  Label* if_iteratorundefined);

  template <class... TArgs>
  TNode<Object> CallBuiltin(Builtins::Name id, SloppyTNode<Object> context,
                            TArgs... args) {
    return CallStub<Object>(Builtins::CallableFor(isolate(), id), context,
                            args...);
  }

  template <class... TArgs>
  void TailCallBuiltin(Builtins::Name id, SloppyTNode<Object> context,
                       TArgs... args) {
    return TailCallStub(Builtins::CallableFor(isolate(), id), context, args...);
  }

  void LoadPropertyFromFastObject(Node* object, Node* map,
                                  TNode<DescriptorArray> descriptors,
                                  Node* name_index, Variable* var_details,
                                  Variable* var_value);

  void LoadPropertyFromFastObject(Node* object, Node* map,
                                  TNode<DescriptorArray> descriptors,
                                  Node* name_index, Node* details,
                                  Variable* var_value);

  void LoadPropertyFromNameDictionary(Node* dictionary, Node* entry,
                                      Variable* var_details,
                                      Variable* var_value);

  void LoadPropertyFromGlobalDictionary(Node* dictionary, Node* entry,
                                        Variable* var_details,
                                        Variable* var_value, Label* if_deleted);

  // Generic property lookup generator. If the {object} is fast and
  // {unique_name} property is found then the control goes to {if_found_fast}
  // label and {var_meta_storage} and {var_name_index} will contain
  // DescriptorArray and an index of the descriptor's name respectively.
  // If the {object} is slow or global then the control goes to {if_found_dict}
  // or {if_found_global} and the {var_meta_storage} and {var_name_index} will
  // contain a dictionary and an index of the key field of the found entry.
  // If property is not found or given lookup is not supported then
  // the control goes to {if_not_found} or {if_bailout} respectively.
  //
  // Note: this code does not check if the global dictionary points to deleted
  // entry! This has to be done by the caller.
  void TryLookupProperty(SloppyTNode<JSObject> object, SloppyTNode<Map> map,
                         SloppyTNode<Int32T> instance_type,
                         SloppyTNode<Name> unique_name, Label* if_found_fast,
                         Label* if_found_dict, Label* if_found_global,
                         TVariable<HeapObject>* var_meta_storage,
                         TVariable<IntPtrT>* var_name_index,
                         Label* if_not_found, Label* if_bailout);

  // This is a building block for TryLookupProperty() above. Supports only
  // non-special fast and dictionary objects.
  void TryLookupPropertyInSimpleObject(TNode<JSObject> object, TNode<Map> map,
                                       TNode<Name> unique_name,
                                       Label* if_found_fast,
                                       Label* if_found_dict,
                                       TVariable<HeapObject>* var_meta_storage,
                                       TVariable<IntPtrT>* var_name_index,
                                       Label* if_not_found);

  // This method jumps to if_found if the element is known to exist. To
  // if_absent if it's known to not exist. To if_not_found if the prototype
  // chain needs to be checked. And if_bailout if the lookup is unsupported.
  void TryLookupElement(Node* object, Node* map,
                        SloppyTNode<Int32T> instance_type,
                        SloppyTNode<IntPtrT> intptr_index, Label* if_found,
                        Label* if_absent, Label* if_not_found,
                        Label* if_bailout);

  // This is a type of a lookup in holder generator function. In case of a
  // property lookup the {key} is guaranteed to be an unique name and in case of
  // element lookup the key is an Int32 index.
  using LookupInHolder = std::function<void(
      Node* receiver, Node* holder, Node* map, Node* instance_type, Node* key,
      Label* next_holder, Label* if_bailout)>;

  // For integer indexed exotic cases, check if the given string cannot be a
  // special index. If we are not sure that the given string is not a special
  // index with a simple check, return False. Note that "False" return value
  // does not mean that the name_string is a special index in the current
  // implementation.
  void BranchIfMaybeSpecialIndex(TNode<String> name_string,
                                 Label* if_maybe_special_index,
                                 Label* if_not_special_index);

  // Generic property prototype chain lookup generator.
  // For properties it generates lookup using given {lookup_property_in_holder}
  // and for elements it uses {lookup_element_in_holder}.
  // Upon reaching the end of prototype chain the control goes to {if_end}.
  // If it can't handle the case {receiver}/{key} case then the control goes
  // to {if_bailout}.
  // If {if_proxy} is nullptr, proxies go to if_bailout.
  void TryPrototypeChainLookup(Node* receiver, Node* object, Node* key,
                               const LookupInHolder& lookup_property_in_holder,
                               const LookupInHolder& lookup_element_in_holder,
                               Label* if_end, Label* if_bailout,
                               Label* if_proxy);

  // Instanceof helpers.
  // Returns true if {object} has {prototype} somewhere in it's prototype
  // chain, otherwise false is returned. Might cause arbitrary side effects
  // due to [[GetPrototypeOf]] invocations.
  Node* HasInPrototypeChain(Node* context, Node* object, Node* prototype);
  // ES6 section 7.3.19 OrdinaryHasInstance (C, O)
  Node* OrdinaryHasInstance(Node* context, Node* callable, Node* object);

  // Load type feedback vector from the stub caller's frame.
  TNode<FeedbackVector> LoadFeedbackVectorForStub();

  // Load the value from closure's feedback cell.
  TNode<HeapObject> LoadFeedbackCellValue(SloppyTNode<JSFunction> closure);

  // Load the object from feedback vector cell for the given closure.
  // The returned object could be undefined if the closure does not have
  // a feedback vector associated with it.
  TNode<HeapObject> LoadFeedbackVector(SloppyTNode<JSFunction> closure);

  // Load the ClosureFeedbackCellArray that contains the feedback cells
  // used when creating closures from this function. This array could be
  // directly hanging off the FeedbackCell when there is no feedback vector
  // or available from the feedback vector's header.
  TNode<ClosureFeedbackCellArray> LoadClosureFeedbackArray(
      SloppyTNode<JSFunction> closure);

  // Update the type feedback vector.
  void UpdateFeedback(Node* feedback, Node* feedback_vector, Node* slot_id);

  // Report that there was a feedback update, performing any tasks that should
  // be done after a feedback update.
  void ReportFeedbackUpdate(SloppyTNode<FeedbackVector> feedback_vector,
                            SloppyTNode<IntPtrT> slot_id, const char* reason);

  // Combine the new feedback with the existing_feedback. Do nothing if
  // existing_feedback is nullptr.
  void CombineFeedback(Variable* existing_feedback, int feedback);
  void CombineFeedback(Variable* existing_feedback, Node* feedback);

  // Overwrite the existing feedback with new_feedback. Do nothing if
  // existing_feedback is nullptr.
  void OverwriteFeedback(Variable* existing_feedback, int new_feedback);

  // Check if a property name might require protector invalidation when it is
  // used for a property store or deletion.
  void CheckForAssociatedProtector(Node* name, Label* if_protector);

  TNode<Map> LoadReceiverMap(SloppyTNode<Object> receiver);

  enum class ArgumentsAccessMode { kLoad, kStore, kHas };
  // Emits keyed sloppy arguments has. Returns whether the key is in the
  // arguments.
  Node* HasKeyedSloppyArguments(Node* receiver, Node* key, Label* bailout) {
    return EmitKeyedSloppyArguments(receiver, key, nullptr, bailout,
                                    ArgumentsAccessMode::kHas);
  }

  // Emits keyed sloppy arguments load. Returns either the loaded value.
  Node* LoadKeyedSloppyArguments(Node* receiver, Node* key, Label* bailout) {
    return EmitKeyedSloppyArguments(receiver, key, nullptr, bailout,
                                    ArgumentsAccessMode::kLoad);
  }

  // Emits keyed sloppy arguments store.
  void StoreKeyedSloppyArguments(Node* receiver, Node* key, Node* value,
                                 Label* bailout) {
    DCHECK_NOT_NULL(value);
    EmitKeyedSloppyArguments(receiver, key, value, bailout,
                             ArgumentsAccessMode::kStore);
  }

  // Loads script context from the script context table.
  TNode<Context> LoadScriptContext(TNode<Context> context,
                                   TNode<IntPtrT> context_index);

  Node* Int32ToUint8Clamped(Node* int32_value);
  Node* Float64ToUint8Clamped(Node* float64_value);

  Node* PrepareValueForWriteToTypedArray(TNode<Object> input,
                                         ElementsKind elements_kind,
                                         TNode<Context> context);

  // Store value to an elements array with given elements kind.
  // TODO(turbofan): For BIGINT64_ELEMENTS and BIGUINT64_ELEMENTS
  // we pass {value} as BigInt object instead of int64_t. We should
  // teach TurboFan to handle int64_t on 32-bit platforms eventually.
  void StoreElement(Node* elements, ElementsKind kind, Node* index, Node* value,
                    ParameterMode mode);

  // Implements the BigInt part of
  // https://tc39.github.io/proposal-bigint/#sec-numbertorawbytes,
  // including truncation to 64 bits (i.e. modulo 2^64).
  // {var_high} is only used on 32-bit platforms.
  void BigIntToRawBytes(TNode<BigInt> bigint, TVariable<UintPtrT>* var_low,
                        TVariable<UintPtrT>* var_high);

  void EmitElementStore(Node* object, Node* key, Node* value,
                        ElementsKind elements_kind,
                        KeyedAccessStoreMode store_mode, Label* bailout,
                        Node* context,
                        Variable* maybe_converted_value = nullptr);

  Node* CheckForCapacityGrow(Node* object, Node* elements, ElementsKind kind,
                             Node* length, Node* key, ParameterMode mode,
                             Label* bailout);

  Node* CopyElementsOnWrite(Node* object, Node* elements, ElementsKind kind,
                            Node* length, ParameterMode mode, Label* bailout);

  void TransitionElementsKind(Node* object, Node* map, ElementsKind from_kind,
                              ElementsKind to_kind, Label* bailout);

  void TrapAllocationMemento(Node* object, Label* memento_found);

  TNode<IntPtrT> PageFromAddress(TNode<IntPtrT> address);

  // Store a weak in-place reference into the FeedbackVector.
  TNode<MaybeObject> StoreWeakReferenceInFeedbackVector(
      SloppyTNode<FeedbackVector> feedback_vector, Node* slot,
      SloppyTNode<HeapObject> value, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  // Create a new AllocationSite and install it into a feedback vector.
  TNode<AllocationSite> CreateAllocationSiteInFeedbackVector(
      SloppyTNode<FeedbackVector> feedback_vector, TNode<Smi> slot);

  // TODO(ishell, cbruni): Change to HasBoilerplate.
  TNode<BoolT> NotHasBoilerplate(TNode<Object> maybe_literal_site);
  TNode<Smi> LoadTransitionInfo(TNode<AllocationSite> allocation_site);
  TNode<JSObject> LoadBoilerplate(TNode<AllocationSite> allocation_site);
  TNode<Int32T> LoadElementsKind(TNode<AllocationSite> allocation_site);

  enum class IndexAdvanceMode { kPre, kPost };

  using FastLoopBody = std::function<void(Node* index)>;

  Node* BuildFastLoop(const VariableList& var_list, Node* start_index,
                      Node* end_index, const FastLoopBody& body, int increment,
                      ParameterMode parameter_mode,
                      IndexAdvanceMode advance_mode = IndexAdvanceMode::kPre);

  Node* BuildFastLoop(Node* start_index, Node* end_index,
                      const FastLoopBody& body, int increment,
                      ParameterMode parameter_mode,
                      IndexAdvanceMode advance_mode = IndexAdvanceMode::kPre) {
    return BuildFastLoop(VariableList(0, zone()), start_index, end_index, body,
                         increment, parameter_mode, advance_mode);
  }

  enum class ForEachDirection { kForward, kReverse };

  using FastFixedArrayForEachBody =
      std::function<void(Node* fixed_array, Node* offset)>;

  void BuildFastFixedArrayForEach(
      const CodeStubAssembler::VariableList& vars, Node* fixed_array,
      ElementsKind kind, Node* first_element_inclusive,
      Node* last_element_exclusive, const FastFixedArrayForEachBody& body,
      ParameterMode mode = INTPTR_PARAMETERS,
      ForEachDirection direction = ForEachDirection::kReverse);

  void BuildFastFixedArrayForEach(
      Node* fixed_array, ElementsKind kind, Node* first_element_inclusive,
      Node* last_element_exclusive, const FastFixedArrayForEachBody& body,
      ParameterMode mode = INTPTR_PARAMETERS,
      ForEachDirection direction = ForEachDirection::kReverse) {
    CodeStubAssembler::VariableList list(0, zone());
    BuildFastFixedArrayForEach(list, fixed_array, kind, first_element_inclusive,
                               last_element_exclusive, body, mode, direction);
  }

  TNode<IntPtrT> GetArrayAllocationSize(Node* element_count, ElementsKind kind,
                                        ParameterMode mode, int header_size) {
    return ElementOffsetFromIndex(element_count, kind, mode, header_size);
  }

  TNode<IntPtrT> GetFixedArrayAllocationSize(Node* element_count,
                                             ElementsKind kind,
                                             ParameterMode mode) {
    return GetArrayAllocationSize(element_count, kind, mode,
                                  FixedArray::kHeaderSize);
  }

  TNode<IntPtrT> GetPropertyArrayAllocationSize(Node* element_count,
                                                ParameterMode mode) {
    return GetArrayAllocationSize(element_count, PACKED_ELEMENTS, mode,
                                  PropertyArray::kHeaderSize);
  }

  void GotoIfFixedArraySizeDoesntFitInNewSpace(Node* element_count,
                                               Label* doesnt_fit, int base_size,
                                               ParameterMode mode);

  void InitializeFieldsWithRoot(Node* object, Node* start_offset,
                                Node* end_offset, RootIndex root);

  Node* RelationalComparison(Operation op, Node* left, Node* right,
                             Node* context,
                             Variable* var_type_feedback = nullptr);

  void BranchIfNumberRelationalComparison(Operation op, Node* left, Node* right,
                                          Label* if_true, Label* if_false);

  void BranchIfNumberEqual(TNode<Number> left, TNode<Number> right,
                           Label* if_true, Label* if_false) {
    BranchIfNumberRelationalComparison(Operation::kEqual, left, right, if_true,
                                       if_false);
  }

  void BranchIfNumberNotEqual(TNode<Number> left, TNode<Number> right,
                              Label* if_true, Label* if_false) {
    BranchIfNumberEqual(left, right, if_false, if_true);
  }

  void BranchIfNumberLessThan(TNode<Number> left, TNode<Number> right,
                              Label* if_true, Label* if_false) {
    BranchIfNumberRelationalComparison(Operation::kLessThan, left, right,
                                       if_true, if_false);
  }

  void BranchIfNumberLessThanOrEqual(TNode<Number> left, TNode<Number> right,
                                     Label* if_true, Label* if_false) {
    BranchIfNumberRelationalComparison(Operation::kLessThanOrEqual, left, right,
                                       if_true, if_false);
  }

  void BranchIfNumberGreaterThan(TNode<Number> left, TNode<Number> right,
                                 Label* if_true, Label* if_false) {
    BranchIfNumberRelationalComparison(Operation::kGreaterThan, left, right,
                                       if_true, if_false);
  }

  void BranchIfNumberGreaterThanOrEqual(TNode<Number> left, TNode<Number> right,
                                        Label* if_true, Label* if_false) {
    BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, left,
                                       right, if_true, if_false);
  }

  void BranchIfAccessorPair(Node* value, Label* if_accessor_pair,
                            Label* if_not_accessor_pair) {
    GotoIf(TaggedIsSmi(value), if_not_accessor_pair);
    Branch(IsAccessorPair(value), if_accessor_pair, if_not_accessor_pair);
  }

  void GotoIfNumberGreaterThanOrEqual(Node* left, Node* right, Label* if_false);

  Node* Equal(Node* lhs, Node* rhs, Node* context,
              Variable* var_type_feedback = nullptr);

  TNode<Oddball> StrictEqual(SloppyTNode<Object> lhs, SloppyTNode<Object> rhs,
                             Variable* var_type_feedback = nullptr);

  // ECMA#sec-samevalue
  // Similar to StrictEqual except that NaNs are treated as equal and minus zero
  // differs from positive zero.
  enum class SameValueMode { kNumbersOnly, kFull };
  void BranchIfSameValue(Node* lhs, Node* rhs, Label* if_true, Label* if_false,
                         SameValueMode mode = SameValueMode::kFull);
  // A part of BranchIfSameValue() that handles two double values.
  // Treats NaN == NaN and +0 != -0.
  void BranchIfSameNumberValue(TNode<Float64T> lhs_value,
                               TNode<Float64T> rhs_value, Label* if_true,
                               Label* if_false);

  enum HasPropertyLookupMode { kHasProperty, kForInHasProperty };

  TNode<Oddball> HasProperty(SloppyTNode<Context> context,
                             SloppyTNode<Object> object,
                             SloppyTNode<Object> key,
                             HasPropertyLookupMode mode);

  // Due to naming conflict with the builtin function namespace.
  TNode<Oddball> HasProperty_Inline(TNode<Context> context,
                                    TNode<JSReceiver> object,
                                    TNode<Object> key) {
    return HasProperty(context, object, key,
                       HasPropertyLookupMode::kHasProperty);
  }

  Node* Typeof(Node* value);

  TNode<Object> GetSuperConstructor(SloppyTNode<Context> context,
                                    SloppyTNode<JSFunction> active_function);

  TNode<JSReceiver> SpeciesConstructor(
      SloppyTNode<Context> context, SloppyTNode<Object> object,
      SloppyTNode<JSReceiver> default_constructor);

  Node* InstanceOf(Node* object, Node* callable, Node* context);

  // Debug helpers
  Node* IsDebugActive();

  // JSArrayBuffer helpers
  TNode<Uint32T> LoadJSArrayBufferBitField(TNode<JSArrayBuffer> array_buffer);
  TNode<RawPtrT> LoadJSArrayBufferBackingStore(
      TNode<JSArrayBuffer> array_buffer);
  TNode<BoolT> IsDetachedBuffer(TNode<JSArrayBuffer> buffer);
  void ThrowIfArrayBufferIsDetached(SloppyTNode<Context> context,
                                    TNode<JSArrayBuffer> array_buffer,
                                    const char* method_name);

  // JSArrayBufferView helpers
  TNode<JSArrayBuffer> LoadJSArrayBufferViewBuffer(
      TNode<JSArrayBufferView> array_buffer_view);
  TNode<UintPtrT> LoadJSArrayBufferViewByteLength(
      TNode<JSArrayBufferView> array_buffer_view);
  TNode<UintPtrT> LoadJSArrayBufferViewByteOffset(
      TNode<JSArrayBufferView> array_buffer_view);
  void ThrowIfArrayBufferViewBufferIsDetached(
      SloppyTNode<Context> context, TNode<JSArrayBufferView> array_buffer_view,
      const char* method_name);

  // JSTypedArray helpers
  TNode<UintPtrT> LoadJSTypedArrayLength(TNode<JSTypedArray> typed_array);
  TNode<RawPtrT> LoadJSTypedArrayBackingStore(TNode<JSTypedArray> typed_array);

  TNode<IntPtrT> ElementOffsetFromIndex(Node* index, ElementsKind kind,
                                        ParameterMode mode, int base_size = 0);

  // Check that a field offset is within the bounds of the an object.
  TNode<BoolT> IsOffsetInBounds(SloppyTNode<IntPtrT> offset,
                                SloppyTNode<IntPtrT> length, int header_size,
                                ElementsKind kind = HOLEY_ELEMENTS);

  // Load a builtin's code from the builtin array in the isolate.
  TNode<Code> LoadBuiltin(TNode<Smi> builtin_id);

  // Figure out the SFI's code object using its data field.
  // If |if_compile_lazy| is provided then the execution will go to the given
  // label in case of an CompileLazy code object.
  TNode<Code> GetSharedFunctionInfoCode(
      SloppyTNode<SharedFunctionInfo> shared_info,
      Label* if_compile_lazy = nullptr);

  Node* AllocateFunctionWithMapAndContext(Node* map, Node* shared_info,
                                          Node* context);

  // Promise helpers
  Node* IsPromiseHookEnabled();
  Node* HasAsyncEventDelegate();
  Node* IsPromiseHookEnabledOrHasAsyncEventDelegate();
  Node* IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate();

  // for..in helpers
  void CheckPrototypeEnumCache(Node* receiver, Node* receiver_map,
                               Label* if_fast, Label* if_slow);
  Node* CheckEnumCache(Node* receiver, Label* if_empty, Label* if_runtime);

  TNode<Object> GetArgumentValue(TorqueStructArguments args,
                                 TNode<IntPtrT> index);

  TorqueStructArguments GetFrameArguments(TNode<RawPtrT> frame,
                                          TNode<IntPtrT> argc);

  // Support for printf-style debugging
  void Print(const char* s);
  void Print(const char* prefix, Node* tagged_value);
  inline void Print(SloppyTNode<Object> tagged_value) {
    return Print(nullptr, tagged_value);
  }
  inline void Print(TNode<MaybeObject> tagged_value) {
    return Print(nullptr, tagged_value);
  }

  template <class... TArgs>
  Node* MakeTypeError(MessageTemplate message, Node* context, TArgs... args) {
    STATIC_ASSERT(sizeof...(TArgs) <= 3);
    Node* const make_type_error = LoadContextElement(
        LoadNativeContext(context), Context::MAKE_TYPE_ERROR_INDEX);
    return CallJS(CodeFactory::Call(isolate()), context, make_type_error,
                  UndefinedConstant(), SmiConstant(message), args...);
  }

  void Abort(AbortReason reason) {
    CallRuntime(Runtime::kAbort, NoContextConstant(), SmiConstant(reason));
    Unreachable();
  }

  bool ConstexprBoolNot(bool value) { return !value; }

  bool ConstexprInt31Equal(int31_t a, int31_t b) { return a == b; }
  bool ConstexprInt31GreaterThanEqual(int31_t a, int31_t b) { return a >= b; }
  uint32_t ConstexprUint32Add(uint32_t a, uint32_t b) { return a + b; }
  int31_t ConstexprInt31Add(int31_t a, int31_t b) {
    int32_t val;
    CHECK(!base::bits::SignedAddOverflow32(a, b, &val));
    return val;
  }
  int31_t ConstexprInt31Mul(int31_t a, int31_t b) {
    int32_t val;
    CHECK(!base::bits::SignedMulOverflow32(a, b, &val));
    return val;
  }

  void PerformStackCheck(TNode<Context> context);

  void SetPropertyLength(TNode<Context> context, TNode<Object> array,
                         TNode<Number> length);

  // Checks that {object_map}'s prototype map is the {initial_prototype_map} and
  // makes sure that the field with name at index {descriptor} is still
  // constant. If it is not, go to label {if_modified}.
  //
  // To make the checks robust, the method also asserts that the descriptor has
  // the right key, the caller must pass the root index of the key
  // in {field_name_root_index}.
  //
  // This is useful for checking that given function has not been patched
  // on the prototype.
  void GotoIfInitialPrototypePropertyModified(TNode<Map> object_map,
                                              TNode<Map> initial_prototype_map,
                                              int descfriptor,
                                              RootIndex field_name_root_index,
                                              Label* if_modified);
  struct DescriptorIndexAndName {
    DescriptorIndexAndName() {}
    DescriptorIndexAndName(int descriptor_index, RootIndex name_root_index)
        : descriptor_index(descriptor_index),
          name_root_index(name_root_index) {}

    int descriptor_index;
    RootIndex name_root_index;
  };
  void GotoIfInitialPrototypePropertiesModified(
      TNode<Map> object_map, TNode<Map> initial_prototype_map,
      Vector<DescriptorIndexAndName> properties, Label* if_modified);

  // Implements DescriptorArray::Search().
  void DescriptorLookup(SloppyTNode<Name> unique_name,
                        SloppyTNode<DescriptorArray> descriptors,
                        SloppyTNode<Uint32T> bitfield3, Label* if_found,
                        TVariable<IntPtrT>* var_name_index,
                        Label* if_not_found);

  // Implements TransitionArray::SearchName() - searches for first transition
  // entry with given name (note that there could be multiple entries with
  // the same name).
  void TransitionLookup(SloppyTNode<Name> unique_name,
                        SloppyTNode<TransitionArray> transitions,
                        Label* if_found, TVariable<IntPtrT>* var_name_index,
                        Label* if_not_found);

  // Implements generic search procedure like i::Search<Array>().
  template <typename Array>
  void Lookup(TNode<Name> unique_name, TNode<Array> array,
              TNode<Uint32T> number_of_valid_entries, Label* if_found,
              TVariable<IntPtrT>* var_name_index, Label* if_not_found);

  // Implements generic linear search procedure like i::LinearSearch<Array>().
  template <typename Array>
  void LookupLinear(TNode<Name> unique_name, TNode<Array> array,
                    TNode<Uint32T> number_of_valid_entries, Label* if_found,
                    TVariable<IntPtrT>* var_name_index, Label* if_not_found);

  // Implements generic binary search procedure like i::BinarySearch<Array>().
  template <typename Array>
  void LookupBinary(TNode<Name> unique_name, TNode<Array> array,
                    TNode<Uint32T> number_of_valid_entries, Label* if_found,
                    TVariable<IntPtrT>* var_name_index, Label* if_not_found);

  // Converts [Descriptor/Transition]Array entry number to a fixed array index.
  template <typename Array>
  TNode<IntPtrT> EntryIndexToIndex(TNode<Uint32T> entry_index);

  // Implements [Descriptor/Transition]Array::ToKeyIndex.
  template <typename Array>
  TNode<IntPtrT> ToKeyIndex(TNode<Uint32T> entry_index);

  // Implements [Descriptor/Transition]Array::GetKey.
  template <typename Array>
  TNode<Name> GetKey(TNode<Array> array, TNode<Uint32T> entry_index);

  // Implements DescriptorArray::GetDetails.
  TNode<Uint32T> DescriptorArrayGetDetails(TNode<DescriptorArray> descriptors,
                                           TNode<Uint32T> descriptor_number);

  using ForEachDescriptorBodyFunction =
      std::function<void(TNode<IntPtrT> descriptor_key_index)>;

  // Descriptor array accessors based on key_index, which is equal to
  // DescriptorArray::ToKeyIndex(descriptor).
  TNode<Name> LoadKeyByKeyIndex(TNode<DescriptorArray> container,
                                TNode<IntPtrT> key_index);
  TNode<Uint32T> LoadDetailsByKeyIndex(TNode<DescriptorArray> container,
                                       TNode<IntPtrT> key_index);
  TNode<Object> LoadValueByKeyIndex(TNode<DescriptorArray> container,
                                    TNode<IntPtrT> key_index);
  TNode<MaybeObject> LoadFieldTypeByKeyIndex(TNode<DescriptorArray> container,
                                             TNode<IntPtrT> key_index);

  TNode<IntPtrT> DescriptorEntryToIndex(TNode<IntPtrT> descriptor);

  // Descriptor array accessors based on descriptor.
  TNode<Name> LoadKeyByDescriptorEntry(TNode<DescriptorArray> descriptors,
                                       TNode<IntPtrT> descriptor);
  TNode<Name> LoadKeyByDescriptorEntry(TNode<DescriptorArray> descriptors,
                                       int descriptor);
  TNode<Uint32T> LoadDetailsByDescriptorEntry(
      TNode<DescriptorArray> descriptors, TNode<IntPtrT> descriptor);
  TNode<Uint32T> LoadDetailsByDescriptorEntry(
      TNode<DescriptorArray> descriptors, int descriptor);
  TNode<Object> LoadValueByDescriptorEntry(TNode<DescriptorArray> descriptors,
                                           int descriptor);
  TNode<MaybeObject> LoadFieldTypeByDescriptorEntry(
      TNode<DescriptorArray> descriptors, TNode<IntPtrT> descriptor);

  using ForEachKeyValueFunction =
      std::function<void(TNode<Name> key, TNode<Object> value)>;

  enum ForEachEnumerationMode {
    // String and then Symbol properties according to the spec
    // ES#sec-object.assign
    kEnumerationOrder,
    // Order of property addition
    kPropertyAdditionOrder,
  };

  // For each JSObject property (in DescriptorArray order), check if the key is
  // enumerable, and if so, load the value from the receiver and evaluate the
  // closure.
  void ForEachEnumerableOwnProperty(TNode<Context> context, TNode<Map> map,
                                    TNode<JSObject> object,
                                    ForEachEnumerationMode mode,
                                    const ForEachKeyValueFunction& body,
                                    Label* bailout);

  TNode<Object> CallGetterIfAccessor(Node* value, Node* details, Node* context,
                                     Node* receiver, Label* if_bailout,
                                     GetOwnPropertyMode mode = kCallJSGetter);

  TNode<IntPtrT> TryToIntptr(Node* key, Label* miss);

  void BranchIfPrototypesHaveNoElements(Node* receiver_map,
                                        Label* definitely_no_elements,
                                        Label* possibly_elements);

  void InitializeFunctionContext(Node* native_context, Node* context,
                                 int slots);

  TNode<JSArray> ArrayCreate(TNode<Context> context, TNode<Number> length);

  // Allocate a clone of a mutable primitive, if {object} is a
  // MutableHeapNumber.
  TNode<Object> CloneIfMutablePrimitive(TNode<Object> object);

 private:
  friend class CodeStubArguments;

  void HandleBreakOnNode();

  TNode<HeapObject> AllocateRawDoubleAligned(TNode<IntPtrT> size_in_bytes,
                                             AllocationFlags flags,
                                             TNode<RawPtrT> top_address,
                                             TNode<RawPtrT> limit_address);
  TNode<HeapObject> AllocateRawUnaligned(TNode<IntPtrT> size_in_bytes,
                                         AllocationFlags flags,
                                         TNode<RawPtrT> top_address,
                                         TNode<RawPtrT> limit_address);
  TNode<HeapObject> AllocateRaw(TNode<IntPtrT> size_in_bytes,
                                AllocationFlags flags,
                                TNode<RawPtrT> top_address,
                                TNode<RawPtrT> limit_address);

  // Allocate and return a JSArray of given total size in bytes with header
  // fields initialized.
  TNode<JSArray> AllocateUninitializedJSArray(TNode<Map> array_map,
                                              TNode<Smi> length,
                                              Node* allocation_site,
                                              TNode<IntPtrT> size_in_bytes);

  TNode<BoolT> IsValidSmi(TNode<Smi> smi);
  Node* SmiShiftBitsConstant();

  // Emits keyed sloppy arguments load if the |value| is nullptr or store
  // otherwise. Returns either the loaded value or |value|.
  Node* EmitKeyedSloppyArguments(Node* receiver, Node* key, Node* value,
                                 Label* bailout,
                                 ArgumentsAccessMode access_mode);

  TNode<String> AllocateSlicedString(RootIndex map_root_index,
                                     TNode<Uint32T> length,
                                     TNode<String> parent, TNode<Smi> offset);

  // Allocate a MutableHeapNumber without initializing its value.
  TNode<MutableHeapNumber> AllocateMutableHeapNumber();

  Node* SelectImpl(TNode<BoolT> condition, const NodeGenerator& true_body,
                   const NodeGenerator& false_body, MachineRepresentation rep);

  // Implements [Descriptor/Transition]Array::number_of_entries.
  template <typename Array>
  TNode<Uint32T> NumberOfEntries(TNode<Array> array);

  // Implements [Descriptor/Transition]Array::GetSortedKeyIndex.
  template <typename Array>
  TNode<Uint32T> GetSortedKeyIndex(TNode<Array> descriptors,
                                   TNode<Uint32T> entry_index);

  TNode<Smi> CollectFeedbackForString(SloppyTNode<Int32T> instance_type);
  void GenerateEqual_Same(Node* value, Label* if_equal, Label* if_notequal,
                          Variable* var_type_feedback = nullptr);
  TNode<String> AllocAndCopyStringCharacters(Node* from,
                                             Node* from_instance_type,
                                             TNode<IntPtrT> from_index,
                                             TNode<IntPtrT> character_count);

  static const int kElementLoopUnrollThreshold = 8;

  // {convert_bigint} is only meaningful when {mode} == kToNumber.
  Node* NonNumberToNumberOrNumeric(
      Node* context, Node* input, Object::Conversion mode,
      BigIntHandling bigint_handling = BigIntHandling::kThrow);

  void TaggedToNumeric(Node* context, Node* value, Label* done,
                       Variable* var_numeric, Variable* var_feedback);

  template <Object::Conversion conversion>
  void TaggedToWord32OrBigIntImpl(Node* context, Node* value, Label* if_number,
                                  Variable* var_word32,
                                  Label* if_bigint = nullptr,
                                  Variable* var_bigint = nullptr,
                                  Variable* var_feedback = nullptr);

 private:
  // Low-level accessors for Descriptor arrays.
  template <typename T>
  TNode<T> LoadDescriptorArrayElement(TNode<DescriptorArray> object,
                                      TNode<IntPtrT> index,
                                      int additional_offset);
};

class V8_EXPORT_PRIVATE CodeStubArguments {
 public:
  using Node = compiler::Node;
  template <class T>
  using TNode = compiler::TNode<T>;
  template <class T>
  using SloppyTNode = compiler::SloppyTNode<T>;
  enum ReceiverMode { kHasReceiver, kNoReceiver };

  // |argc| is an intptr value which specifies the number of arguments passed
  // to the builtin excluding the receiver. The arguments will include a
  // receiver iff |receiver_mode| is kHasReceiver.
  CodeStubArguments(CodeStubAssembler* assembler, Node* argc,
                    ReceiverMode receiver_mode = ReceiverMode::kHasReceiver)
      : CodeStubArguments(assembler, argc, nullptr,
                          CodeStubAssembler::INTPTR_PARAMETERS, receiver_mode) {
  }

  // |argc| is either a smi or intptr depending on |param_mode|. The arguments
  // include a receiver iff |receiver_mode| is kHasReceiver.
  CodeStubArguments(CodeStubAssembler* assembler, Node* argc, Node* fp,
                    CodeStubAssembler::ParameterMode param_mode,
                    ReceiverMode receiver_mode = ReceiverMode::kHasReceiver);

  // Used by Torque to construct arguments based on a Torque-defined
  // struct of values.
  CodeStubArguments(CodeStubAssembler* assembler,
                    TorqueStructArguments torque_arguments)
      : assembler_(assembler),
        argc_mode_(CodeStubAssembler::INTPTR_PARAMETERS),
        receiver_mode_(ReceiverMode::kHasReceiver),
        argc_(torque_arguments.length),
        base_(torque_arguments.base),
        fp_(torque_arguments.frame) {}

  TNode<Object> GetReceiver() const;
  // Replaces receiver argument on the expression stack. Should be used only
  // for manipulating arguments in trampoline builtins before tail calling
  // further with passing all the JS arguments as is.
  void SetReceiver(TNode<Object> object) const;

  // Computes address of the index'th argument.
  TNode<WordT> AtIndexPtr(Node* index,
                          CodeStubAssembler::ParameterMode mode =
                              CodeStubAssembler::INTPTR_PARAMETERS) const;

  // |index| is zero-based and does not include the receiver
  TNode<Object> AtIndex(Node* index,
                        CodeStubAssembler::ParameterMode mode =
                            CodeStubAssembler::INTPTR_PARAMETERS) const;

  TNode<Object> AtIndex(int index) const;

  TNode<Object> GetOptionalArgumentValue(int index) {
    return GetOptionalArgumentValue(index, assembler_->UndefinedConstant());
  }
  TNode<Object> GetOptionalArgumentValue(int index,
                                         TNode<Object> default_value);

  Node* GetLength(CodeStubAssembler::ParameterMode mode) const {
    DCHECK_EQ(mode, argc_mode_);
    return argc_;
  }

  TorqueStructArguments GetTorqueArguments() const {
    DCHECK_EQ(argc_mode_, CodeStubAssembler::INTPTR_PARAMETERS);
    return TorqueStructArguments{assembler_->UncheckedCast<RawPtrT>(fp_), base_,
                                 assembler_->UncheckedCast<IntPtrT>(argc_)};
  }

  TNode<Object> GetOptionalArgumentValue(TNode<IntPtrT> index) {
    return GetOptionalArgumentValue(index, assembler_->UndefinedConstant());
  }
  TNode<Object> GetOptionalArgumentValue(TNode<IntPtrT> index,
                                         TNode<Object> default_value);
  TNode<IntPtrT> GetLength() const {
    DCHECK_EQ(argc_mode_, CodeStubAssembler::INTPTR_PARAMETERS);
    return assembler_->UncheckedCast<IntPtrT>(argc_);
  }

  using ForEachBodyFunction = std::function<void(Node* arg)>;

  // Iteration doesn't include the receiver. |first| and |last| are zero-based.
  void ForEach(const ForEachBodyFunction& body, Node* first = nullptr,
               Node* last = nullptr,
               CodeStubAssembler::ParameterMode mode =
                   CodeStubAssembler::INTPTR_PARAMETERS) {
    CodeStubAssembler::VariableList list(0, assembler_->zone());
    ForEach(list, body, first, last);
  }

  // Iteration doesn't include the receiver. |first| and |last| are zero-based.
  void ForEach(const CodeStubAssembler::VariableList& vars,
               const ForEachBodyFunction& body, Node* first = nullptr,
               Node* last = nullptr,
               CodeStubAssembler::ParameterMode mode =
                   CodeStubAssembler::INTPTR_PARAMETERS);

  void PopAndReturn(Node* value);

 private:
  Node* GetArguments();

  CodeStubAssembler* assembler_;
  CodeStubAssembler::ParameterMode argc_mode_;
  ReceiverMode receiver_mode_;
  Node* argc_;
  TNode<RawPtrT> base_;
  Node* fp_;
};

class ToDirectStringAssembler : public CodeStubAssembler {
 private:
  enum StringPointerKind { PTR_TO_DATA, PTR_TO_STRING };

 public:
  enum Flag {
    kDontUnpackSlicedStrings = 1 << 0,
  };
  using Flags = base::Flags<Flag>;

  ToDirectStringAssembler(compiler::CodeAssemblerState* state, Node* string,
                          Flags flags = Flags());

  // Converts flat cons, thin, and sliced strings and returns the direct
  // string. The result can be either a sequential or external string.
  // Jumps to if_bailout if the string if the string is indirect and cannot
  // be unpacked.
  TNode<String> TryToDirect(Label* if_bailout);

  // Returns a pointer to the beginning of the string data.
  // Jumps to if_bailout if the external string cannot be unpacked.
  TNode<RawPtrT> PointerToData(Label* if_bailout) {
    return TryToSequential(PTR_TO_DATA, if_bailout);
  }

  // Returns a pointer that, offset-wise, looks like a String.
  // Jumps to if_bailout if the external string cannot be unpacked.
  TNode<RawPtrT> PointerToString(Label* if_bailout) {
    return TryToSequential(PTR_TO_STRING, if_bailout);
  }

  Node* string() { return var_string_.value(); }
  Node* instance_type() { return var_instance_type_.value(); }
  TNode<IntPtrT> offset() {
    return UncheckedCast<IntPtrT>(var_offset_.value());
  }
  Node* is_external() { return var_is_external_.value(); }

 private:
  TNode<RawPtrT> TryToSequential(StringPointerKind ptr_kind, Label* if_bailout);

  Variable var_string_;
  Variable var_instance_type_;
  Variable var_offset_;
  Variable var_is_external_;

  const Flags flags_;
};

DEFINE_OPERATORS_FOR_FLAGS(CodeStubAssembler::AllocationFlags)

}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_CODE_STUB_ASSEMBLER_H_
