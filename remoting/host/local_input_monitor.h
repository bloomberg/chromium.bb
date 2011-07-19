// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_LOCAL_INPUT_MONITOR_H_
#define REMOTING_LOCAL_INPUT_MONITOR_H_

namespace remoting {

class ChromotingHost;

class LocalInputMonitor {
 public:
  virtual ~LocalInputMonitor() {}

  virtual void Start(ChromotingHost* host) = 0;
  virtual void Stop() = 0;

  static LocalInputMonitor* Create();
};

}  // namespace remoting

#endif
