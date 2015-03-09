// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/policy_compiler.h"

#include <errno.h>
#include <linux/filter.h>
#include <sys/syscall.h>

#include <limits>

#include "base/logging.h"
#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl_impl.h"
#include "sandbox/linux/bpf_dsl/codegen.h"
#include "sandbox/linux/bpf_dsl/dump_bpf.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/bpf_dsl/seccomp_macros.h"
#include "sandbox/linux/bpf_dsl/syscall_set.h"
#include "sandbox/linux/bpf_dsl/verifier.h"
#include "sandbox/linux/seccomp-bpf/errorcode.h"
#include "sandbox/linux/system_headers/linux_seccomp.h"

namespace sandbox {
namespace bpf_dsl {

namespace {

#if defined(__i386__) || defined(__x86_64__)
const bool kIsIntel = true;
#else
const bool kIsIntel = false;
#endif
#if defined(__x86_64__) && defined(__ILP32__)
const bool kIsX32 = true;
#else
const bool kIsX32 = false;
#endif

const int kSyscallsRequiredForUnsafeTraps[] = {
    __NR_rt_sigprocmask,
    __NR_rt_sigreturn,
#if defined(__NR_sigprocmask)
    __NR_sigprocmask,
#endif
#if defined(__NR_sigreturn)
    __NR_sigreturn,
#endif
};

bool HasExactlyOneBit(uint64_t x) {
  // Common trick; e.g., see http://stackoverflow.com/a/108329.
  return x != 0 && (x & (x - 1)) == 0;
}

// A Trap() handler that returns an "errno" value. The value is encoded
// in the "aux" parameter.
intptr_t ReturnErrno(const struct arch_seccomp_data&, void* aux) {
  // TrapFnc functions report error by following the native kernel convention
  // of returning an exit code in the range of -1..-4096. They do not try to
  // set errno themselves. The glibc wrapper that triggered the SIGSYS will
  // ultimately do so for us.
  int err = reinterpret_cast<intptr_t>(aux) & SECCOMP_RET_DATA;
  return -err;
}

bool HasUnsafeTraps(const Policy* policy) {
  DCHECK(policy);
  for (uint32_t sysnum : SyscallSet::ValidOnly()) {
    if (policy->EvaluateSyscall(sysnum)->HasUnsafeTraps()) {
      return true;
    }
  }
  return policy->InvalidSyscall()->HasUnsafeTraps();
}

}  // namespace

struct PolicyCompiler::Range {
  uint32_t from;
  CodeGen::Node node;
};

PolicyCompiler::PolicyCompiler(const Policy* policy, TrapRegistry* registry)
    : policy_(policy),
      registry_(registry),
      escapepc_(0),
      conds_(),
      gen_(),
      has_unsafe_traps_(HasUnsafeTraps(policy_)) {
  DCHECK(policy);
}

PolicyCompiler::~PolicyCompiler() {
}

scoped_ptr<CodeGen::Program> PolicyCompiler::Compile(bool verify) {
  CHECK(policy_->InvalidSyscall()->IsDeny())
      << "Policies should deny invalid system calls";

  // If our BPF program has unsafe traps, enable support for them.
  if (has_unsafe_traps_) {
    CHECK_NE(0U, escapepc_) << "UnsafeTrap() requires a valid escape PC";

    for (int sysnum : kSyscallsRequiredForUnsafeTraps) {
      CHECK(policy_->EvaluateSyscall(sysnum)->IsAllow())
          << "Policies that use UnsafeTrap() must unconditionally allow all "
             "required system calls";
    }

    CHECK(registry_->EnableUnsafeTraps())
        << "We'd rather die than enable unsafe traps";
  }

  // Assemble the BPF filter program.
  scoped_ptr<CodeGen::Program> program(new CodeGen::Program());
  gen_.Compile(AssemblePolicy(), program.get());

  // Make sure compilation resulted in a BPF program that executes
  // correctly. Otherwise, there is an internal error in our BPF compiler.
  // There is really nothing the caller can do until the bug is fixed.
  if (verify) {
    const char* err = nullptr;
    if (!Verifier::VerifyBPF(this, *program, *policy_, &err)) {
      DumpBPF::PrintProgram(*program);
      LOG(FATAL) << err;
    }
  }

  return program.Pass();
}

void PolicyCompiler::DangerousSetEscapePC(uint64_t escapepc) {
  escapepc_ = escapepc;
}

CodeGen::Node PolicyCompiler::AssemblePolicy() {
  // A compiled policy consists of three logical parts:
  //   1. Check that the "arch" field matches the expected architecture.
  //   2. If the policy involves unsafe traps, check if the syscall was
  //      invoked by Syscall::Call, and then allow it unconditionally.
  //   3. Check the system call number and jump to the appropriate compiled
  //      system call policy number.
  return CheckArch(MaybeAddEscapeHatch(DispatchSyscall()));
}

CodeGen::Node PolicyCompiler::CheckArch(CodeGen::Node passed) {
  // If the architecture doesn't match SECCOMP_ARCH, disallow the
  // system call.
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, SECCOMP_ARCH_IDX,
      gen_.MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K, SECCOMP_ARCH, passed,
          CompileResult(Kill("Invalid audit architecture in BPF filter"))));
}

CodeGen::Node PolicyCompiler::MaybeAddEscapeHatch(CodeGen::Node rest) {
  // If no unsafe traps, then simply return |rest|.
  if (!has_unsafe_traps_) {
    return rest;
  }

  // We already enabled unsafe traps in Compile, but enable them again to give
  // the trap registry a second chance to complain before we add the backdoor.
  CHECK(registry_->EnableUnsafeTraps());

  // Allow system calls, if they originate from our magic return address.
  const uint32_t lopc = static_cast<uint32_t>(escapepc_);
  const uint32_t hipc = static_cast<uint32_t>(escapepc_ >> 32);

  // BPF cannot do native 64-bit comparisons, so we have to compare
  // both 32-bit halves of the instruction pointer. If they match what
  // we expect, we return ERR_ALLOWED. If either or both don't match,
  // we continue evalutating the rest of the sandbox policy.
  //
  // For simplicity, we check the full 64-bit instruction pointer even
  // on 32-bit architectures.
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, SECCOMP_IP_LSB_IDX,
      gen_.MakeInstruction(
          BPF_JMP + BPF_JEQ + BPF_K, lopc,
          gen_.MakeInstruction(
              BPF_LD + BPF_W + BPF_ABS, SECCOMP_IP_MSB_IDX,
              gen_.MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, hipc,
                                   CompileResult(Allow()), rest)),
          rest));
}

CodeGen::Node PolicyCompiler::DispatchSyscall() {
  // Evaluate all possible system calls and group their ErrorCodes into
  // ranges of identical codes.
  Ranges ranges;
  FindRanges(&ranges);

  // Compile the system call ranges to an optimized BPF jumptable
  CodeGen::Node jumptable = AssembleJumpTable(ranges.begin(), ranges.end());

  // Grab the system call number, so that we can check it and then
  // execute the jump table.
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS, SECCOMP_NR_IDX, CheckSyscallNumber(jumptable));
}

CodeGen::Node PolicyCompiler::CheckSyscallNumber(CodeGen::Node passed) {
  if (kIsIntel) {
    // On Intel architectures, verify that system call numbers are in the
    // expected number range.
    CodeGen::Node invalidX32 =
        CompileResult(Kill("Illegal mixing of system call ABIs"));
    if (kIsX32) {
      // The newer x32 API always sets bit 30.
      return gen_.MakeInstruction(
          BPF_JMP + BPF_JSET + BPF_K, 0x40000000, passed, invalidX32);
    } else {
      // The older i386 and x86-64 APIs clear bit 30 on all system calls.
      return gen_.MakeInstruction(
          BPF_JMP + BPF_JSET + BPF_K, 0x40000000, invalidX32, passed);
    }
  }

  // TODO(mdempsky): Similar validation for other architectures?
  return passed;
}

void PolicyCompiler::FindRanges(Ranges* ranges) {
  // Please note that "struct seccomp_data" defines system calls as a signed
  // int32_t, but BPF instructions always operate on unsigned quantities. We
  // deal with this disparity by enumerating from MIN_SYSCALL to MAX_SYSCALL,
  // and then verifying that the rest of the number range (both positive and
  // negative) all return the same ErrorCode.
  const CodeGen::Node invalid_node = CompileResult(policy_->InvalidSyscall());
  uint32_t old_sysnum = 0;
  CodeGen::Node old_node =
      SyscallSet::IsValid(old_sysnum)
          ? CompileResult(policy_->EvaluateSyscall(old_sysnum))
          : invalid_node;

  for (uint32_t sysnum : SyscallSet::All()) {
    CodeGen::Node node =
        SyscallSet::IsValid(sysnum)
            ? CompileResult(policy_->EvaluateSyscall(static_cast<int>(sysnum)))
            : invalid_node;
    // N.B., here we rely on CodeGen folding (i.e., returning the same
    // node value for) identical code sequences, otherwise our jump
    // table will blow up in size.
    if (node != old_node) {
      ranges->push_back(Range{old_sysnum, old_node});
      old_sysnum = sysnum;
      old_node = node;
    }
  }
  ranges->push_back(Range{old_sysnum, old_node});
}

CodeGen::Node PolicyCompiler::AssembleJumpTable(Ranges::const_iterator start,
                                                Ranges::const_iterator stop) {
  // We convert the list of system call ranges into jump table that performs
  // a binary search over the ranges.
  // As a sanity check, we need to have at least one distinct ranges for us
  // to be able to build a jump table.
  CHECK(start < stop) << "Invalid iterator range";
  const auto n = stop - start;
  if (n == 1) {
    // If we have narrowed things down to a single range object, we can
    // return from the BPF filter program.
    return start->node;
  }

  // Pick the range object that is located at the mid point of our list.
  // We compare our system call number against the lowest valid system call
  // number in this range object. If our number is lower, it is outside of
  // this range object. If it is greater or equal, it might be inside.
  Ranges::const_iterator mid = start + n / 2;

  // Sub-divide the list of ranges and continue recursively.
  CodeGen::Node jf = AssembleJumpTable(start, mid);
  CodeGen::Node jt = AssembleJumpTable(mid, stop);
  return gen_.MakeInstruction(BPF_JMP + BPF_JGE + BPF_K, mid->from, jt, jf);
}

CodeGen::Node PolicyCompiler::CompileResult(const ResultExpr& res) {
  return RetExpression(res->Compile(this));
}

CodeGen::Node PolicyCompiler::RetExpression(const ErrorCode& err) {
  switch (err.error_type()) {
    case ErrorCode::ET_COND:
      return CondExpression(err);
    case ErrorCode::ET_SIMPLE:
    case ErrorCode::ET_TRAP:
      return gen_.MakeInstruction(BPF_RET + BPF_K, err.err());
    default:
      LOG(FATAL)
          << "ErrorCode is not suitable for returning from a BPF program";
      return CodeGen::kNullNode;
  }
}

CodeGen::Node PolicyCompiler::CondExpression(const ErrorCode& cond) {
  // Sanity check that |cond| makes sense.
  CHECK(cond.argno_ >= 0 && cond.argno_ < 6) << "Invalid argument number "
                                             << cond.argno_;
  CHECK(cond.width_ == ErrorCode::TP_32BIT ||
        cond.width_ == ErrorCode::TP_64BIT)
      << "Invalid argument width " << cond.width_;
  CHECK_NE(0U, cond.mask_) << "Zero mask is invalid";
  CHECK_EQ(cond.value_, cond.value_ & cond.mask_)
      << "Value contains masked out bits";
  if (sizeof(void*) == 4) {
    CHECK_EQ(ErrorCode::TP_32BIT, cond.width_)
        << "Invalid width on 32-bit platform";
  }
  if (cond.width_ == ErrorCode::TP_32BIT) {
    CHECK_EQ(0U, cond.mask_ >> 32) << "Mask exceeds argument size";
    CHECK_EQ(0U, cond.value_ >> 32) << "Value exceeds argument size";
  }

  CodeGen::Node passed = RetExpression(*cond.passed_);
  CodeGen::Node failed = RetExpression(*cond.failed_);

  // We want to emit code to check "(arg & mask) == value" where arg, mask, and
  // value are 64-bit values, but the BPF machine is only 32-bit. We implement
  // this by independently testing the upper and lower 32-bits and continuing to
  // |passed| if both evaluate true, or to |failed| if either evaluate false.
  return CondExpressionHalf(cond,
                            UpperHalf,
                            CondExpressionHalf(cond, LowerHalf, passed, failed),
                            failed);
}

CodeGen::Node PolicyCompiler::CondExpressionHalf(const ErrorCode& cond,
                                                 ArgHalf half,
                                                 CodeGen::Node passed,
                                                 CodeGen::Node failed) {
  if (cond.width_ == ErrorCode::TP_32BIT && half == UpperHalf) {
    // Special logic for sanity checking the upper 32-bits of 32-bit system
    // call arguments.

    // TODO(mdempsky): Compile Unexpected64bitArgument() just per program.
    CodeGen::Node invalid_64bit = RetExpression(Unexpected64bitArgument());

    const uint32_t upper = SECCOMP_ARG_MSB_IDX(cond.argno_);
    const uint32_t lower = SECCOMP_ARG_LSB_IDX(cond.argno_);

    if (sizeof(void*) == 4) {
      // On 32-bit platforms, the upper 32-bits should always be 0:
      //   LDW  [upper]
      //   JEQ  0, passed, invalid
      return gen_.MakeInstruction(
          BPF_LD + BPF_W + BPF_ABS,
          upper,
          gen_.MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K, 0, passed, invalid_64bit));
    }

    // On 64-bit platforms, the upper 32-bits may be 0 or ~0; but we only allow
    // ~0 if the sign bit of the lower 32-bits is set too:
    //   LDW  [upper]
    //   JEQ  0, passed, (next)
    //   JEQ  ~0, (next), invalid
    //   LDW  [lower]
    //   JSET (1<<31), passed, invalid
    //
    // TODO(mdempsky): The JSET instruction could perhaps jump to passed->next
    // instead, as the first instruction of passed should be "LDW [lower]".
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        upper,
        gen_.MakeInstruction(
            BPF_JMP + BPF_JEQ + BPF_K,
            0,
            passed,
            gen_.MakeInstruction(
                BPF_JMP + BPF_JEQ + BPF_K,
                std::numeric_limits<uint32_t>::max(),
                gen_.MakeInstruction(
                    BPF_LD + BPF_W + BPF_ABS,
                    lower,
                    gen_.MakeInstruction(BPF_JMP + BPF_JSET + BPF_K,
                                         1U << 31,
                                         passed,
                                         invalid_64bit)),
                invalid_64bit)));
  }

  const uint32_t idx = (half == UpperHalf) ? SECCOMP_ARG_MSB_IDX(cond.argno_)
                                           : SECCOMP_ARG_LSB_IDX(cond.argno_);
  const uint32_t mask = (half == UpperHalf) ? cond.mask_ >> 32 : cond.mask_;
  const uint32_t value = (half == UpperHalf) ? cond.value_ >> 32 : cond.value_;

  // Emit a suitable instruction sequence for (arg & mask) == value.

  // For (arg & 0) == 0, just return passed.
  if (mask == 0) {
    CHECK_EQ(0U, value);
    return passed;
  }

  // For (arg & ~0) == value, emit:
  //   LDW  [idx]
  //   JEQ  value, passed, failed
  if (mask == std::numeric_limits<uint32_t>::max()) {
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen_.MakeInstruction(BPF_JMP + BPF_JEQ + BPF_K, value, passed, failed));
  }

  // For (arg & mask) == 0, emit:
  //   LDW  [idx]
  //   JSET mask, failed, passed
  // (Note: failed and passed are intentionally swapped.)
  if (value == 0) {
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen_.MakeInstruction(BPF_JMP + BPF_JSET + BPF_K, mask, failed, passed));
  }

  // For (arg & x) == x where x is a single-bit value, emit:
  //   LDW  [idx]
  //   JSET mask, passed, failed
  if (mask == value && HasExactlyOneBit(mask)) {
    return gen_.MakeInstruction(
        BPF_LD + BPF_W + BPF_ABS,
        idx,
        gen_.MakeInstruction(BPF_JMP + BPF_JSET + BPF_K, mask, passed, failed));
  }

  // Generic fallback:
  //   LDW  [idx]
  //   AND  mask
  //   JEQ  value, passed, failed
  return gen_.MakeInstruction(
      BPF_LD + BPF_W + BPF_ABS,
      idx,
      gen_.MakeInstruction(
          BPF_ALU + BPF_AND + BPF_K,
          mask,
          gen_.MakeInstruction(
              BPF_JMP + BPF_JEQ + BPF_K, value, passed, failed)));
}

ErrorCode PolicyCompiler::Unexpected64bitArgument() {
  return Kill("Unexpected 64bit argument detected")->Compile(this);
}

ErrorCode PolicyCompiler::Error(int err) {
  if (has_unsafe_traps_) {
    // When inside an UnsafeTrap() callback, we want to allow all system calls.
    // This means, we must conditionally disable the sandbox -- and that's not
    // something that kernel-side BPF filters can do, as they cannot inspect
    // any state other than the syscall arguments.
    // But if we redirect all error handlers to user-space, then we can easily
    // make this decision.
    // The performance penalty for this extra round-trip to user-space is not
    // actually that bad, as we only ever pay it for denied system calls; and a
    // typical program has very few of these.
    return Trap(ReturnErrno, reinterpret_cast<void*>(err), true);
  }

  return ErrorCode(err);
}

ErrorCode PolicyCompiler::Trap(TrapRegistry::TrapFnc fnc,
                               const void* aux,
                               bool safe) {
  uint16_t trap_id = registry_->Add(fnc, aux, safe);
  return ErrorCode(trap_id, fnc, aux, safe);
}

bool PolicyCompiler::IsRequiredForUnsafeTrap(int sysno) {
  for (size_t i = 0; i < arraysize(kSyscallsRequiredForUnsafeTraps); ++i) {
    if (sysno == kSyscallsRequiredForUnsafeTraps[i]) {
      return true;
    }
  }
  return false;
}

ErrorCode PolicyCompiler::CondMaskedEqual(int argno,
                                          ErrorCode::ArgType width,
                                          uint64_t mask,
                                          uint64_t value,
                                          const ErrorCode& passed,
                                          const ErrorCode& failed) {
  return ErrorCode(argno,
                   width,
                   mask,
                   value,
                   &*conds_.insert(passed).first,
                   &*conds_.insert(failed).first);
}

}  // namespace bpf_dsl
}  // namespace sandbox
