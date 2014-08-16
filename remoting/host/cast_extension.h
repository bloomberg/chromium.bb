// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAST_EXTENSION_H_
#define REMOTING_HOST_CAST_EXTENSION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/host_extension.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace remoting {

namespace protocol {
struct NetworkSettings;
}  // namespace protocol

// CastExtension extends HostExtension to enable WebRTC support.
class CastExtension : public HostExtension {
 public:
  CastExtension(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const protocol::NetworkSettings& network_settings);
  virtual ~CastExtension();

  // HostExtension interface.
  virtual std::string capability() const OVERRIDE;
  virtual scoped_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub) OVERRIDE;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  const protocol::NetworkSettings& network_settings_;

  DISALLOW_COPY_AND_ASSIGN(CastExtension);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAST_EXTENSION_H_

