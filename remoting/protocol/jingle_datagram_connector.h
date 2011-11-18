// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_DATAGRAM_CONNECTOR_H_
#define REMOTING_PROTOCOL_JINGLE_DATAGRAM_CONNECTOR_H_

#include <string>

#include "net/base/completion_callback.h"
#include "remoting/protocol/jingle_channel_connector.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

class JingleSession;

class JingleDatagramConnector : public JingleChannelConnector {
 public:
  JingleDatagramConnector(JingleSession* session,
                         const std::string& name,
                         const Session::DatagramChannelCallback& callback);
  virtual ~JingleDatagramConnector();

  // Starts connection process for the channel. |local_private_key| is
  // owned by the caller, and must exist until this object is
  // destroyed.
  virtual void Connect(bool initiator,
                       const std::string& local_cert,
                       const std::string& remote_cert,
                       crypto::RSAPrivateKey* local_private_key,
                       cricket::TransportChannel* raw_channel) OVERRIDE;

 private:
  JingleSession* session_;
  std::string name_;
  Session::DatagramChannelCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(JingleDatagramConnector);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_DATAGRAM_CONNECTOR_H_
