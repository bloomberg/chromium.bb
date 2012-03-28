// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/daemon_controller.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/values.h"

namespace remoting {

namespace {

class DaemonControllerWin : public remoting::DaemonController {
 public:
  DaemonControllerWin();

  virtual State GetState() OVERRIDE;
  virtual void GetConfig(const GetConfigCallback& callback) OVERRIDE;
  virtual void SetConfigAndStart(
      scoped_ptr<base::DictionaryValue> config) OVERRIDE;
  virtual void SetPin(const std::string& pin) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DaemonControllerWin);
};

DaemonControllerWin::DaemonControllerWin() {
}

DaemonController::State DaemonControllerWin::GetState() {
  return DaemonController::STATE_NOT_IMPLEMENTED;
}

void DaemonControllerWin::GetConfig(const GetConfigCallback& callback) {
  NOTIMPLEMENTED();
}

void DaemonControllerWin::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config) {
  NOTIMPLEMENTED();
}

void DaemonControllerWin::SetPin(const std::string& pin) {
  NOTIMPLEMENTED();
}

void DaemonControllerWin::Stop() {
  NOTIMPLEMENTED();
}

}  // namespace

DaemonController* remoting::DaemonController::Create() {
  return new DaemonControllerWin();
}

}  // namespace remoting
