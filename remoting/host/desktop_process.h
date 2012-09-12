// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_PROCESS_H_
#define REMOTING_HOST_DESKTOP_PROCESS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace remoting {

class DesktopProcess {
 public:
  DesktopProcess();
  ~DesktopProcess();

  // Runs the desktop process.
  int Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_PROCESS_H_
