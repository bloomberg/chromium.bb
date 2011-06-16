// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/continue_window.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace remoting {

class ContinueWindowMac : public remoting::ContinueWindow {
 public:
  ContinueWindowMac() {}
  virtual ~ContinueWindowMac() {}

  virtual void Show(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContinueWindowMac);
};

void ContinueWindowMac::Show(remoting::ChromotingHost* host) {
  NOTIMPLEMENTED();
}

void ContinueWindowMac::Hide() {
  NOTIMPLEMENTED();
}

ContinueWindow* ContinueWindow::Create() {
  return new ContinueWindowMac();
}

}  // namespace remoting
