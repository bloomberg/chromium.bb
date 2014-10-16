// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_BPF_DSL_IMPL_H_
#define SANDBOX_LINUX_BPF_DSL_BPF_DSL_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {
class ErrorCode;
class SandboxBPF;

namespace bpf_dsl {
namespace internal {

// Internal interface implemented by BoolExpr implementations.
class BoolExprImpl : public base::RefCounted<BoolExprImpl> {
 public:
  BoolExprImpl() {}
  virtual ErrorCode Compile(PolicyCompiler* pc,
                            ErrorCode true_ec,
                            ErrorCode false_ec) const = 0;

 protected:
  virtual ~BoolExprImpl() {}

 private:
  friend class base::RefCounted<BoolExprImpl>;
  DISALLOW_COPY_AND_ASSIGN(BoolExprImpl);
};

// Internal interface implemented by ResultExpr implementations.
class ResultExprImpl : public base::RefCounted<ResultExprImpl> {
 public:
  ResultExprImpl() {}
  virtual ErrorCode Compile(PolicyCompiler* pc) const = 0;
  virtual bool HasUnsafeTraps() const;

 protected:
  virtual ~ResultExprImpl() {}

 private:
  friend class base::RefCounted<ResultExprImpl>;
  DISALLOW_COPY_AND_ASSIGN(ResultExprImpl);
};

}  // namespace internal
}  // namespace bpf_dsl
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_BPF_DSL_IMPL_H_
