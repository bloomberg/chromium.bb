// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdaterTestNetwork, NetworkFetcherWinHTTPFactory) {
  base::MessageLoopForUI message_loop;
  auto fetcher = base::MakeRefCounted<NetworkFetcherFactory>()->Create();
  EXPECT_NE(nullptr, fetcher.get());
}

}  // namespace updater
