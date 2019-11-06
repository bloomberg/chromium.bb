// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NATIVE_IP_SYNTHESIZER_H_
#define REMOTING_PROTOCOL_NATIVE_IP_SYNTHESIZER_H_

#include "base/callback.h"
#include "build/build_config.h"

namespace rtc {
class SocketAddress;
}  // namespace rtc

namespace remoting {
namespace protocol {

// Converts a socket in IPv4 literal to IPv6 literal that is acceptable by the
// local NAT64 (i.e. IPv6->IPv4) gateway of the network interface with the
// highest priority. Note that the synthesized address might not be connectible
// on network interfaces with lower priority.
//
// Returns the |original_socket| if we are not under DNS64/NAT64 network, or the
// OS emulates the IPv4 stack (464 CLAT), or the socket is in IPv6 literal.
//
// Chromotocol only has IPv4 relay endpoints, while DNS64/NAT64 network does not
// natively support connecting to IPv4 literals. This function allows host and
// client to connect using relay, but won't fix direct and STUN connection.
// WebRTC has IPv6 relay endpoint so this function is not needed.
rtc::SocketAddress ToNativeSocket(const rtc::SocketAddress& original_socket);

#if defined(OS_NACL)
// Refreshes the NativeIpSynthesizer so that it picks up the latest rule to
// synthesize IP address. You may want to call this before each P2P connection
// attempt. |on_done| will be called once the synthesizer is updated.
//
// This is only available to NaCl because NaCl doesn't support
// NetworkChangeNotifier, so caller must manually refresh to make sure the
// synthesizer is up to date.
void RefreshNativeIpSynthesizer(base::OnceClosure on_done);
#elif !defined(OS_ANDROID) && !defined(OS_IOS)
// Call this to setup the native IP synthesizer. Does nothing if the synthesizer
// is already initialized.
//
// This will cause the IP synthesizer to refresh itself when this function is
// first called, and every time the network conditions are changed.
void InitializeNativeIpSynthesizer();
#endif
// Android and iOS have built-in IPv6 synthesizing logic so refreshing IP
// synthesizer is not needed.

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NATIVE_IP_SYNTHESIZER_H_
