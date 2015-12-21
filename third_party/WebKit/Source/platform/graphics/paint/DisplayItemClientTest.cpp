// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#if ENABLE(ASSERT)

class TestDisplayItemClient : public DisplayItemClient {
public:
    String debugName() const final { return "TestDisplayItemClient"; }
    IntRect visualRect() const final { return IntRect(); }
};

TEST(DisplayItemClientTest, IsAlive)
{
    EXPECT_FALSE(DisplayItemClient::isAlive(*reinterpret_cast<DisplayItemClient*>(0x12345678)));
    TestDisplayItemClient* testClient = new TestDisplayItemClient;
    EXPECT_TRUE(DisplayItemClient::isAlive(*testClient));
    delete testClient;
    EXPECT_FALSE(DisplayItemClient::isAlive(*testClient));
}

#endif // ENABLE(ASSERT)

} // namespace
} // namespace blink
