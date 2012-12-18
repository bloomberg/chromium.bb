// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_WIN_SCOPED_THREAD_DESKTOP_H_
#define REMOTING_CAPTURER_WIN_SCOPED_THREAD_DESKTOP_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class Desktop;

class ScopedThreadDesktop {
 public:
  ScopedThreadDesktop();
  ~ScopedThreadDesktop();

  // Returns true if |desktop| has the same desktop name as the currently
  // assigned desktop (if assigned) or as the initial desktop (if not assigned).
  // Returns false in any other case including failing Win32 APIs and
  // uninitialized desktop handles.
  bool IsSame(const Desktop& desktop);

  // Reverts the calling thread to use the initial desktop.
  void Revert();

  // Assigns |desktop| to be the calling thread. Returns true if the thread has
  // been switched to |desktop| successfully.
  bool SetThreadDesktop(scoped_ptr<Desktop> desktop);

 private:
  // The desktop handle assigned to the calling thread by Set
  scoped_ptr<Desktop> assigned_;

  // The desktop handle assigned to the calling thread at creation.
  scoped_ptr<Desktop> initial_;

  DISALLOW_COPY_AND_ASSIGN(ScopedThreadDesktop);
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_WIN_SCOPED_THREAD_DESKTOP_H_
