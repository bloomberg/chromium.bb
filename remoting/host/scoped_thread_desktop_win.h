// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCOPED_THREAD_DESKTOP_WIN_H_
#define REMOTING_HOST_SCOPED_THREAD_DESKTOP_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class DesktopWin;

class ScopedThreadDesktopWin {
 public:
  ScopedThreadDesktopWin();
  ~ScopedThreadDesktopWin();

  // Returns true if |desktop| has the same desktop name as the currently
  // assigned desktop (if assigned) or as the initial desktop (if not assigned).
  // Returns false in any other case including failing Win32 APIs and
  // uninitialized desktop handles.
  bool IsSame(const DesktopWin& desktop);

  // Reverts the calling thread to use the initial desktop.
  void Revert();

  // Assigns |desktop| to be the calling thread. Returns true if the thread has
  // been switched to |desktop| successfully.
  bool SetThreadDesktop(scoped_ptr<DesktopWin> desktop);

 private:
  // The desktop handle assigned to the calling thread by Set
  scoped_ptr<DesktopWin> assigned_;

  // The desktop handle assigned to the calling thread at creation.
  scoped_ptr<DesktopWin> initial_;

  DISALLOW_COPY_AND_ASSIGN(ScopedThreadDesktopWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCOPED_THREAD_DESKTOP_WIN_H_
