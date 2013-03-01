// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_RDP_CLIENT_H_
#define REMOTING_HOST_WIN_RDP_CLIENT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "third_party/skia/include/core/SkSize.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace

namespace net {
class IPEndPoint;
}  // namespace

namespace remoting {

// Establishes a loopback RDP connection to spawn a new Windows session.
class RdpClient : public base::NonThreadSafe {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Notifies the event handler that an RDP connection has been established
    // successfully.
    virtual void OnRdpConnected(const net::IPEndPoint& client_endpoint) = 0;

    // Notifies that the RDP connection has been closed.
    virtual void OnRdpClosed() = 0;
  };

  RdpClient(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const SkISize& screen_size,
      EventHandler* event_handler);
  virtual ~RdpClient();

 private:
  // The actual implementation resides in Core class.
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(RdpClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_RDP_CLIENT_H_
