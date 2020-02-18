// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_id.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace nqe {
namespace internal {

namespace {

TEST(NetworkIDTest, TestSerialize) {
  nqe::internal::NetworkID network_id(NetworkChangeNotifier::CONNECTION_2G,
                                      "test1", 2);
  std::string serialized = network_id.ToString();
  EXPECT_EQ(network_id, NetworkID::FromString(serialized));
}

}  // namespace

}  // namespace internal
}  // namespace nqe
}  // namespace net
