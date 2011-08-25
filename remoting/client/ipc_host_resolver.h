// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IPC_HOST_RESOLVER_H_
#define REMOTING_CLIENT_IPC_HOST_ADDRESS_RESOLVER_H_

#include "base/compiler_specific.h"
#include "remoting/jingle_glue/host_resolver.h"

namespace content {
class P2PSocketDispatcher;
}  // namespace content

namespace remoting {

// Implementation of HostResolverFactory interface that works in
// renderer.
//
// TODO(sergeyu): Move this class to content/renderer/p2p after
// HostResolver interface is moved to libjingle.
class IpcHostResolverFactory : public HostResolverFactory {
 public:
  IpcHostResolverFactory(content::P2PSocketDispatcher* socket_dispatcher);
  virtual ~IpcHostResolverFactory();

  virtual HostResolver* CreateHostResolver() OVERRIDE;

 private:
  content::P2PSocketDispatcher* socket_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(IpcHostResolverFactory);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_HOST_RESOLVER_H_
