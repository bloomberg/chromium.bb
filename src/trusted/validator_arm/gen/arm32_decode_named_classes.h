

/*
 * Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// DO NOT EDIT: GENERATED CODE


#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif



#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_

#include "native_client/src/trusted/validator_arm/named_class_decoder.h"


/*
 * Define rule decoder classes.
 */
namespace nacl_arm_dec {

class Add_Rule_7_A1_P26Binary4RegisterShiftedOp
   : public Binary4RegisterShiftedOp {
 public:
  virtual ~Add_Rule_7_A1_P26Binary4RegisterShiftedOp() {}
};


class Rsb_Rule_144_A1_P288Binary4RegisterShiftedOp
   : public Binary4RegisterShiftedOp {
 public:
  virtual ~Rsb_Rule_144_A1_P288Binary4RegisterShiftedOp() {}
};


}  // nacl_arm_dec

namespace nacl_arm_test {

/*
 * Define named class decoders for each class decoder.
 * The main purpose of these classes is to introduce
 * instances that are named specifically to the class decoder
 * and/or rule that was used to parse them. This makes testing
 * much easier in that error messages use these named classes
 * to clarify what row in the corresponding table was used
 * to select this decoder. Without these names, debugging the
 * output of the test code would be nearly impossible
 */


class NamedBinary3RegisterShiftedTest : public NamedClassDecoder {
 public:
  inline NamedBinary3RegisterShiftedTest()
    : NamedClassDecoder(decoder_, "Binary3RegisterShiftedTest")
  {}
  virtual ~NamedBinary3RegisterShiftedTest() {}
 protected:
  explicit inline NamedBinary3RegisterShiftedTest(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Binary3RegisterShiftedTest decoder_;
};


class NamedBinary4RegisterShiftedOp : public NamedClassDecoder {
 public:
  inline NamedBinary4RegisterShiftedOp()
    : NamedClassDecoder(decoder_, "Binary4RegisterShiftedOp")
  {}
  virtual ~NamedBinary4RegisterShiftedOp() {}
 protected:
  explicit inline NamedBinary4RegisterShiftedOp(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Binary4RegisterShiftedOp decoder_;
};


class NamedBranch : public NamedClassDecoder {
 public:
  inline NamedBranch()
    : NamedClassDecoder(decoder_, "Branch")
  {}
  virtual ~NamedBranch() {}
 protected:
  explicit inline NamedBranch(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Branch decoder_;
};


class NamedBreakpoint : public NamedClassDecoder {
 public:
  inline NamedBreakpoint()
    : NamedClassDecoder(decoder_, "Breakpoint")
  {}
  virtual ~NamedBreakpoint() {}
 protected:
  explicit inline NamedBreakpoint(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Breakpoint decoder_;
};


class NamedBxBlx : public NamedClassDecoder {
 public:
  inline NamedBxBlx()
    : NamedClassDecoder(decoder_, "BxBlx")
  {}
  virtual ~NamedBxBlx() {}
 protected:
  explicit inline NamedBxBlx(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::BxBlx decoder_;
};


class NamedCoprocessorOp : public NamedClassDecoder {
 public:
  inline NamedCoprocessorOp()
    : NamedClassDecoder(decoder_, "CoprocessorOp")
  {}
  virtual ~NamedCoprocessorOp() {}
 protected:
  explicit inline NamedCoprocessorOp(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::CoprocessorOp decoder_;
};


class NamedDataProc : public NamedClassDecoder {
 public:
  inline NamedDataProc()
    : NamedClassDecoder(decoder_, "DataProc")
  {}
  virtual ~NamedDataProc() {}
 protected:
  explicit inline NamedDataProc(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::DataProc decoder_;
};


class NamedDeprecated : public NamedClassDecoder {
 public:
  inline NamedDeprecated()
    : NamedClassDecoder(decoder_, "Deprecated")
  {}
  virtual ~NamedDeprecated() {}
 protected:
  explicit inline NamedDeprecated(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Deprecated decoder_;
};


class NamedEffectiveNoOp : public NamedClassDecoder {
 public:
  inline NamedEffectiveNoOp()
    : NamedClassDecoder(decoder_, "EffectiveNoOp")
  {}
  virtual ~NamedEffectiveNoOp() {}
 protected:
  explicit inline NamedEffectiveNoOp(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::EffectiveNoOp decoder_;
};


class NamedForbidden : public NamedClassDecoder {
 public:
  inline NamedForbidden()
    : NamedClassDecoder(decoder_, "Forbidden")
  {}
  virtual ~NamedForbidden() {}
 protected:
  explicit inline NamedForbidden(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Forbidden decoder_;
};


class NamedImmediateBic : public NamedClassDecoder {
 public:
  inline NamedImmediateBic()
    : NamedClassDecoder(decoder_, "ImmediateBic")
  {}
  virtual ~NamedImmediateBic() {}
 protected:
  explicit inline NamedImmediateBic(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::ImmediateBic decoder_;
};


class NamedLoadCoprocessor : public NamedClassDecoder {
 public:
  inline NamedLoadCoprocessor()
    : NamedClassDecoder(decoder_, "LoadCoprocessor")
  {}
  virtual ~NamedLoadCoprocessor() {}
 protected:
  explicit inline NamedLoadCoprocessor(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadCoprocessor decoder_;
};


class NamedLoadDoubleExclusive : public NamedClassDecoder {
 public:
  inline NamedLoadDoubleExclusive()
    : NamedClassDecoder(decoder_, "LoadDoubleExclusive")
  {}
  virtual ~NamedLoadDoubleExclusive() {}
 protected:
  explicit inline NamedLoadDoubleExclusive(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadDoubleExclusive decoder_;
};


class NamedLoadDoubleI : public NamedClassDecoder {
 public:
  inline NamedLoadDoubleI()
    : NamedClassDecoder(decoder_, "LoadDoubleI")
  {}
  virtual ~NamedLoadDoubleI() {}
 protected:
  explicit inline NamedLoadDoubleI(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadDoubleI decoder_;
};


class NamedLoadDoubleR : public NamedClassDecoder {
 public:
  inline NamedLoadDoubleR()
    : NamedClassDecoder(decoder_, "LoadDoubleR")
  {}
  virtual ~NamedLoadDoubleR() {}
 protected:
  explicit inline NamedLoadDoubleR(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadDoubleR decoder_;
};


class NamedLoadExclusive : public NamedClassDecoder {
 public:
  inline NamedLoadExclusive()
    : NamedClassDecoder(decoder_, "LoadExclusive")
  {}
  virtual ~NamedLoadExclusive() {}
 protected:
  explicit inline NamedLoadExclusive(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadExclusive decoder_;
};


class NamedLoadImmediate : public NamedClassDecoder {
 public:
  inline NamedLoadImmediate()
    : NamedClassDecoder(decoder_, "LoadImmediate")
  {}
  virtual ~NamedLoadImmediate() {}
 protected:
  explicit inline NamedLoadImmediate(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadImmediate decoder_;
};


class NamedLoadMultiple : public NamedClassDecoder {
 public:
  inline NamedLoadMultiple()
    : NamedClassDecoder(decoder_, "LoadMultiple")
  {}
  virtual ~NamedLoadMultiple() {}
 protected:
  explicit inline NamedLoadMultiple(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadMultiple decoder_;
};


class NamedLoadRegister : public NamedClassDecoder {
 public:
  inline NamedLoadRegister()
    : NamedClassDecoder(decoder_, "LoadRegister")
  {}
  virtual ~NamedLoadRegister() {}
 protected:
  explicit inline NamedLoadRegister(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LoadRegister decoder_;
};


class NamedLongMultiply : public NamedClassDecoder {
 public:
  inline NamedLongMultiply()
    : NamedClassDecoder(decoder_, "LongMultiply")
  {}
  virtual ~NamedLongMultiply() {}
 protected:
  explicit inline NamedLongMultiply(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::LongMultiply decoder_;
};


class NamedMoveDoubleFromCoprocessor : public NamedClassDecoder {
 public:
  inline NamedMoveDoubleFromCoprocessor()
    : NamedClassDecoder(decoder_, "MoveDoubleFromCoprocessor")
  {}
  virtual ~NamedMoveDoubleFromCoprocessor() {}
 protected:
  explicit inline NamedMoveDoubleFromCoprocessor(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::MoveDoubleFromCoprocessor decoder_;
};


class NamedMoveFromCoprocessor : public NamedClassDecoder {
 public:
  inline NamedMoveFromCoprocessor()
    : NamedClassDecoder(decoder_, "MoveFromCoprocessor")
  {}
  virtual ~NamedMoveFromCoprocessor() {}
 protected:
  explicit inline NamedMoveFromCoprocessor(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::MoveFromCoprocessor decoder_;
};


class NamedMoveToStatusRegister : public NamedClassDecoder {
 public:
  inline NamedMoveToStatusRegister()
    : NamedClassDecoder(decoder_, "MoveToStatusRegister")
  {}
  virtual ~NamedMoveToStatusRegister() {}
 protected:
  explicit inline NamedMoveToStatusRegister(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::MoveToStatusRegister decoder_;
};


class NamedMultiply : public NamedClassDecoder {
 public:
  inline NamedMultiply()
    : NamedClassDecoder(decoder_, "Multiply")
  {}
  virtual ~NamedMultiply() {}
 protected:
  explicit inline NamedMultiply(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Multiply decoder_;
};


class NamedPackSatRev : public NamedClassDecoder {
 public:
  inline NamedPackSatRev()
    : NamedClassDecoder(decoder_, "PackSatRev")
  {}
  virtual ~NamedPackSatRev() {}
 protected:
  explicit inline NamedPackSatRev(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::PackSatRev decoder_;
};


class NamedRoadblock : public NamedClassDecoder {
 public:
  inline NamedRoadblock()
    : NamedClassDecoder(decoder_, "Roadblock")
  {}
  virtual ~NamedRoadblock() {}
 protected:
  explicit inline NamedRoadblock(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Roadblock decoder_;
};


class NamedSatAddSub : public NamedClassDecoder {
 public:
  inline NamedSatAddSub()
    : NamedClassDecoder(decoder_, "SatAddSub")
  {}
  virtual ~NamedSatAddSub() {}
 protected:
  explicit inline NamedSatAddSub(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::SatAddSub decoder_;
};


class NamedStoreCoprocessor : public NamedClassDecoder {
 public:
  inline NamedStoreCoprocessor()
    : NamedClassDecoder(decoder_, "StoreCoprocessor")
  {}
  virtual ~NamedStoreCoprocessor() {}
 protected:
  explicit inline NamedStoreCoprocessor(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::StoreCoprocessor decoder_;
};


class NamedStoreExclusive : public NamedClassDecoder {
 public:
  inline NamedStoreExclusive()
    : NamedClassDecoder(decoder_, "StoreExclusive")
  {}
  virtual ~NamedStoreExclusive() {}
 protected:
  explicit inline NamedStoreExclusive(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::StoreExclusive decoder_;
};


class NamedStoreImmediate : public NamedClassDecoder {
 public:
  inline NamedStoreImmediate()
    : NamedClassDecoder(decoder_, "StoreImmediate")
  {}
  virtual ~NamedStoreImmediate() {}
 protected:
  explicit inline NamedStoreImmediate(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::StoreImmediate decoder_;
};


class NamedStoreRegister : public NamedClassDecoder {
 public:
  inline NamedStoreRegister()
    : NamedClassDecoder(decoder_, "StoreRegister")
  {}
  virtual ~NamedStoreRegister() {}
 protected:
  explicit inline NamedStoreRegister(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::StoreRegister decoder_;
};


class NamedTest : public NamedClassDecoder {
 public:
  inline NamedTest()
    : NamedClassDecoder(decoder_, "Test")
  {}
  virtual ~NamedTest() {}
 protected:
  explicit inline NamedTest(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Test decoder_;
};


class NamedTestImmediate : public NamedClassDecoder {
 public:
  inline NamedTestImmediate()
    : NamedClassDecoder(decoder_, "TestImmediate")
  {}
  virtual ~NamedTestImmediate() {}
 protected:
  explicit inline NamedTestImmediate(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::TestImmediate decoder_;
};


class NamedUnary3RegisterShiftedOp : public NamedClassDecoder {
 public:
  inline NamedUnary3RegisterShiftedOp()
    : NamedClassDecoder(decoder_, "Unary3RegisterShiftedOp")
  {}
  virtual ~NamedUnary3RegisterShiftedOp() {}
 protected:
  explicit inline NamedUnary3RegisterShiftedOp(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Unary3RegisterShiftedOp decoder_;
};


class NamedUndefined : public NamedClassDecoder {
 public:
  inline NamedUndefined()
    : NamedClassDecoder(decoder_, "Undefined")
  {}
  virtual ~NamedUndefined() {}
 protected:
  explicit inline NamedUndefined(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Undefined decoder_;
};


class NamedUnpredictable : public NamedClassDecoder {
 public:
  inline NamedUnpredictable()
    : NamedClassDecoder(decoder_, "Unpredictable")
  {}
  virtual ~NamedUnpredictable() {}
 protected:
  explicit inline NamedUnpredictable(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::Unpredictable decoder_;
};


class NamedVectorLoad : public NamedClassDecoder {
 public:
  inline NamedVectorLoad()
    : NamedClassDecoder(decoder_, "VectorLoad")
  {}
  virtual ~NamedVectorLoad() {}
 protected:
  explicit inline NamedVectorLoad(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::VectorLoad decoder_;
};


class NamedVectorStore : public NamedClassDecoder {
 public:
  inline NamedVectorStore()
    : NamedClassDecoder(decoder_, "VectorStore")
  {}
  virtual ~NamedVectorStore() {}
 protected:
  explicit inline NamedVectorStore(const char* name)
    : NamedClassDecoder(decoder_, name) {}
 private:
  nacl_arm_dec::VectorStore decoder_;
};


class NamedAdd_Rule_7_A1_P26Binary4RegisterShiftedOp
    : public NamedBinary4RegisterShiftedOp {
 public:
  inline NamedAdd_Rule_7_A1_P26Binary4RegisterShiftedOp()
    : NamedBinary4RegisterShiftedOp("Add_Rule_7_A1_P26Binary4RegisterShiftedOp")
  {}
  virtual ~NamedAdd_Rule_7_A1_P26Binary4RegisterShiftedOp() {}
};


class NamedRsb_Rule_144_A1_P288Binary4RegisterShiftedOp
    : public NamedBinary4RegisterShiftedOp {
 public:
  inline NamedRsb_Rule_144_A1_P288Binary4RegisterShiftedOp()
    : NamedBinary4RegisterShiftedOp("Rsb_Rule_144_A1_P288Binary4RegisterShiftedOp")
  {}
  virtual ~NamedRsb_Rule_144_A1_P288Binary4RegisterShiftedOp() {}
};


} // namespace nacl_arm_test
#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_GEN_ARM32_DECODE_NAMED_CLASSES_H_
