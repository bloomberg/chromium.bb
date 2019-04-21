// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/native_ip_synthesizer.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "net/base/sys_addrinfo.h"
#include "remoting/protocol/rfc7050_ip_synthesizer.h"
#include "remoting/protocol/rfc7050_prefix_refresher.h"
#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {
namespace protocol {

// static
rtc::SocketAddress ToNativeSocket(const rtc::SocketAddress& original_socket) {
#if defined(OS_IOS)
  // iOS implements IPv6 synthesis logic into the getaddrinfo() function. See:
  // https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/NetworkingOverview/UnderstandingandPreparingfortheIPv6Transition/UnderstandingandPreparingfortheIPv6Transition.html#//apple_ref/doc/uid/TP40010220-CH213-DontLinkElementID_4

  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_DEFAULT;

  addrinfo* result = nullptr;
  // getaddrinfo() will resolve an IPv4 address into its curresponding IPv4/IPv6
  // address connectable on current network environment. Note that this doesn't
  // really send out a DNS request on iOS.
  int error = getaddrinfo(original_socket.ipaddr().ToString().c_str(), nullptr,
                          &hints, &result);
  if (error) {
    LOG(ERROR) << "getaddrinfo() failed for " << gai_strerror(error);
    return original_socket;
  }

  if (!result) {
    return original_socket;
  }

  rtc::SocketAddress new_socket;
  bool success = rtc::SocketAddressFromSockAddrStorage(
      *reinterpret_cast<sockaddr_storage*>(result->ai_addr), &new_socket);
  DCHECK(success);
  freeaddrinfo(result);
  new_socket.SetPort(original_socket.port());
  return new_socket;
#elif defined(OS_ANDROID)
  // Android implements 464XLAT and emulates an IPv4 network stack.
  return original_socket;
#else
  return Rfc7050IpSynthesizer::GetInstance()->ToNativeSocket(original_socket);
#endif
}

#if defined(OS_NACL)
void RefreshNativeIpSynthesizer(base::OnceClosure on_done) {
  Rfc7050IpSynthesizer::GetInstance()->UpdateDns64Prefix(std::move(on_done));
}
#elif !defined(OS_ANDROID) && !defined(OS_IOS)
void InitializeNativeIpSynthesizer() {
  static base::NoDestructor<Rfc7050PrefixRefresher> refresher;
}
#endif

}  // namespace protocol
}  // namespace remoting
