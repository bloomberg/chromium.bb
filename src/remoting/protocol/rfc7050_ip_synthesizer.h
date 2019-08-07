// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_RFC7050_IP_SYNTHESIZER_H_
#define REMOTING_PROTOCOL_RFC7050_IP_SYNTHESIZER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {
namespace protocol {

// A class that uses RFC 7050 to discover the DNS64 prefix and use that to
// synthesize IPv6 addresses on DNS64/NAT64 network.
class Rfc7050IpSynthesizer {
 public:
  static Rfc7050IpSynthesizer* GetInstance();

  ~Rfc7050IpSynthesizer();

  // Updates the DNS64 prefix to be used to synthesize the IP address. Calls
  // |on_done| once the update is finished.
  void UpdateDns64Prefix(base::OnceClosure on_done);

  // Returns a socket by converting the |original_socket|'s IP address into
  // the synthesized one using RFC 7050.
  rtc::SocketAddress ToNativeSocket(
      const rtc::SocketAddress& original_socket) const;

 private:
  using ResolveIpv4OnlyHostCallback =
      base::RepeatingCallback<int(rtc::IPAddress*)>;

  friend class Rfc7050IpSynthesizerTest;
  friend class base::NoDestructor<Rfc7050IpSynthesizer>;

  Rfc7050IpSynthesizer();

  void UpdateDns64PrefixOnBackgroundThread();
  rtc::IPAddress DiscoverDns64Prefix() const;

  mutable base::Lock lock_;
  rtc::IPAddress dns64_prefix_;
  ResolveIpv4OnlyHostCallback resolve_ipv4_only_host_;

  DISALLOW_COPY_AND_ASSIGN(Rfc7050IpSynthesizer);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_RFC7050_IP_SYNTHESIZER_H_
