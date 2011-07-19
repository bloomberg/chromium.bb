// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_SSL_ADAPTER_H_
#define REMOTING_JINGLE_GLUE_SSL_ADAPTER_H_

namespace talk_base {
class AsyncSocket;
class SSLAdapter;
}  // namespace talk_base

namespace remoting {

// Wraps the given socket in a platform-dependent SSLAdapter
// implementation.
talk_base::SSLAdapter* CreateSSLAdapter(talk_base::AsyncSocket* socket);

// Utility template class that overrides CreateSSLAdapter() to use the
// above function.
template <class SocketFactory>
class SSLAdapterSocketFactory : public SocketFactory {
 public:
  virtual talk_base::SSLAdapter* CreateSSLAdapter(
      talk_base::AsyncSocket* socket) {
    return ::remoting::CreateSSLAdapter(socket);
  }
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_SSL_ADAPTER_H_

