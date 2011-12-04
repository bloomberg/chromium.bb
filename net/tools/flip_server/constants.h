// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_CONSTANTS_H_
#define NET_TOOLS_FLIP_SERVER_CONSTANTS_H_

#include "net/spdy/spdy_protocol.h"

const int kMSS = 1460;
const int kSSLOverhead = 25;
const int kSpdyOverhead = spdy::SpdyFrame::kHeaderSize;
const int kInitialDataSendersThreshold = (2 * kMSS) - kSpdyOverhead;
const int kSSLSegmentSize = (1 * kMSS) - kSSLOverhead;
const int kSpdySegmentSize = kSSLSegmentSize - kSpdyOverhead;

#define ACCEPTOR_CLIENT_IDENT \
    acceptor_->listen_ip_ << ":" \
    << acceptor_->listen_port_ << " "

#define NEXT_PROTO_STRING "\x06spdy/2\x08http/1.1\x08http/1.0"

#define SSL_CIPHER_LIST "!aNULL:!ADH:!eNull:!LOW:!EXP:RC4+RSA:MEDIUM:HIGH"

#define IPV4_PRINTABLE_FORMAT(IP) (((IP)>>0)&0xff), (((IP)>>8)&0xff), \
                                  (((IP)>>16)&0xff), (((IP)>>24)&0xff)

#define PIDFILE "/var/run/flip-server.pid"

#endif  // NET_TOOLS_FLIP_SERVER_CONSTANTS_H_
