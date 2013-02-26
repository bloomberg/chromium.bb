// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Commonly used macro definitions to make the code build in different
// target environments (e.g. as part of Chrome vs. stand-alone)

#ifndef SANDBOX_LINUX_SECCOMP_BPF_PORT_H__
#define SANDBOX_LINUX_SECCOMP_BPF_PORT_H__

#if !defined(SECCOMP_BPF_STANDALONE)
  #include "base/basictypes.h"
  #include "base/logging.h"
  #include "base/posix/eintr_wrapper.h"
#else
  #define arraysize(x) (sizeof(x)/sizeof(*(x)))

  #define HANDLE_EINTR TEMP_FAILURE_RETRY

  #define DISALLOW_COPY_AND_ASSIGN(TypeName)       \
    TypeName(const TypeName&);                     \
    void operator=(const TypeName&)

  #define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
    TypeName();                                    \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

  template <bool>
  struct CompileAssert {
  };

  #define COMPILE_ASSERT(expr, msg) \
    typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]
#endif

#endif  // SANDBOX_LINUX_SECCOMP_BPF_PORT_H__
