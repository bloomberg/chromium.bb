// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_
#define SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_forward.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace bpf_dsl {
class Policy;

// PolicyCompiler implements the bpf_dsl compiler, allowing users to
// transform bpf_dsl policies into BPF programs to be executed by the
// Linux kernel.
class SANDBOX_EXPORT PolicyCompiler {
 public:
  PolicyCompiler(const Policy* policy, TrapRegistry* registry);
  ~PolicyCompiler();

  // Compile registers any trap handlers needed by the policy and
  // compiles the policy to a BPF program, which it returns.
  scoped_ptr<CodeGen::Program> Compile(bool verify);

  // DangerousSetEscapePC sets the "escape PC" that is allowed to issue any
  // system calls, regardless of policy.
  void DangerousSetEscapePC(uint64_t escapepc);

  // Error returns an ErrorCode to indicate the system call should fail with
  // the specified error number.
  ErrorCode Error(int err);

  // Trap returns an ErrorCode to indicate the system call should
  // instead invoke a trap handler.
  ErrorCode Trap(TrapRegistry::TrapFnc fnc, const void* aux, bool safe);

  // UnsafeTraps require some syscalls to always be allowed.
  // This helper function returns true for these calls.
  static bool IsRequiredForUnsafeTrap(int sysno);

  // We can also use ErrorCode to request evaluation of a conditional
  // statement based on inspection of system call parameters.
  // This method wrap an ErrorCode object around the conditional statement.
  // Argument "argno" (1..6) will be bitwise-AND'd with "mask" and compared
  // to "value"; if equal, then "passed" will be returned, otherwise "failed".
  // If "is32bit" is set, the argument must in the range of 0x0..(1u << 32 - 1)
  // If it is outside this range, the sandbox treats the system call just
  // the same as any other ABI violation (i.e. it aborts with an error
  // message).
  ErrorCode CondMaskedEqual(int argno,
                            ErrorCode::ArgType is_32bit,
                            uint64_t mask,
                            uint64_t value,
                            const ErrorCode& passed,
                            const ErrorCode& failed);

  // Returns the fatal ErrorCode that is used to indicate that somebody
  // attempted to pass a 64bit value in a 32bit system call argument.
  // This method is primarily needed for testing purposes.
  ErrorCode Unexpected64bitArgument();

 private:
  struct Range;
  typedef std::vector<Range> Ranges;
  typedef std::set<ErrorCode, struct ErrorCode::LessThan> Conds;

  // Used by CondExpressionHalf to track which half of the argument it's
  // emitting instructions for.
  enum ArgHalf {
    LowerHalf,
    UpperHalf,
  };

  // Compile the configured policy into a complete instruction sequence.
  CodeGen::Node AssemblePolicy();

  // Return an instruction sequence that checks the
  // arch_seccomp_data's "arch" field is valid, and then passes
  // control to |passed| if so.
  CodeGen::Node CheckArch(CodeGen::Node passed);

  // If |has_unsafe_traps_| is true, returns an instruction sequence
  // that allows all system calls from |escapepc_|, and otherwise
  // passes control to |rest|. Otherwise, simply returns |rest|.
  CodeGen::Node MaybeAddEscapeHatch(CodeGen::Node rest);

  // Return an instruction sequence that loads and checks the system
  // call number, performs a binary search, and then dispatches to an
  // appropriate instruction sequence compiled from the current
  // policy.
  CodeGen::Node DispatchSyscall();

  // Return an instruction sequence that checks the system call number
  // (expected to be loaded in register A) and if valid, passes
  // control to |passed| (with register A still valid).
  CodeGen::Node CheckSyscallNumber(CodeGen::Node passed);

  // Finds all the ranges of system calls that need to be handled. Ranges are
  // sorted in ascending order of system call numbers. There are no gaps in the
  // ranges. System calls with identical ErrorCodes are coalesced into a single
  // range.
  void FindRanges(Ranges* ranges);

  // Returns a BPF program snippet that implements a jump table for the
  // given range of system call numbers. This function runs recursively.
  CodeGen::Node AssembleJumpTable(Ranges::const_iterator start,
                                  Ranges::const_iterator stop);

  // CompileResult compiles an individual result expression into a
  // CodeGen node.
  CodeGen::Node CompileResult(const ResultExpr& res);

  // Returns a BPF program snippet that makes the BPF filter program exit
  // with the given ErrorCode "err". N.B. the ErrorCode may very well be a
  // conditional expression; if so, this function will recursively call
  // CondExpression() and possibly RetExpression() to build a complex set of
  // instructions.
  CodeGen::Node RetExpression(const ErrorCode& err);

  // Returns a BPF program that evaluates the conditional expression in
  // "cond" and returns the appropriate value from the BPF filter program.
  // This function recursively calls RetExpression(); it should only ever be
  // called from RetExpression().
  CodeGen::Node CondExpression(const ErrorCode& cond);

  // Returns a BPF program that evaluates half of a conditional expression;
  // it should only ever be called from CondExpression().
  CodeGen::Node CondExpressionHalf(const ErrorCode& cond,
                                   ArgHalf half,
                                   CodeGen::Node passed,
                                   CodeGen::Node failed);

  const Policy* policy_;
  TrapRegistry* registry_;
  uint64_t escapepc_;

  Conds conds_;
  CodeGen gen_;
  bool has_unsafe_traps_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCompiler);
};

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_POLICY_COMPILER_H_
