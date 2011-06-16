// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace remoting {

class ContinueWindowLinux : public remoting::ContinueWindow {
 public:
  ContinueWindowLinux() {}
  virtual ~ContinueWindowLinux() {}

  virtual void Show(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContinueWindowLinux);
};

void ContinueWindowLinux::Show(remoting::ChromotingHost* host) {
  NOTIMPLEMENTED();
}

void ContinueWindowLinux::Hide() {
  NOTIMPLEMENTED();
}

ContinueWindow* ContinueWindow::Create() {
  return new ContinueWindowLinux();
}

}  // namespace remoting
