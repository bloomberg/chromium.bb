// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/curtain_mode.h"

namespace remoting {

namespace {

class CurtainModeAndroid : public CurtainMode {
 public:
  CurtainModeAndroid() {}

  // Overriden from CurtainMode.
  bool Activate() override {
    NOTIMPLEMENTED();
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CurtainModeAndroid);
};

}  // namespace

// static
std::unique_ptr<CurtainMode> CurtainMode::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return base::WrapUnique(new CurtainModeAndroid());
}

}  // namespace remoting
