// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/continue_window.h"

namespace remoting {

namespace {

class ContinueWindowAndroid : public ContinueWindow {
 public:
  ContinueWindowAndroid() {}

 protected:
  // ContinueWindow overrides.
  void ShowUi() override {
    NOTIMPLEMENTED();
  }

  void HideUi() override {
    NOTIMPLEMENTED();
  }

  DISALLOW_COPY_AND_ASSIGN(ContinueWindowAndroid);
};

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateContinueWindow() {
  return base::WrapUnique(new ContinueWindowAndroid());
}

}  // namespace remoting
