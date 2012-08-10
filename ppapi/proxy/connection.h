// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_CONNECTION_H_
#define PPAPI_PROXY_CONNECTION_H_

namespace IPC {
class Sender;
}

namespace ppapi {
namespace proxy {

// This struct holds the channels that a resource uses to send message to the
// browser and renderer.
struct Connection {
  Connection() : browser_sender(0), renderer_sender(0) {
  }
  Connection(IPC::Sender* browser, IPC::Sender* renderer)
      : browser_sender(browser),
        renderer_sender(renderer) {
  }

  IPC::Sender* browser_sender;
  IPC::Sender* renderer_sender;
};

}  // namespace proxy
}  // namespace ppapi


#endif  // PPAPI_PROXY_CONNECTION_H_

