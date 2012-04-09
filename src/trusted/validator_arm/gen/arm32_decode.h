/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * DO NOT EDIT: GENERATED CODE
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_

#include "native_client/src/trusted/validator_arm/decode.h"

namespace nacl_arm_dec {

/*
 * Defines a stateless decoder class selector for instructions
 */
/*
 * Define the class decoders used by this decoder state.
 */
class Arm32DecoderState : DecoderState {
 public:
  // Generates an instance of a decoder state.
  explicit Arm32DecoderState();
  virtual ~Arm32DecoderState();
  
  // Parses the given instruction, returning the decoder to use.
  virtual const class ClassDecoder &decode(const Instruction) const;
  
  // Define the decoders to use in this decoder state
  CoprocessorOp CoprocessorOp_instance_;
  ImmediateBic ImmediateBic_instance_;
  LoadMultiple LoadMultiple_instance_;
  LoadCoprocessor LoadCoprocessor_instance_;
  LoadDoubleExclusive LoadDoubleExclusive_instance_;
  Branch Branch_instance_;
  Test Test_instance_;
  StoreRegister StoreRegister_instance_;
  MoveDoubleFromCoprocessor MoveDoubleFromCoprocessor_instance_;
  TestImmediate TestImmediate_instance_;
  BxBlx BxBlx_instance_;
  EffectiveNoOp EffectiveNoOp_instance_;
  LongMultiply LongMultiply_instance_;
  Binary4RegisterShiftedOp Binary4RegisterShiftedOp_instance_;
  Breakpoint Breakpoint_instance_;
  Multiply Multiply_instance_;
  PackSatRev PackSatRev_instance_;
  LoadExclusive LoadExclusive_instance_;
  VectorStore VectorStore_instance_;
  Unary3RegisterShiftedOp Unary3RegisterShiftedOp_instance_;
  Undefined Undefined_instance_;
  DataProc DataProc_instance_;
  Deprecated Deprecated_instance_;
  LoadImmediate LoadImmediate_instance_;
  StoreCoprocessor StoreCoprocessor_instance_;
  Roadblock Roadblock_instance_;
  LoadDoubleR LoadDoubleR_instance_;
  StoreExclusive StoreExclusive_instance_;
  StoreImmediate StoreImmediate_instance_;
  MoveFromCoprocessor MoveFromCoprocessor_instance_;
  LoadRegister LoadRegister_instance_;
  LoadDoubleI LoadDoubleI_instance_;
  Binary3RegisterShiftedTest Binary3RegisterShiftedTest_instance_;
  Unpredictable Unpredictable_instance_;
  Forbidden Forbidden_instance_;
  VectorLoad VectorLoad_instance_;
  MoveToStatusRegister MoveToStatusRegister_instance_;
  SatAddSub SatAddSub_instance_;
  
 private:
  // Don't allow the following!
  explicit Arm32DecoderState(const Arm32DecoderState&);
  void operator=(const Arm32DecoderState&);
};

}  // namespace
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_H_
