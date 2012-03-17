// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/scoped_thread_desktop_win.h"

#include "base/logging.h"

#include "remoting/host/desktop_win.h"

namespace remoting {

ScopedThreadDesktopWin::ScopedThreadDesktopWin()
    : initial_(DesktopWin::GetThreadDesktop()) {
}

ScopedThreadDesktopWin::~ScopedThreadDesktopWin() {
  Revert();
}

bool ScopedThreadDesktopWin::IsSame(const DesktopWin& desktop) {
  if (assigned_.get() != NULL) {
    return assigned_->IsSame(desktop);
  } else {
    return initial_->IsSame(desktop);
  }
}

void ScopedThreadDesktopWin::Revert() {
  if (assigned_.get() != NULL) {
    initial_->SetThreadDesktop();
    assigned_.reset();
  }
}

bool ScopedThreadDesktopWin::SetThreadDesktop(scoped_ptr<DesktopWin> desktop) {
  Revert();

  if (initial_->IsSame(*desktop))
    return false;

  if (!desktop->SetThreadDesktop())
    return false;

  assigned_ = desktop.Pass();
  return true;
}

}  // namespace remoting
