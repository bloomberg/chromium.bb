// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_PLATFORM_API_QUIC_TEST_H_
#define QUICHE_QUIC_PLATFORM_API_QUIC_TEST_H_

#include "quic/platform/api/quic_logging.h"
#include "net/quic/platform/impl/quic_test_impl.h"
#include "common/platform/api/quiche_test.h"

using QuicFlagSaver = QuicFlagSaverImpl;

// Defines the base classes to be used in QUIC tests.
using QuicTest = QuicTestImpl;
template <class T>
using QuicTestWithParam = QuicTestWithParamImpl<T>;

// Class which needs to be instantiated in tests which use threads.
using ScopedEnvironmentForThreads = ScopedEnvironmentForThreadsImpl;

#define QUIC_TEST_DISABLED_IN_CHROME(name) \
  QUIC_TEST_DISABLED_IN_CHROME_IMPL(name)

inline std::string QuicGetTestMemoryCachePath() {
  return QuicGetTestMemoryCachePathImpl();
}

#define QUIC_SLOW_TEST(test) QUIC_SLOW_TEST_IMPL(test)

#endif  // QUICHE_QUIC_PLATFORM_API_QUIC_TEST_H_
