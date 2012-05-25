// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_LOCAL_INPUT_MONITOR_H_
#define REMOTING_LOCAL_INPUT_MONITOR_H_

#include "base/memory/scoped_ptr.h"

namespace remoting {

class ChromotingHost;

class LocalInputMonitor {
 public:
  virtual ~LocalInputMonitor() {}

  virtual void Start(ChromotingHost* host) = 0;
  virtual void Stop() = 0;

  // TODO(sergeyu): This is a short-term hack to disable disconnection
  // shortcut on Mac.
  virtual void DisableShortcutOnMac() {};

  static scoped_ptr<LocalInputMonitor> Create();
};

}  // namespace remoting

#endif
