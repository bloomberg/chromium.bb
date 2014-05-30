// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EXTENSION_H_
#define REMOTING_HOST_HOST_EXTENSION_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace remoting {

class ClientSession;
class HostExtensionSession;

// Extends |ChromotingHost| with new functionality, and can use extension
// messages to communicate with the client.
class HostExtension {
 public:
  virtual ~HostExtension() {}

  // Returns a space-separated list of capabilities provided by this extension.
  // Capabilities may be used to inform the client of the availability of an
  // extension. They are merged into the capabilities the host reports to the
  // client.
  virtual std::string GetCapabilities() = 0;

  // Creates an extension session, which can handle extension messages for a
  // client session.
  // NULL may be returned if |client_session| cannot support this
  // extension.
  // |client_session| must outlive the resulting |HostExtensionSession|.
  virtual scoped_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSession* client_session) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EXTENSION_H_
