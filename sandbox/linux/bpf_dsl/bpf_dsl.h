// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_
#define SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_

#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "sandbox/linux/bpf_dsl/cons.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf_policy.h"
#include "sandbox/linux/seccomp-bpf/trap.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
class ErrorCode;
class SandboxBPF;
}

// The sandbox::bpf_dsl namespace provides a domain-specific language
// to make writing BPF policies more expressive.  In general, the
// object types all have value semantics (i.e., they can be copied
// around, returned from or passed to function calls, etc. without any
// surprising side effects), though not all support assignment.
//
// An idiomatic and demonstrative (albeit silly) example of this API
// would be:
//
//      #include "sandbox/linux/bpf_dsl/bpf_dsl.h"
//
//      using namespace sandbox::bpf_dsl;
//
//      class SillyPolicy : public SandboxBPFDSLPolicy {
//       public:
//        SillyPolicy() {}
//        virtual ~SillyPolicy() {}
//        virtual ResultExpr EvaluateSyscall(int sysno) const OVERRIDE {
//          if (sysno == __NR_fcntl) {
//            Arg<int> fd(0), cmd(1);
//            Arg<unsigned long> flags(2);
//            const unsigned long kBadFlags = ~(O_ACCMODE | O_NONBLOCK);
//            return If(fd == 0 && cmd == F_SETFL && (flags & kBadFlags) == 0,
//                      Allow())
//                .ElseIf(cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC,
//                        Error(EMFILE))
//                .Else(Trap(SetFlagHandler, NULL));
//          } else {
//            return Allow();
//          }
//        }
//
//       private:
//        DISALLOW_COPY_AND_ASSIGN(SillyPolicy);
//      };
//
// More generally, the DSL currently supports the following grammar:
//
//   result = Allow() | Error(errno) | Trap(trap_func, arg)
//          | If(bool, result)[.ElseIf(bool, result)].Else(result)
//   bool   = arg == val | (arg & mask) == mask | (arg & mask) == 0
//          | !bool | bool && bool | bool || bool
//
// The semantics of each function and operator are intended to be
// intuitive, but are described in more detail below.
//
// (Credit to Sean Parent's "Inheritance is the Base Class of Evil"
// talk at Going Native 2013 for promoting value semantics via shared
// pointers to immutable state.)

namespace sandbox {
namespace bpf_dsl {

// Forward declarations of classes; see below for proper documentation.
class Elser;
namespace internal {
class ResultExprImpl;
class BoolExprImpl;
}

// ResultExpr is an opaque reference to an immutable result expression tree.
typedef scoped_refptr<const internal::ResultExprImpl> ResultExpr;

// BoolExpr is an opaque reference to an immutable boolean expression tree.
typedef scoped_refptr<const internal::BoolExprImpl> BoolExpr;

// Helper class to make writing policies easier.
class SANDBOX_EXPORT SandboxBPFDSLPolicy : public SandboxBPFPolicy {
 public:
  SandboxBPFDSLPolicy() : SandboxBPFPolicy() {}
  virtual ~SandboxBPFDSLPolicy() {}

  // User extension point for writing custom sandbox policies.
  virtual ResultExpr EvaluateSyscall(int sysno) const = 0;

  // Optional overload for specifying alternate behavior for invalid
  // system calls.  The default is to return ENOSYS.
  virtual ResultExpr InvalidSyscall() const;

  // Override implementations from SandboxBPFPolicy.  Marked as FINAL
  // to prevent mixups with child classes accidentally overloading
  // these instead of the above methods.
  virtual ErrorCode EvaluateSyscall(SandboxBPF* sb,
                                    int sysno) const OVERRIDE FINAL;
  virtual ErrorCode InvalidSyscall(SandboxBPF* sb) const OVERRIDE FINAL;

  // Helper method so policies can just write Trap(func, aux).
  static ResultExpr Trap(::sandbox::Trap::TrapFnc trap_func, void* aux);

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxBPFDSLPolicy);
};

// Allow specifies a result that the system call should be allowed to
// execute normally.
SANDBOX_EXPORT ResultExpr Allow();

// Error specifies a result that the system call should fail with
// error number |err|.  As a special case, Error(0) will result in the
// system call appearing to have succeeded, but without having any
// side effects.
SANDBOX_EXPORT ResultExpr Error(int err);

// Trap specifies a result that the system call should be handled by
// trapping back into userspace and invoking |trap_func|, passing
// |aux| as the second parameter.
SANDBOX_EXPORT ResultExpr Trap(::sandbox::Trap::TrapFnc trap_func, void* aux);

template <typename T>
class SANDBOX_EXPORT Arg {
 public:
  // Initializes the Arg to represent the |num|th system call
  // argument (indexed from 0), which is of type |T|.
  explicit Arg(int num) : num_(num), mask_(-1) {}

  Arg(const Arg& arg) : num_(arg.num_), mask_(arg.mask_) {}

  // Returns an Arg representing the current argument, but after
  // bitwise-and'ing it with |rhs|.
  Arg operator&(uint64_t rhs) const { return Arg(num_, mask_ & rhs); }

  // Returns a boolean expression comparing whether the system call
  // argument (after applying any bitmasks, if appropriate) equals |rhs|.
  BoolExpr operator==(T rhs) const;

 private:
  Arg(int num, uint64_t mask) : num_(num), mask_(mask) {}
  int num_;
  uint64_t mask_;
  DISALLOW_ASSIGN(Arg);
};

// Various ways to combine boolean expressions into more complex expressions.
// They follow standard boolean algebra laws.
SANDBOX_EXPORT BoolExpr operator!(BoolExpr cond);
SANDBOX_EXPORT BoolExpr operator&&(BoolExpr lhs, BoolExpr rhs);
SANDBOX_EXPORT BoolExpr operator||(BoolExpr lhs, BoolExpr rhs);

// If begins a conditional result expression predicated on the
// specified boolean expression.
SANDBOX_EXPORT Elser If(BoolExpr cond, ResultExpr then_result);

class SANDBOX_EXPORT Elser {
 public:
  Elser(const Elser& elser);
  ~Elser();

  // ElseIf extends the conditional result expression with another
  // "if then" clause, predicated on the specified boolean expression.
  Elser ElseIf(BoolExpr cond, ResultExpr then_result) const;

  // Else terminates a conditional result expression using |else_result| as
  // the default fallback result expression.
  ResultExpr Else(ResultExpr else_result) const;

 private:
  typedef std::pair<BoolExpr, ResultExpr> Clause;
  explicit Elser(Cons<Clause>::List clause_list);
  Cons<Clause>::List clause_list_;
  friend Elser If(BoolExpr, ResultExpr);
  DISALLOW_ASSIGN(Elser);
};

// =====================================================================
// Official API ends here.
// =====================================================================

// Definitions below are necessary here only for C++03 compatibility.
// Once C++11 is available, they should be moved into bpf_dsl.cc via extern
// templates.
namespace internal {

// Returns a boolean expression that represents whether system call
// argument |num| of size |size| is equal to |val|, when masked
// according to |mask|.  Users should use the Arg template class below
// instead of using this API directly.
SANDBOX_EXPORT BoolExpr
    ArgEq(int num, size_t size, uint64_t mask, uint64_t val);

// Internal interface implemented by BoolExpr implementations.
class SANDBOX_EXPORT BoolExprImpl : public base::RefCounted<BoolExprImpl> {
 public:
  BoolExprImpl() {}
  virtual ErrorCode Compile(SandboxBPF* sb,
                            ErrorCode true_ec,
                            ErrorCode false_ec) const = 0;

 protected:
  virtual ~BoolExprImpl() {}

 private:
  friend class base::RefCounted<BoolExprImpl>;
  DISALLOW_COPY_AND_ASSIGN(BoolExprImpl);
};

// Internal interface implemented by ResultExpr implementations.
class SANDBOX_EXPORT ResultExprImpl : public base::RefCounted<ResultExprImpl> {
 public:
  ResultExprImpl() {}
  virtual ErrorCode Compile(SandboxBPF* sb) const = 0;

 protected:
  virtual ~ResultExprImpl() {}

 private:
  friend class base::RefCounted<ResultExprImpl>;
  DISALLOW_COPY_AND_ASSIGN(ResultExprImpl);
};

}  // namespace internal

// Definition requires ArgEq to have been declared.  Moved out-of-line
// to minimize how much internal clutter users have to ignore while
// reading the header documentation.
template <typename T>
BoolExpr Arg<T>::operator==(T rhs) const {
  return internal::ArgEq(num_, sizeof(T), mask_, static_cast<uint64_t>(rhs));
}

}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_BPF_DSL_H_
