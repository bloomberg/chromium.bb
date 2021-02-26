// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_PLATFORM_IMPL_QUIC_MOCK_SYSCALL_WRAPPER_H_
#define QUICHE_QUIC_PLATFORM_IMPL_QUIC_MOCK_SYSCALL_WRAPPER_H_

#include "net/third_party/quiche/src/quic/core/quic_syscall_wrapper.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"

namespace quic {
namespace test {

class MockQuicSyscallWrapper : public QuicSyscallWrapper {
 public:
  // Create a standard mock object.
  MockQuicSyscallWrapper() = default;

  // Create a 'mockable' object that delegates everything to |delegate| by
  // default.
  explicit MockQuicSyscallWrapper(QuicSyscallWrapper* delegate);

  MOCK_METHOD(ssize_t,
              Sendmsg,
              (int sockfd, const msghdr*, int flags),
              (override));

  MOCK_METHOD(int,
              Sendmmsg,
              (int sockfd, mmsghdr*, unsigned int vlen, int flags),
              (override));
};

}  // namespace test
}  // namespace quic

#endif  // QUICHE_QUIC_PLATFORM_IMPL_QUIC_MOCK_SYSCALL_WRAPPER_H_
