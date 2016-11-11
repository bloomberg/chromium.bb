// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_PUSH_PROMISE_DELEGATE_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_PUSH_PROMISE_DELEGATE_H_

#include "net/quic/core/quic_client_push_promise_index.h"

namespace net {
namespace test {

class PushPromiseDelegate : public QuicClientPushPromiseIndex::Delegate {
 public:
  explicit PushPromiseDelegate(bool match);

  bool CheckVary(const SpdyHeaderBlock& client_request,
                 const SpdyHeaderBlock& promise_request,
                 const SpdyHeaderBlock& promise_response) override;

  void OnRendezvousResult(QuicSpdyStream* stream) override;

  QuicSpdyStream* rendezvous_stream() { return rendezvous_stream_; }
  bool rendezvous_fired() { return rendezvous_fired_; }

 private:
  bool match_;
  bool rendezvous_fired_;
  QuicSpdyStream* rendezvous_stream_;
};

}  // namespace test
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_PUSH_PROMISE_DELEGATE_H_
