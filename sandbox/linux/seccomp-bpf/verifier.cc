// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/syscall_iterator.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"


namespace playground2 {

bool Verifier::VerifyBPF(const std::vector<struct sock_filter>& program,
                         const Sandbox::Evaluators& evaluators,
                         const char **err) {
  *err = NULL;
  if (evaluators.size() != 1) {
    *err = "Not implemented";
    return false;
  }
  Sandbox::EvaluateSyscall evaluate_syscall = evaluators.begin()->first;
  void *aux                                 = evaluators.begin()->second;
  for (SyscallIterator iter(false); !iter.Done(); ) {
    uint32_t sysnum = iter.Next();
    // We ideally want to iterate over the full system call range and values
    // just above and just below this range. This gives us the full result set
    // of the "evaluators".
    // On Intel systems, this can fail in a surprising way, as a cleared bit 30
    // indicates either i386 or x86-64; and a set bit 30 indicates x32. And
    // unless we pay attention to setting this bit correctly, an early check in
    // our BPF program will make us fail with a misleading error code.
    struct arch_seccomp_data data = { static_cast<int>(sysnum),
                                      static_cast<uint32_t>(SECCOMP_ARCH) };
#if defined(__i386__) || defined(__x86_64__)
#if defined(__x86_64__) && defined(__ILP32__)
    if (!(sysnum & 0x40000000u)) {
      continue;
    }
#else
    if (sysnum & 0x40000000u) {
      continue;
    }
#endif
#endif
    ErrorCode code = evaluate_syscall(sysnum, aux);
    if (!VerifyErrorCode(program, &data, code, err)) {
      return false;
    }
  }
  return true;
}

bool Verifier::VerifyErrorCode(const std::vector<struct sock_filter>& program,
                               struct arch_seccomp_data *data,
                               const ErrorCode& code, const char **err) {
  if (code.error_type_ == ErrorCode::ET_SIMPLE ||
      code.error_type_ == ErrorCode::ET_TRAP) {
    uint32_t computed_ret = EvaluateBPF(program, *data, err);
    if (*err) {
      return false;
    } else if (computed_ret != code.err()) {
      *err = "Exit code from BPF program doesn't match";
      return false;
    }
  } else if (code.error_type_ == ErrorCode::ET_COND) {
    if (code.argno_ < 0 || code.argno_ >= 6) {
      *err = "Invalid argument number in error code";
      return false;
    }
    switch (code.op_) {
    case ErrorCode::OP_EQUAL:
      // Verify that we can check a 32bit value (or the LSB of a 64bit value)
      // for equality.
      data->args[code.argno_] = code.value_;
      if (!VerifyErrorCode(program, data, *code.passed_, err)) {
        return false;
      }

      // Change the value to no longer match and verify that this is detected
      // as an inequality.
      data->args[code.argno_] = code.value_ ^ 0x55AA55AA;
      if (!VerifyErrorCode(program, data, *code.failed_, err)) {
        return false;
      }

      // BPF programs can only ever operate on 32bit values. So, we have
      // generated additional BPF instructions that inspect the MSB. Verify
      // that they behave as intended.
      if (code.width_ == ErrorCode::TP_32BIT) {
        if (code.value_ >> 32) {
          SANDBOX_DIE("Invalid comparison of a 32bit system call argument "
                      "against a 64bit constant; this test is always false.");
        }

        // If the system call argument was intended to be a 32bit parameter,
        // verify that it is a fatal error if a 64bit value is ever passed
        // here.
        data->args[code.argno_] = 0x100000000ull;
        if (!VerifyErrorCode(program, data, Sandbox::Unexpected64bitArgument(),
                           err)) {
          return false;
        }
      } else {
        // If the system call argument was intended to be a 64bit parameter,
        // verify that we can handle (in-)equality for the MSB. This is
        // essentially the same test that we did earlier for the LSB.
        // We only need to verify the behavior of the inequality test. We
        // know that the equality test already passed, as unlike the kernel
        // the Verifier does operate on 64bit quantities.
        data->args[code.argno_] = code.value_ ^ 0x55AA55AA00000000ull;
        if (!VerifyErrorCode(program, data, *code.failed_, err)) {
          return false;
        }
      }
      break;
    default: // TODO(markus): We can only check for equality so far.
      *err = "Unsupported operation in conditional error code";
      return false;
    }
  } else {
    *err = "Attempting to return invalid error code from BPF program";
    return false;
  }
  return true;
}

uint32_t Verifier::EvaluateBPF(const std::vector<struct sock_filter>& program,
                               const struct arch_seccomp_data& data,
                               const char **err) {
  *err = NULL;
  if (program.size() < 1 || program.size() >= SECCOMP_MAX_PROGRAM_SIZE) {
    *err = "Invalid program length";
    return 0;
  }
  for (State state(program, data); !*err; ++state.ip) {
    if (state.ip >= program.size()) {
      *err = "Invalid instruction pointer in BPF program";
      break;
    }
    const struct sock_filter& insn = program[state.ip];
    switch (BPF_CLASS(insn.code)) {
    case BPF_LD:
      Ld(&state, insn, err);
      break;
    case BPF_JMP:
      Jmp(&state, insn, err);
      break;
    case BPF_RET: {
      uint32_t r = Ret(&state, insn, err);
      switch (r & SECCOMP_RET_ACTION) {
      case SECCOMP_RET_TRAP:
      case SECCOMP_RET_ERRNO:
      case SECCOMP_RET_ALLOW:
        break;
      case SECCOMP_RET_KILL:     // We don't ever generate this
      case SECCOMP_RET_TRACE:    // We don't ever generate this
      case SECCOMP_RET_INVALID:  // Should never show up in BPF program
      default:
        *err = "Unexpected return code found in BPF program";
        return 0;
      }
      return r; }
    default:
      *err = "Unexpected instruction in BPF program";
      break;
    }
  }
  return 0;
}

void Verifier::Ld(State *state, const struct sock_filter& insn,
                  const char **err) {
  if (BPF_SIZE(insn.code) != BPF_W ||
      BPF_MODE(insn.code) != BPF_ABS) {
    *err = "Invalid BPF_LD instruction";
    return;
  }
  if (insn.k < sizeof(struct arch_seccomp_data) && (insn.k & 3) == 0) {
    // We only allow loading of properly aligned 32bit quantities.
    memcpy(&state->accumulator,
           reinterpret_cast<const char *>(&state->data) + insn.k,
           4);
  } else {
    *err = "Invalid operand in BPF_LD instruction";
    return;
  }
  state->acc_is_valid = true;
  return;
}

void Verifier::Jmp(State *state, const struct sock_filter& insn,
                   const char **err) {
  if (BPF_OP(insn.code) == BPF_JA) {
    if (state->ip + insn.k + 1 >= state->program.size() ||
        state->ip + insn.k + 1 <= state->ip) {
    compilation_failure:
      *err = "Invalid BPF_JMP instruction";
      return;
    }
    state->ip += insn.k;
  } else {
    if (BPF_SRC(insn.code) != BPF_K ||
        !state->acc_is_valid ||
        state->ip + insn.jt + 1 >= state->program.size() ||
        state->ip + insn.jf + 1 >= state->program.size()) {
      goto compilation_failure;
    }
    switch (BPF_OP(insn.code)) {
    case BPF_JEQ:
      if (state->accumulator == insn.k) {
        state->ip += insn.jt;
      } else {
        state->ip += insn.jf;
      }
      break;
    case BPF_JGT:
      if (state->accumulator > insn.k) {
        state->ip += insn.jt;
      } else {
        state->ip += insn.jf;
      }
      break;
    case BPF_JGE:
      if (state->accumulator >= insn.k) {
        state->ip += insn.jt;
      } else {
        state->ip += insn.jf;
      }
      break;
    case BPF_JSET:
      if (state->accumulator & insn.k) {
        state->ip += insn.jt;
      } else {
        state->ip += insn.jf;
      }
      break;
    default:
      goto compilation_failure;
    }
  }
}

uint32_t Verifier::Ret(State *, const struct sock_filter& insn,
                       const char **err) {
  if (BPF_SRC(insn.code) != BPF_K) {
    *err = "Invalid BPF_RET instruction";
    return 0;
  }
  return insn.k;
}

}  // namespace
