// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_ERRORCODE_H__
#define SANDBOX_LINUX_SECCOMP_BPF_ERRORCODE_H__

namespace playground2 {

struct arch_seccomp_data;

// This class holds all the possible values that can returned by a sandbox
// policy.
// We can either wrap a symbolic ErrorCode (i.e. ERR_XXX enum values), an
// errno value (in the range 1..4095), a pointer to a TrapFnc callback
// handling a SECCOMP_RET_TRAP trap, or a complex constraint.
// All of the commonly used values are stored in the "err_" field. So, code
// that is using the ErrorCode class typically operates on a single 32bit
// field.
class ErrorCode {
 public:
  enum {
    // Allow this system call.
    ERR_ALLOWED   = 0x0000,

    // Deny the system call with a particular "errno" value.
    ERR_MIN_ERRNO = 1,
    ERR_MAX_ERRNO = 4095,

    // This code should never be used directly, it is used internally only.
    ERR_INVALID   = -1,
  };

  // TrapFnc is a pointer to a function that handles Seccomp traps in
  // user-space. The seccomp policy can request that a trap handler gets
  // installed; it does so by returning a suitable ErrorCode() from the
  // syscallEvaluator. See the ErrorCode() constructor for how to pass in
  // the function pointer.
  // Please note that TrapFnc is executed from signal context and must be
  // async-signal safe:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
  typedef intptr_t (*TrapFnc)(const struct arch_seccomp_data& args, void *aux);

  enum ArgType {
    TP_32BIT, TP_64BIT,
  };

  enum Operation {
    OP_EQUAL, OP_GREATER, OP_GREATER_EQUAL, OP_HAS_BITS,
    OP_NUM_OPS,
  };

  // We allow the default constructor, as it makes the ErrorCode class
  // much easier to use. But if we ever encounter an invalid ErrorCode
  // when compiling a BPF filter, we deliberately generate an invalid
  // program that will get flagged both by our Verifier class and by
  // the Linux kernel.
  ErrorCode() :
      error_type_(ET_INVALID),
      err_(SECCOMP_RET_INVALID) {
  }
  explicit ErrorCode(int err);

  // For all practical purposes, ErrorCodes are treated as if they were
  // structs. The copy constructor and assignment operator are trivial and
  // we do not need to explicitly specify them.
  // Most notably, it is in fact perfectly OK to directly copy the passed_ and
  // failed_ field. They only ever get set by our private constructor, and the
  // callers handle life-cycle management for these objects.

  // Destructor
  ~ErrorCode() { }

  bool Equals(const ErrorCode& err) const;
  bool LessThan(const ErrorCode& err) const;

  uint32_t err() const { return err_; }

  struct LessThan {
    bool operator()(const ErrorCode& a, const ErrorCode& b) const {
      return a.LessThan(b);
    }
  };

 private:
  friend class CodeGen;
  friend class Sandbox;
  friend class Verifier;

  enum ErrorType {
    ET_INVALID, ET_SIMPLE, ET_TRAP, ET_COND,
  };

  // If we are wrapping a callback, we must assign a unique id. This id is
  // how the kernel tells us which one of our different SECCOMP_RET_TRAP
  // cases has been triggered.
  ErrorCode(TrapFnc fnc, const void *aux, bool safe, uint16_t id);

  // Some system calls require inspection of arguments. This constructor
  // allows us to specify additional constraints.
  ErrorCode(int argno, ArgType width, Operation op, uint64_t value,
            const ErrorCode *passed, const ErrorCode *failed);

  ErrorType error_type_;

  union {
    // Fields needed for SECCOMP_RET_TRAP callbacks
    struct {
      TrapFnc fnc_;              // Callback function and arg, if trap was
      void    *aux_;             //   triggered by the kernel's BPF filter.
      bool    safe_;             // Keep sandbox active while calling fnc_()
    };

    // Fields needed when inspecting additional arguments.
    struct {
      uint64_t  value_;          // Value that we are comparing with.
      int       argno_;          // Syscall arg number that we are inspecting.
      ArgType   width_;          // Whether we are looking at a 32/64bit value.
      Operation op_;             // Comparison operation.
      const ErrorCode *passed_;  // Value to be returned if comparison passed,
      const ErrorCode *failed_;  //   or if it failed.
    };
  };

  // 32bit field used for all possible types of ErrorCode values. This is
  // the value that uniquely identifies any ErrorCode and it (typically) can
  // be emitted directly into a BPF filter program.
  uint32_t err_;

};

}  // namespace

#endif  // SANDBOX_LINUX_SECCOMP_BPF_ERRORCODE_H__
