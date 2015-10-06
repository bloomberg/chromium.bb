// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_CHECKER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_CHECKER_H_

#include "mojo/public/cpp/system/macros.h"

#if !defined(_WIN32)
#include "mojo/public/cpp/bindings/lib/thread_checker_posix.h"
#endif

namespace mojo {
namespace internal {

class ThreadCheckerDoNothing {
 public:
  bool CalledOnValidThread() const MOJO_WARN_UNUSED_RESULT {
    return true;
  }
};

// ThreadChecker is a class used to verify that some methods of a class are
// called from the same thread. It is meant to be a member variable of a class.
// The entire lifecycle of a ThreadChecker must occur on a single thread.
// In Release mode (without dcheck_always_on), ThreadChecker does nothing.
#if !defined(_WIN32) && (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON))
using ThreadChecker = ThreadCheckerPosix;
#else
using ThreadChecker = ThreadCheckerDoNothing;
#endif

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_CHECKER_H_
