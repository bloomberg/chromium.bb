/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * DO NOT EDIT: GENERATED CODE
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_H_

#include "native_client/src/trusted/validator_arm/decode.h"

namespace nacl_arm_dec {

struct NamedCoprocessorOp : public CoprocessorOp {
  virtual ~NamedCoprocessorOp() {}
  virtual const char* name() const;
};

struct NamedImmediateBic : public ImmediateBic {
  virtual ~NamedImmediateBic() {}
  virtual const char* name() const;
};

struct NamedLoadMultiple : public LoadMultiple {
  virtual ~NamedLoadMultiple() {}
  virtual const char* name() const;
};

struct NamedLoadCoprocessor : public LoadCoprocessor {
  virtual ~NamedLoadCoprocessor() {}
  virtual const char* name() const;
};

struct NamedLoadDoubleExclusive : public LoadDoubleExclusive {
  virtual ~NamedLoadDoubleExclusive() {}
  virtual const char* name() const;
};

struct NamedBranch : public Branch {
  virtual ~NamedBranch() {}
  virtual const char* name() const;
};

struct NamedTest : public Test {
  virtual ~NamedTest() {}
  virtual const char* name() const;
};

struct NamedStoreRegister : public StoreRegister {
  virtual ~NamedStoreRegister() {}
  virtual const char* name() const;
};

struct NamedMoveDoubleFromCoprocessor : public MoveDoubleFromCoprocessor {
  virtual ~NamedMoveDoubleFromCoprocessor() {}
  virtual const char* name() const;
};

struct NamedTestImmediate : public TestImmediate {
  virtual ~NamedTestImmediate() {}
  virtual const char* name() const;
};

struct NamedBxBlx : public BxBlx {
  virtual ~NamedBxBlx() {}
  virtual const char* name() const;
};

struct NamedEffectiveNoOp : public EffectiveNoOp {
  virtual ~NamedEffectiveNoOp() {}
  virtual const char* name() const;
};

struct NamedLongMultiply : public LongMultiply {
  virtual ~NamedLongMultiply() {}
  virtual const char* name() const;
};

struct NamedBinary4RegisterShiftedOp : public Binary4RegisterShiftedOp {
  virtual ~NamedBinary4RegisterShiftedOp() {}
  virtual const char* name() const;
};

struct NamedBreakpoint : public Breakpoint {
  virtual ~NamedBreakpoint() {}
  virtual const char* name() const;
};

struct NamedMultiply : public Multiply {
  virtual ~NamedMultiply() {}
  virtual const char* name() const;
};

struct NamedPackSatRev : public PackSatRev {
  virtual ~NamedPackSatRev() {}
  virtual const char* name() const;
};

struct NamedLoadExclusive : public LoadExclusive {
  virtual ~NamedLoadExclusive() {}
  virtual const char* name() const;
};

struct NamedVectorStore : public VectorStore {
  virtual ~NamedVectorStore() {}
  virtual const char* name() const;
};

struct NamedUnary3RegisterShiftedOp : public Unary3RegisterShiftedOp {
  virtual ~NamedUnary3RegisterShiftedOp() {}
  virtual const char* name() const;
};

struct NamedUndefined : public Undefined {
  virtual ~NamedUndefined() {}
  virtual const char* name() const;
};

struct NamedDataProc : public DataProc {
  virtual ~NamedDataProc() {}
  virtual const char* name() const;
};

struct NamedDeprecated : public Deprecated {
  virtual ~NamedDeprecated() {}
  virtual const char* name() const;
};

struct NamedLoadImmediate : public LoadImmediate {
  virtual ~NamedLoadImmediate() {}
  virtual const char* name() const;
};

struct NamedStoreCoprocessor : public StoreCoprocessor {
  virtual ~NamedStoreCoprocessor() {}
  virtual const char* name() const;
};

struct NamedRoadblock : public Roadblock {
  virtual ~NamedRoadblock() {}
  virtual const char* name() const;
};

struct NamedLoadDoubleR : public LoadDoubleR {
  virtual ~NamedLoadDoubleR() {}
  virtual const char* name() const;
};

struct NamedStoreExclusive : public StoreExclusive {
  virtual ~NamedStoreExclusive() {}
  virtual const char* name() const;
};

struct NamedStoreImmediate : public StoreImmediate {
  virtual ~NamedStoreImmediate() {}
  virtual const char* name() const;
};

struct NamedMoveFromCoprocessor : public MoveFromCoprocessor {
  virtual ~NamedMoveFromCoprocessor() {}
  virtual const char* name() const;
};

struct NamedLoadRegister : public LoadRegister {
  virtual ~NamedLoadRegister() {}
  virtual const char* name() const;
};

struct NamedLoadDoubleI : public LoadDoubleI {
  virtual ~NamedLoadDoubleI() {}
  virtual const char* name() const;
};

struct NamedBinary3RegisterShiftedTest : public Binary3RegisterShiftedTest {
  virtual ~NamedBinary3RegisterShiftedTest() {}
  virtual const char* name() const;
};

struct NamedUnpredictable : public Unpredictable {
  virtual ~NamedUnpredictable() {}
  virtual const char* name() const;
};

struct NamedForbidden : public Forbidden {
  virtual ~NamedForbidden() {}
  virtual const char* name() const;
};

struct NamedVectorLoad : public VectorLoad {
  virtual ~NamedVectorLoad() {}
  virtual const char* name() const;
};

struct NamedMoveToStatusRegister : public MoveToStatusRegister {
  virtual ~NamedMoveToStatusRegister() {}
  virtual const char* name() const;
};

struct NamedSatAddSub : public SatAddSub {
  virtual ~NamedSatAddSub() {}
  virtual const char* name() const;
};

/*
 * Defines a stateless decoder class selector for instructions
 */
/*
 * Define the class decoders used by this decoder state.
 */
class NamedArm32DecoderState : DecoderState {
 public:
  // Generates an instance of a decoder state.
  explicit NamedArm32DecoderState();
  virtual ~NamedArm32DecoderState();
  
  // Parses the given instruction, returning the decoder to use.
  virtual const class ClassDecoder &decode(const Instruction) const;
  
  // Define the decoders to use in this decoder state
  NamedCoprocessorOp CoprocessorOp_instance_;
  NamedImmediateBic ImmediateBic_instance_;
  NamedLoadMultiple LoadMultiple_instance_;
  NamedLoadCoprocessor LoadCoprocessor_instance_;
  NamedLoadDoubleExclusive LoadDoubleExclusive_instance_;
  NamedBranch Branch_instance_;
  NamedTest Test_instance_;
  NamedStoreRegister StoreRegister_instance_;
  NamedMoveDoubleFromCoprocessor MoveDoubleFromCoprocessor_instance_;
  NamedTestImmediate TestImmediate_instance_;
  NamedBxBlx BxBlx_instance_;
  NamedEffectiveNoOp EffectiveNoOp_instance_;
  NamedLongMultiply LongMultiply_instance_;
  NamedBinary4RegisterShiftedOp Binary4RegisterShiftedOp_instance_;
  NamedBreakpoint Breakpoint_instance_;
  NamedMultiply Multiply_instance_;
  NamedPackSatRev PackSatRev_instance_;
  NamedLoadExclusive LoadExclusive_instance_;
  NamedVectorStore VectorStore_instance_;
  NamedUnary3RegisterShiftedOp Unary3RegisterShiftedOp_instance_;
  NamedUndefined Undefined_instance_;
  NamedDataProc DataProc_instance_;
  NamedDeprecated Deprecated_instance_;
  NamedLoadImmediate LoadImmediate_instance_;
  NamedStoreCoprocessor StoreCoprocessor_instance_;
  NamedRoadblock Roadblock_instance_;
  NamedLoadDoubleR LoadDoubleR_instance_;
  NamedStoreExclusive StoreExclusive_instance_;
  NamedStoreImmediate StoreImmediate_instance_;
  NamedMoveFromCoprocessor MoveFromCoprocessor_instance_;
  NamedLoadRegister LoadRegister_instance_;
  NamedLoadDoubleI LoadDoubleI_instance_;
  NamedBinary3RegisterShiftedTest Binary3RegisterShiftedTest_instance_;
  NamedUnpredictable Unpredictable_instance_;
  NamedForbidden Forbidden_instance_;
  NamedVectorLoad VectorLoad_instance_;
  NamedMoveToStatusRegister MoveToStatusRegister_instance_;
  NamedSatAddSub SatAddSub_instance_;
  
 private:
  // Don't allow the following!
  explicit NamedArm32DecoderState(const NamedArm32DecoderState&);
  void operator=(const NamedArm32DecoderState&);
};

}  // namespace
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_H_
