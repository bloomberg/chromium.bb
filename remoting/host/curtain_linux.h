// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_LINUX_H_
#define REMOTING_HOST_CURTAIN_LINUX_H_

#include "remoting/host/curtain.h"
#include "base/compiler_specific.h"

namespace remoting {

class CurtainLinux : public Curtain {
 public:
  virtual void EnableCurtainMode(bool enable) OVERRIDE;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CURTAIN_LINUX_H_
