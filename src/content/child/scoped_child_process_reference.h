// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCOPED_CHILD_PROCESS_REFERENCE_H_
#define CONTENT_CHILD_SCOPED_CHILD_PROCESS_REFERENCE_H_

#include "base/macros.h"

namespace base {
class TimeDelta;
}

namespace content {

// Scoper class that automatically adds a reference to the current child
// process in constructor and releases the reference on scope out.
// Consumers of this class can call ReleaseWithDelay() to explicitly release
// the reference with a certain delay.
class ScopedChildProcessReference {
 public:
  ScopedChildProcessReference();
  ~ScopedChildProcessReference();

  // Releases the process reference after |delay|. Once this is called
  // scoping out has no effect.
  // It is not valid to call this more than once.
  void ReleaseWithDelay(const base::TimeDelta& delay);

 private:
  bool has_reference_;

  DISALLOW_COPY_AND_ASSIGN(ScopedChildProcessReference);
};

}  // namespace content

#endif  // CONTENT_CHILD_SCOPED_CHILD_PROCESS_REFERENCE_H_
