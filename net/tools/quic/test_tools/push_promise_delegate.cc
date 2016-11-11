// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/push_promise_delegate.h"

namespace net {
namespace test {
PushPromiseDelegate::PushPromiseDelegate(bool match)
    : match_(match), rendezvous_fired_(false), rendezvous_stream_(nullptr) {}

bool PushPromiseDelegate::CheckVary(const SpdyHeaderBlock& client_request,
                                    const SpdyHeaderBlock& promise_request,
                                    const SpdyHeaderBlock& promise_response) {
  DVLOG(1) << "match " << match_;
  return match_;
}

void PushPromiseDelegate::OnRendezvousResult(QuicSpdyStream* stream) {
  rendezvous_fired_ = true;
  rendezvous_stream_ = stream;
}

}  // namespace test
}  // namespace net
