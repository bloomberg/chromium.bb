// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_HOST_RESOLVER_H_
#define REMOTING_JINGLE_GLUE_HOST_RESOLVER_H_

#include <string>

#include "base/basictypes.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace remoting {

// TODO(sergeyu): Move HostResolver and HostResolverFactory to
// libjingle and use them in StunPort.

class HostResolver {
 public:
  HostResolver();
  virtual ~HostResolver();

  virtual void Resolve(const talk_base::SocketAddress& address) = 0;

  sigslot::signal2<HostResolver*, const talk_base::SocketAddress&> SignalDone;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

class HostResolverFactory {
 public:
  HostResolverFactory() { }
  virtual ~HostResolverFactory() { }

  virtual HostResolver* CreateHostResolver() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResolverFactory);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_HOST_RESOLVER_H_
