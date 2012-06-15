// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#include "sandbox/linux/seccomp-bpf/verifier.h"


namespace playground2 {

bool Verifier::verifyBPF(const std::vector<struct sock_filter>& program,
                         const Sandbox::Evaluators& evaluators,
                         const char **err) {
  *err = NULL;
  if (evaluators.size() != 1) {
    *err = "Not implemented";
    return false;
  }
  Sandbox::EvaluateSyscall evaluateSyscall = evaluators.begin()->first;
  for (int nr = MIN_SYSCALL-1; nr <= static_cast<int>(MAX_SYSCALL)+1; ++nr) {
    // We ideally want to iterate over the full system call range and values
    // just above and just below this range. This gives us the full result set
    // of the "evaluators".
    // On Intel systems, this can fail in a surprising way, as a cleared bit 30
    // indicates either i386 or x86-64; and a set bit 30 indicates x32. And
    // unless we pay attention to setting this bit correctly, an early check in
    // our BPF program will make us fail with a misleading error code.
#if defined(__i386__) || defined(__x86_64__)
#if defined(__x86_64__) && defined(__ILP32__)
    int sysnum = nr |  0x40000000;
#else
    int sysnum = nr & ~0x40000000;
#endif
#else
    int sysnum = nr;
#endif

    struct arch_seccomp_data data = { sysnum, SECCOMP_ARCH };
    Sandbox::ErrorCode code = evaluateSyscall(sysnum);
    uint32_t computedRet = evaluateBPF(program, data, err);
    if (*err) {
      return false;
    } else if (computedRet != code) {
      *err = "Exit code from BPF program doesn't match";
      return false;
    }
  }
  return true;
}

uint32_t Verifier::evaluateBPF(const std::vector<struct sock_filter>& program,
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
      ld(&state, insn, err);
      break;
    case BPF_JMP:
      jmp(&state, insn, err);
      break;
    case BPF_RET:
      return ret(&state, insn, err);
    default:
      *err = "Unexpected instruction in BPF program";
      break;
    }
  }
  return 0;
}

void Verifier::ld(State *state, const struct sock_filter& insn,
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
  state->accIsValid = true;
  return;
}

void Verifier::jmp(State *state, const struct sock_filter& insn,
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
        !state->accIsValid ||
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

uint32_t Verifier::ret(State *, const struct sock_filter& insn,
                       const char **err) {
  if (BPF_SRC(insn.code) != BPF_K) {
    *err = "Invalid BPF_RET instruction";
    return 0;
  }
  return insn.k;
}

}  // namespace
