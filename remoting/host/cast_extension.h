// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAST_EXTENSION_H_
#define REMOTING_HOST_CAST_EXTENSION_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/host_extension.h"

namespace remoting {

namespace protocol {
class TransportContext;
}  // namespace protocol

// CastExtension extends HostExtension to enable WebRTC support.
class CastExtension : public HostExtension {
 public:
  CastExtension(scoped_refptr<protocol::TransportContext> transport_context);
  ~CastExtension() override;

  // HostExtension interface.
  std::string capability() const override;
  scoped_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub) override;

 private:
  scoped_refptr<protocol::TransportContext> transport_context_;

  DISALLOW_COPY_AND_ASSIGN(CastExtension);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAST_EXTENSION_H_

