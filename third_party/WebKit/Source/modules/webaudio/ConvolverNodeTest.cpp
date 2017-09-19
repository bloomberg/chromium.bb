// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DummyPageHolder.h"
#include "modules/webaudio/ConvolverNode.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

TEST(ConvolverNodeTest, ReverbLifetime) {
  std::unique_ptr<DummyPageHolder> page = DummyPageHolder::Create();
  OfflineAudioContext* context = OfflineAudioContext::Create(
      &page->GetDocument(), 2, 1, 48000, ASSERT_NO_EXCEPTION);
  ConvolverNode* node = context->createConvolver(ASSERT_NO_EXCEPTION);
  ConvolverHandler& handler = node->GetConvolverHandler();
  EXPECT_FALSE(handler.reverb_);
  node->setBuffer(AudioBuffer::Create(2, 1, 48000), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(handler.reverb_);
  BaseAudioContext::GraphAutoLocker locker(context);
  handler.Dispose();
  // m_reverb should live after dispose() because an audio thread is using it.
  EXPECT_TRUE(handler.reverb_);
}

}  // namespace blink
