// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rfc7050_ip_synthesizer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "net/base/sys_addrinfo.h"

namespace {

// Well-known IPs: 192.0.0.170 and 192.0.0.171
constexpr char kIpv4OnlyHost[] = "ipv4only.arpa";

// Only /96 DNS64 prefixes are supported at this time.
constexpr int kPrefixLength = 96;

// Resolves |kIpv4OnlyHost| and returns the result of getaddrinfo.
int ResolveIpv4OnlyHost(rtc::IPAddress* out_ip) {
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET6;
  addrinfo* result = nullptr;
  int error = getaddrinfo(kIpv4OnlyHost, nullptr, &hints, &result);
  if (!error && !rtc::IPFromAddrInfo(result, out_ip)) {
    LOG(ERROR) << "Failed to get IP from addrinfo";
    error = EAI_FAIL;
  }
  if (result) {
    freeaddrinfo(result);
  }
  return error;
}

}  // namespace

namespace remoting {
namespace protocol {

// static
Rfc7050IpSynthesizer* Rfc7050IpSynthesizer::GetInstance() {
  static base::NoDestructor<Rfc7050IpSynthesizer> instance;
  return instance.get();
}

Rfc7050IpSynthesizer::Rfc7050IpSynthesizer()
    : resolve_ipv4_only_host_(base::BindRepeating(&ResolveIpv4OnlyHost)) {}

Rfc7050IpSynthesizer::~Rfc7050IpSynthesizer() {
  LOG(WARNING) << "Rfc7050IpSynthesizer should never be destructed unless it's "
                  "in unit test.";
}

void Rfc7050IpSynthesizer::UpdateDns64Prefix(base::OnceClosure on_done) {
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock()},
      // Safe to use raw pointer because the singleton is never destroyed.
      base::BindOnce(&Rfc7050IpSynthesizer::UpdateDns64PrefixOnBackgroundThread,
                     base::Unretained(this)),
      std::move(on_done));
}

rtc::SocketAddress Rfc7050IpSynthesizer::ToNativeSocket(
    const rtc::SocketAddress& original_socket) const {
  in6_addr new_ipv6_address;

  {
    // Ideally read operations should share the same lock, but Chromium has no
    // ready-to-use readers-writer lock implementation.
    base::AutoLock auto_lock(lock_);
    if (dns64_prefix_.IsNil() || original_socket.family() != AF_INET) {
      return original_socket;
    }
    DCHECK_EQ(AF_INET6, dns64_prefix_.family());
    new_ipv6_address = dns64_prefix_.ipv6_address();
  }

  in_addr ipv4_address = original_socket.ipaddr().ipv4_address();
  // Replace the last 4 bytes with the IPv4 address.
  memcpy(&new_ipv6_address.s6_addr[12], &ipv4_address.s_addr,
         sizeof(ipv4_address.s_addr));
  rtc::IPAddress new_ip(new_ipv6_address);
  return rtc::SocketAddress(new_ip, original_socket.port());
}

void Rfc7050IpSynthesizer::UpdateDns64PrefixOnBackgroundThread() {
  rtc::IPAddress new_prefix = DiscoverDns64Prefix();

  {
    base::AutoLock auto_lock(lock_);
    dns64_prefix_ = new_prefix;
  }
}

rtc::IPAddress Rfc7050IpSynthesizer::DiscoverDns64Prefix() const {
  // This is not the full RFC 7050 implementation. It only implements /96 prefix
  // discovery, as Android CLAT does. See https://tools.ietf.org/html/rfc7050
  rtc::IPAddress prefix;

  rtc::IPAddress ip;
  int error = resolve_ipv4_only_host_.Run(&ip);
  if (error) {
    if (error == EAI_NONAME) {
      VLOG(0) << "Node is unknown. You are probably not in DNS64/NAT64 "
              << "network.";
    } else {
      LOG(WARNING) << "Failed to discover DNS64 prefix. Error: " << error
                   << ", " << gai_strerror(error);
    }
    return prefix;
  }

  if (ip.family() != AF_INET6 || rtc::IPIsV4Mapped(ip)) {
    VLOG(0)
        << "Resolved IP is not in IPv6. You probably don't have DNS64 prefix";
  } else {
    prefix = rtc::TruncateIP(ip, kPrefixLength);
    VLOG(0) << "Discovered DNS64/NAT64 prefix: " << prefix.ToString();
  }
  return prefix;
}

}  // namespace protocol
}  // namespace remoting
