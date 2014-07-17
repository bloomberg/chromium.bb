// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/bpf_dsl.h"

#include <errno.h>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

using namespace sandbox::bpf_dsl::internal;
typedef ::sandbox::Trap::TrapFnc TrapFnc;

namespace sandbox {
namespace bpf_dsl {
namespace {

class AllowResultExprImpl : public ResultExprImpl {
 public:
  AllowResultExprImpl() {}
  virtual ErrorCode Compile(SandboxBPF* sb) const OVERRIDE {
    return ErrorCode(ErrorCode::ERR_ALLOWED);
  }

 private:
  virtual ~AllowResultExprImpl() {}
  DISALLOW_COPY_AND_ASSIGN(AllowResultExprImpl);
};

class ErrorResultExprImpl : public ResultExprImpl {
 public:
  explicit ErrorResultExprImpl(int err) : err_(err) {
    CHECK(err_ >= ErrorCode::ERR_MIN_ERRNO && err_ <= ErrorCode::ERR_MAX_ERRNO);
  }
  virtual ErrorCode Compile(SandboxBPF* sb) const OVERRIDE {
    return ErrorCode(err_);
  }

 private:
  virtual ~ErrorResultExprImpl() {}
  int err_;
  DISALLOW_COPY_AND_ASSIGN(ErrorResultExprImpl);
};

class TrapResultExprImpl : public ResultExprImpl {
 public:
  TrapResultExprImpl(TrapFnc func, void* arg) : func_(func), arg_(arg) {
    DCHECK(func_);
  }
  virtual ErrorCode Compile(SandboxBPF* sb) const OVERRIDE {
    return sb->Trap(func_, arg_);
  }

 private:
  virtual ~TrapResultExprImpl() {}
  TrapFnc func_;
  void* arg_;
  DISALLOW_COPY_AND_ASSIGN(TrapResultExprImpl);
};

class IfThenResultExprImpl : public ResultExprImpl {
 public:
  IfThenResultExprImpl(BoolExpr cond,
                       ResultExpr then_result,
                       ResultExpr else_result)
      : cond_(cond), then_result_(then_result), else_result_(else_result) {}
  virtual ErrorCode Compile(SandboxBPF* sb) const OVERRIDE {
    return cond_->Compile(
        sb, then_result_->Compile(sb), else_result_->Compile(sb));
  }

 private:
  virtual ~IfThenResultExprImpl() {}
  BoolExpr cond_;
  ResultExpr then_result_;
  ResultExpr else_result_;
  DISALLOW_COPY_AND_ASSIGN(IfThenResultExprImpl);
};

class PrimitiveBoolExprImpl : public BoolExprImpl {
 public:
  PrimitiveBoolExprImpl(int argno,
                        ErrorCode::ArgType is_32bit,
                        ErrorCode::Operation op,
                        uint64_t value)
      : argno_(argno), is_32bit_(is_32bit), op_(op), value_(value) {}
  virtual ErrorCode Compile(SandboxBPF* sb,
                            ErrorCode true_ec,
                            ErrorCode false_ec) const OVERRIDE {
    return sb->Cond(argno_, is_32bit_, op_, value_, true_ec, false_ec);
  }

 private:
  virtual ~PrimitiveBoolExprImpl() {}
  int argno_;
  ErrorCode::ArgType is_32bit_;
  ErrorCode::Operation op_;
  uint64_t value_;
  DISALLOW_COPY_AND_ASSIGN(PrimitiveBoolExprImpl);
};

class NegateBoolExprImpl : public BoolExprImpl {
 public:
  explicit NegateBoolExprImpl(BoolExpr cond) : cond_(cond) {}
  virtual ErrorCode Compile(SandboxBPF* sb,
                            ErrorCode true_ec,
                            ErrorCode false_ec) const OVERRIDE {
    return cond_->Compile(sb, false_ec, true_ec);
  }

 private:
  virtual ~NegateBoolExprImpl() {}
  BoolExpr cond_;
  DISALLOW_COPY_AND_ASSIGN(NegateBoolExprImpl);
};

class AndBoolExprImpl : public BoolExprImpl {
 public:
  AndBoolExprImpl(BoolExpr lhs, BoolExpr rhs) : lhs_(lhs), rhs_(rhs) {}
  virtual ErrorCode Compile(SandboxBPF* sb,
                            ErrorCode true_ec,
                            ErrorCode false_ec) const OVERRIDE {
    return lhs_->Compile(sb, rhs_->Compile(sb, true_ec, false_ec), false_ec);
  }

 private:
  virtual ~AndBoolExprImpl() {}
  BoolExpr lhs_, rhs_;
  DISALLOW_COPY_AND_ASSIGN(AndBoolExprImpl);
};

class OrBoolExprImpl : public BoolExprImpl {
 public:
  OrBoolExprImpl(BoolExpr lhs, BoolExpr rhs) : lhs_(lhs), rhs_(rhs) {}
  virtual ErrorCode Compile(SandboxBPF* sb,
                            ErrorCode true_ec,
                            ErrorCode false_ec) const OVERRIDE {
    return lhs_->Compile(sb, true_ec, rhs_->Compile(sb, true_ec, false_ec));
  }

 private:
  virtual ~OrBoolExprImpl() {}
  BoolExpr lhs_, rhs_;
  DISALLOW_COPY_AND_ASSIGN(OrBoolExprImpl);
};

}  // namespace

namespace internal {

BoolExpr ArgEq(int num, size_t size, uint64_t mask, uint64_t val) {
  CHECK(num >= 0 && num < 6);
  CHECK(size >= 1 && size <= 8);
  CHECK_NE(0U, mask) << "zero mask doesn't make sense";
  CHECK_EQ(val, val & mask) << "val contains masked out bits";

  // TODO(mdempsky): Should we just always use TP_64BIT?
  const ErrorCode::ArgType arg_type =
      (size <= 4) ? ErrorCode::TP_32BIT : ErrorCode::TP_64BIT;

  if (mask == static_cast<uint64_t>(-1)) {
    // Arg == Val
    return BoolExpr(new const PrimitiveBoolExprImpl(
        num, arg_type, ErrorCode::OP_EQUAL, val));
  } else if (mask == val) {
    // (Arg & Mask) == Mask
    return BoolExpr(new const PrimitiveBoolExprImpl(
        num, arg_type, ErrorCode::OP_HAS_ALL_BITS, mask));
  } else if (val == 0) {
    // (Arg & Mask) == 0, which is semantically equivalent to !((arg & mask) !=
    // 0).
    return !BoolExpr(new const PrimitiveBoolExprImpl(
        num, arg_type, ErrorCode::OP_HAS_ANY_BITS, mask));
  } else {
    CHECK(false) << "Unimplemented ArgEq case";
    return BoolExpr();
  }
}

}  // namespace internal

ResultExpr Allow() {
  return ResultExpr(new const AllowResultExprImpl());
}

ResultExpr Error(int err) {
  return ResultExpr(new const ErrorResultExprImpl(err));
}

ResultExpr Trap(TrapFnc trap_func, void* aux) {
  return ResultExpr(new const TrapResultExprImpl(trap_func, aux));
}

BoolExpr operator!(BoolExpr cond) {
  return BoolExpr(new const NegateBoolExprImpl(cond));
}

BoolExpr operator&&(BoolExpr lhs, BoolExpr rhs) {
  return BoolExpr(new const AndBoolExprImpl(lhs, rhs));
}

BoolExpr operator||(BoolExpr lhs, BoolExpr rhs) {
  return BoolExpr(new const OrBoolExprImpl(lhs, rhs));
}

Elser If(BoolExpr cond, ResultExpr then_result) {
  return Elser(Cons<Elser::Clause>::List()).ElseIf(cond, then_result);
}

Elser::Elser(Cons<Clause>::List clause_list) : clause_list_(clause_list) {
}

Elser::Elser(const Elser& elser) : clause_list_(elser.clause_list_) {
}

Elser::~Elser() {
}

Elser Elser::ElseIf(BoolExpr cond, ResultExpr then_result) const {
  return Elser(
      Cons<Clause>::Make(std::make_pair(cond, then_result), clause_list_));
}

ResultExpr Elser::Else(ResultExpr else_result) const {
  // We finally have the default result expression for this
  // if/then/else sequence.  Also, we've already accumulated all
  // if/then pairs into a list of reverse order (i.e., lower priority
  // conditions are listed before higher priority ones).  E.g., an
  // expression like
  //
  //    If(b1, e1).ElseIf(b2, e2).ElseIf(b3, e3).Else(e4)
  //
  // will have built up a list like
  //
  //    [(b3, e3), (b2, e2), (b1, e1)].
  //
  // Now that we have e4, we can walk the list and create a ResultExpr
  // tree like:
  //
  //    expr = e4
  //    expr = (b3 ? e3 : expr) = (b3 ? e3 : e4)
  //    expr = (b2 ? e2 : expr) = (b2 ? e2 : (b3 ? e3 : e4))
  //    expr = (b1 ? e1 : expr) = (b1 ? e1 : (b2 ? e2 : (b3 ? e3 : e4)))
  //
  // and end up with an appropriately chained tree.

  ResultExpr expr = else_result;
  for (Cons<Clause>::List it = clause_list_; it; it = it->tail()) {
    Clause clause = it->head();
    expr = ResultExpr(
        new const IfThenResultExprImpl(clause.first, clause.second, expr));
  }
  return expr;
}

ResultExpr SandboxBPFDSLPolicy::InvalidSyscall() const {
  return Error(ENOSYS);
}

ErrorCode SandboxBPFDSLPolicy::EvaluateSyscall(SandboxBPF* sb,
                                               int sysno) const {
  return EvaluateSyscall(sysno)->Compile(sb);
}

ErrorCode SandboxBPFDSLPolicy::InvalidSyscall(SandboxBPF* sb) const {
  return InvalidSyscall()->Compile(sb);
}

ResultExpr SandboxBPFDSLPolicy::Trap(::sandbox::Trap::TrapFnc trap_func,
                                     void* aux) {
  return bpf_dsl::Trap(trap_func, aux);
}

}  // namespace bpf_dsl
}  // namespace sandbox
