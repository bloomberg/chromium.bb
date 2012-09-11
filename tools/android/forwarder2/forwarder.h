// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_FORWARDER_H_
#define TOOLS_ANDROID_FORWARDER2_FORWARDER_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "tools/android/forwarder2/socket.h"
#include "tools/android/forwarder2/thread.h"

namespace forwarder2 {

class Forwarder : public Thread {
 public:
  Forwarder(scoped_ptr<Socket> socket1, scoped_ptr<Socket> socket2);
  virtual ~Forwarder();

  // This object self deletes after running, so one cannot join.
  // Thread:
  virtual void Join() OVERRIDE;

 protected:
  // Thread:
  // This object self deletes after running.
  virtual void Run() OVERRIDE;

 private:
  scoped_ptr<Socket> socket1_;
  scoped_ptr<Socket> socket2_;

  DISALLOW_COPY_AND_ASSIGN(Forwarder);
};

}  // namespace forwarder

#endif  // TOOLS_ANDROID_FORWARDER2_FORWARDER_H_
