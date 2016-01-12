// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_packet_reader.h"

#include <errno.h>
#ifndef __APPLE__
// This is a GNU header that is not present in /usr/include on MacOS
#include <features.h>
#endif
#include <string.h>
#include <sys/epoll.h>

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_bug_tracker.h"
#include "net/quic/quic_flags.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_socket_utils.h"

#define MMSG_MORE 0

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

namespace net {

namespace tools {

QuicPacketReader::QuicPacketReader() {
  Initialize();
}

void QuicPacketReader::Initialize() {
  // Zero initialize uninitialized memory.
  memset(cbuf_, 0, arraysize(cbuf_));
  memset(buf_, 0, arraysize(buf_));
  memset(raw_address_, 0, sizeof(raw_address_));
  memset(mmsg_hdr_, 0, sizeof(mmsg_hdr_));

  for (int i = 0; i < kNumPacketsPerReadMmsgCall; ++i) {
    iov_[i].iov_base = buf_ + (kMaxPacketSize * i);
    iov_[i].iov_len = kMaxPacketSize;

    msghdr* hdr = &mmsg_hdr_[i].msg_hdr;
    hdr->msg_name = &raw_address_[i];
    hdr->msg_namelen = sizeof(sockaddr_storage);
    hdr->msg_iov = &iov_[i];
    hdr->msg_iovlen = 1;

    hdr->msg_control = cbuf_ + kSpaceForOverflowAndIp * i;
    hdr->msg_controllen = kSpaceForOverflowAndIp;
  }
}

QuicPacketReader::~QuicPacketReader() {}

bool QuicPacketReader::ReadAndDispatchPackets(
    int fd,
    int port,
    ProcessPacketInterface* processor,
    QuicPacketCount* packets_dropped) {
#if MMSG_MORE
  // Re-set the length fields in case recvmmsg has changed them.
  for (int i = 0; i < kNumPacketsPerReadMmsgCall; ++i) {
    iov_[i].iov_len = kMaxPacketSize;
    mmsg_hdr_[i].msg_len = 0;
    msghdr* hdr = &mmsg_hdr_[i].msg_hdr;
    hdr->msg_namelen = sizeof(sockaddr_storage);
    hdr->msg_iovlen = 1;
    hdr->msg_controllen = kSpaceForOverflowAndIp;
  }

  int packets_read =
      recvmmsg(fd, mmsg_hdr_, kNumPacketsPerReadMmsgCall, 0, nullptr);

  if (packets_read <= 0) {
    return false;  // recvmmsg failed.
  }

  for (int i = 0; i < packets_read; ++i) {
    if (mmsg_hdr_[i].msg_len == 0) {
      continue;
    }

    IPEndPoint client_address = IPEndPoint(raw_address_[i]);
    IPAddressNumber server_ip =
        QuicSocketUtils::GetAddressFromMsghdr(&mmsg_hdr_[i].msg_hdr);
    if (!IsInitializedAddress(server_ip)) {
      QUIC_BUG << "Unable to get server address.";
      continue;
    }

    QuicEncryptedPacket packet(reinterpret_cast<char*>(iov_[i].iov_base),
                               mmsg_hdr_[i].msg_len, false);
    IPEndPoint server_address(server_ip, port);
    processor->ProcessPacket(server_address, client_address, packet);
  }

  if (packets_dropped != nullptr) {
    QuicSocketUtils::GetOverflowFromMsghdr(&mmsg_hdr_[0].msg_hdr,
                                           packets_dropped);
  }

  // We may not have read all of the packets available on the socket.
  return packets_read == kNumPacketsPerReadMmsgCall;
#else
  LOG(FATAL) << "Unsupported";
  return false;
#endif
}

/* static */
bool QuicPacketReader::ReadAndDispatchSinglePacket(
    int fd,
    int port,
    ProcessPacketInterface* processor,
    QuicPacketCount* packets_dropped) {
  char buf[kMaxPacketSize];

  IPEndPoint client_address;
  IPAddressNumber server_ip;
  int bytes_read = QuicSocketUtils::ReadPacket(
      fd, buf, arraysize(buf), packets_dropped, &server_ip, &client_address);

  if (bytes_read < 0) {
    return false;  // ReadPacket failed.
  }

  QuicEncryptedPacket packet(buf, bytes_read, false);
  IPEndPoint server_address(server_ip, port);
  processor->ProcessPacket(server_address, client_address, packet);

  // The socket read was successful, so return true even if packet dispatch
  // failed.
  return true;
}

}  // namespace tools

}  // namespace net
