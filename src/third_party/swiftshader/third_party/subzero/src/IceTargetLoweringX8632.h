//===- subzero/src/IceTargetLoweringX8632.h - x86-32 lowering ---*- C++ -*-===//
//
//                        The Subzero Code Generator
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Declares the TargetLoweringX8632 class, which implements the
/// TargetLowering interface for the x86-32 architecture.
///
//===----------------------------------------------------------------------===//

#ifndef SUBZERO_SRC_ICETARGETLOWERINGX8632_H
#define SUBZERO_SRC_ICETARGETLOWERINGX8632_H

#include "IceAssemblerX8632.h"
#include "IceDefs.h"
#include "IceInstX8632.h"
#include "IceRegistersX8632.h"
#include "IceTargetLowering.h"
#define X86NAMESPACE X8632
#include "IceTargetLoweringX86Base.h"
#undef X86NAMESPACE
#include "IceTargetLoweringX8632Traits.h"

namespace Ice {
namespace X8632 {

class TargetX8632 final : public ::Ice::X8632::TargetX86Base<X8632::Traits> {
  TargetX8632() = delete;
  TargetX8632(const TargetX8632 &) = delete;
  TargetX8632 &operator=(const TargetX8632 &) = delete;

public:
  ~TargetX8632() = default;

  static std::unique_ptr<::Ice::TargetLowering> create(Cfg *Func) {
    return makeUnique<TargetX8632>(Func);
  }

  std::unique_ptr<::Ice::Assembler> createAssembler() const override {
    return makeUnique<X8632::AssemblerX8632>();
  }

protected:
  void _add_sp(Operand *Adjustment);
  void _mov_sp(Operand *NewValue);
  void _sub_sp(Operand *Adjustment);
  void _link_bp();
  void _unlink_bp();
  void _push_reg(RegNumT RegNum);
  void _pop_reg(RegNumT RegNum);

  void emitStackProbe(size_t StackSizeBytes);
  void lowerIndirectJump(Variable *JumpTarget);
  Inst *emitCallToTarget(Operand *CallTarget, Variable *ReturnReg,
                         size_t NumVariadicFpArgs = 0) override;
  Variable *moveReturnValueToRegister(Operand *Value, Type ReturnType) override;

private:
  ENABLE_MAKE_UNIQUE;
  friend class X8632::TargetX86Base<X8632::Traits>;

  explicit TargetX8632(Cfg *Func) : TargetX86Base(Func) {}
};

// The -Wundefined-var-template warning requires to forward-declare static
// members of template class specializations. Note that "An explicit
// specialization of a static data member of a template is a definition if the
// declaration includes an initializer; otherwise, it is a declaration."
// Visual Studio has a bug which treats these declarations as definitions,
// leading to multiple definition errors. Since we only enable
// -Wundefined-var-template for Clang, omit these declarations on other
// compilers.
#if defined(__clang__)
template <>
std::array<SmallBitVector, RCX86_NUM>
    TargetX86Base<X8632::Traits>::TypeToRegisterSet;

template <>
std::array<SmallBitVector, RCX86_NUM>
    TargetX86Base<X8632::Traits>::TypeToRegisterSetUnfiltered;

template <>
std::array<SmallBitVector,
           TargetX86Base<X8632::Traits>::Traits::RegisterSet::Reg_NUM>
    TargetX86Base<X8632::Traits>::RegisterAliases;
#endif // defined(__clang__)

} // end of namespace X8632
} // end of namespace Ice

#endif // SUBZERO_SRC_ICETARGETLOWERINGX8632_H
