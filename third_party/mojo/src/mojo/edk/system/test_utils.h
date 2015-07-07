// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_TEST_UTILS_H_
#define MOJO_EDK_SYSTEM_TEST_UTILS_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace system {
namespace test {

// A timeout smaller than |TestTimeouts::tiny_timeout()|. Warning: This may lead
// to flakiness, but this is unavoidable if, e.g., you're trying to ensure that
// functions with timeouts are reasonably accurate. We want this to be as small
// as possible without causing too much flakiness.
base::TimeDelta EpsilonTimeout();

// |TestTimeouts::tiny_timeout()|, as a |MojoDeadline|. (Expect this to be on
// the order of 100 ms.)
MojoDeadline TinyDeadline();

// |TestTimeouts::action_timeout()|, as a |MojoDeadline|.(Expect this to be on
// the order of 10 s.)
MojoDeadline ActionDeadline();

// Stopwatch -------------------------------------------------------------------

// A simple "stopwatch" for measuring time elapsed from a given starting point.
class Stopwatch {
 public:
  Stopwatch() {}
  ~Stopwatch() {}

  void Start() { start_time_ = base::TimeTicks::Now(); }

  base::TimeDelta Elapsed() { return base::TimeTicks::Now() - start_time_; }

 private:
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(Stopwatch);
};

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_TEST_UTILS_H_
