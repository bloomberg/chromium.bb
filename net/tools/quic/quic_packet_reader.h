// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to read incoming QUIC packets from the UDP socket.

#ifndef NET_TOOLS_QUIC_QUIC_PACKET_READER_H_
#define NET_TOOLS_QUIC_QUIC_PACKET_READER_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include "base/macros.h"
#include "net/quic/quic_protocol.h"

namespace net {

namespace tools {

// Read in larger batches to minimize recvmmsg overhead.
const int kNumPacketsPerReadMmsgCall = 16;
// Allocate space for in6_pktinfo as it's larger than in_pktinfo
const int kSpaceForOverflowAndIp =
    CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(in6_pktinfo));

namespace test {
class QuicServerPeer;
}  // namespace test

class ProcessPacketInterface;
class QuicDispatcher;

class QuicPacketReader {
 public:
  QuicPacketReader();

  virtual ~QuicPacketReader();

  // Reads a number of packets from the given fd, and then passes them off to
  // the PacketProcessInterface.  Returns true if there may be additional
  // packets available on the socket.
  // Populates |packets_dropped| if it is non-null and the socket is configured
  // to track dropped packets and some packets are read.
  virtual bool ReadAndDispatchPackets(int fd,
                                      int port,
                                      ProcessPacketInterface* processor,
                                      QuicPacketCount* packets_dropped);

  // Same as ReadAndDispatchPackets, only does one packet at a time.
  static bool ReadAndDispatchSinglePacket(int fd,
                                          int port,
                                          ProcessPacketInterface* processor,
                                          QuicPacketCount* packets_dropped);

 private:
  // Initialize the internal state of the reader.
  void Initialize();

  // Storage only used when recvmmsg is available.

  // cbuf_ is used for ancillary data from the kernel on recvmmsg.
  char cbuf_[kSpaceForOverflowAndIp * kNumPacketsPerReadMmsgCall];
  // buf_ is used for the data read from the kernel on recvmmsg.
  // TODO(danzh): change it to be a pointer to avoid the allocation on the stack
  // from exceeding maximum allowed frame size.
  char buf_[kMaxPacketSize * kNumPacketsPerReadMmsgCall];
  // iov_ and mmsg_hdr_ are used to supply cbuf and buf to the recvmmsg call.
  iovec iov_[kNumPacketsPerReadMmsgCall];
  mmsghdr mmsg_hdr_[kNumPacketsPerReadMmsgCall];
  // raw_address_ is used for address information provided by the recvmmsg
  // call on the packets.
  struct sockaddr_storage raw_address_[kNumPacketsPerReadMmsgCall];

  DISALLOW_COPY_AND_ASSIGN(QuicPacketReader);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_PACKET_READER_H_
