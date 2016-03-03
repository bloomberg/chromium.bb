// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushManager.h"

#include "bindings/modules/v8/UnionTypesModules.h"
#include "core/dom/DOMArrayBuffer.h"
#include "modules/push_messaging/PushSubscriptionOptions.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char kValidKeyMarker = 0x04;
const unsigned kValidKeyLength = 65;

TEST(PushManagerTest, ValidSenderKey)
{
    uint8_t senderKey[kValidKeyLength];
    memset(senderKey, 0, sizeof(senderKey));
    senderKey[0] = kValidKeyMarker;
    PushSubscriptionOptions options;
    options.setApplicationServerKey(
        ArrayBufferOrArrayBufferView::fromArrayBuffer(
            DOMArrayBuffer::create(senderKey, kValidKeyLength)));

    TrackExceptionState exceptionState;
    WebPushSubscriptionOptions output = PushManager::toWebPushSubscriptionOptions(options, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_EQ(output.applicationServerKey.length(), kValidKeyLength);
    EXPECT_EQ(output.applicationServerKey, WebString::fromUTF8(reinterpret_cast<const char*>(senderKey), kValidKeyLength));
}

TEST(PushManagerTest, InvalidSenderKeyMarker)
{
    uint8_t senderKey[kValidKeyLength];
    memset(senderKey, 0, sizeof(senderKey));
    senderKey[0] = 0x05;
    PushSubscriptionOptions options;
    options.setApplicationServerKey(
        ArrayBufferOrArrayBufferView::fromArrayBuffer(
            DOMArrayBuffer::create(senderKey, kValidKeyLength)));

    TrackExceptionState exceptionState;
    WebPushSubscriptionOptions output = PushManager::toWebPushSubscriptionOptions(options, exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
}

TEST(PushManagerTest, InvalidSenderKeyLength)
{
    uint8_t senderKey[kValidKeyLength - 1];
    memset(senderKey, 0, sizeof(senderKey));
    senderKey[0] = kValidKeyMarker;
    PushSubscriptionOptions options;
    options.setApplicationServerKey(
        ArrayBufferOrArrayBufferView::fromArrayBuffer(
            DOMArrayBuffer::create(senderKey, kValidKeyLength - 1)));

    TrackExceptionState exceptionState;
    WebPushSubscriptionOptions output = PushManager::toWebPushSubscriptionOptions(options, exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
}

} // namespace
} // namespace blink
