// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEPPER_CHANNEL_H_
#define REMOTING_PROTOCOL_PEPPER_CHANNEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"

namespace pp {
class Instance;
}  // namespace pp

namespace cricket {
class Candidate;
}  // namespace cricket

namespace remoting {
namespace protocol {

class ChannelAuthenticator;
struct TransportConfig;

// Interface for stream and datagram channels used by PepperSession.
class PepperChannel : public base::NonThreadSafe {
 public:
  PepperChannel() { }
  virtual ~PepperChannel() { }

  // Connect the channel using specified |config|. The specified
  // |authenticator| is used to authenticate the channel. Takes
  // ownership of |authenticator|.
  virtual void Connect(pp::Instance* pp_instance,
                       const TransportConfig& config,
                       ChannelAuthenticator* authenticator) = 0;

  // Adds |candidate| received from the peer.
  virtual void AddRemoveCandidate(const cricket::Candidate& candidate) = 0;

  // Name of the channel.
  virtual const std::string& name() const = 0;

  // Returns true if the channel is already connected.
  virtual bool is_connected() const = 0;

 protected:
  DISALLOW_COPY_AND_ASSIGN(PepperChannel);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEPPER_CHANNEL_H_
