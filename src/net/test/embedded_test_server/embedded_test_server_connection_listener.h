// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_CONNECTION_LISTENER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_CONNECTION_LISTENER_H_

#include <memory>

namespace net {

class StreamSocket;

namespace test_server {

// An interface for connection event notifications.
class EmbeddedTestServerConnectionListener {
 public:
  // Notified when a socket was accepted by the EmbeddedTestServer. The listener
  // can return |socket| or a wrapper to customize the socket behavior.
  virtual std::unique_ptr<StreamSocket> AcceptedSocket(
      std::unique_ptr<StreamSocket> socket) = 0;

  // Notified when a socket was read from by the EmbeddedTestServer.
  virtual void ReadFromSocket(const StreamSocket& socket, int rv) = 0;

  // Notified when the EmbeddedTestServer has completed a request and response
  // successfully on |socket|. The listener can take |socket| to manually handle
  // further traffic on it (for example, if doing a proxy tunnel), otherwise
  // |socket| is destroyed.
  virtual void OnResponseCompletedSuccessfully(
      std::unique_ptr<StreamSocket> socket) {}

 protected:
  EmbeddedTestServerConnectionListener() {}

  virtual ~EmbeddedTestServerConnectionListener() {}
};

}  // namespace test_server
}  // namespace net

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_EMBEDDED_TEST_SERVER_CONNECTION_LISTENER_H_
