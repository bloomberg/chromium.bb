// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/ssl_adapter.h"

#if defined(OS_WIN)
#include "third_party/libjingle/source/talk/base/ssladapter.h"
#else
#include "remoting/jingle_glue/ssl_socket_adapter.h"
#endif

namespace remoting {

talk_base::SSLAdapter* CreateSSLAdapter(talk_base::AsyncSocket* socket) {
  talk_base::SSLAdapter* ssl_adapter =
      remoting::SSLSocketAdapter::Create(socket);
  DCHECK(ssl_adapter);
  return ssl_adapter;
}

}  // namespace remoting
