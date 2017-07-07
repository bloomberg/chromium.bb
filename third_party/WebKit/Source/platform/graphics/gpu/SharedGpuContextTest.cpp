// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedGpuContext.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using ::testing::Test;

namespace blink {

namespace {

class SharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    SharedGpuContext::SetContextProviderFactoryForTesting([this] {
      gl_.SetIsContextLost(false);
      return std::unique_ptr<WebGraphicsContext3DProvider>(
          new FakeWebGraphicsContext3DProvider(&gl_));
    });
  }

  void TearDown() override {
    SharedGpuContext::SetContextProviderFactoryForTesting(nullptr);
  }

  FakeGLES2Interface gl_;
};

TEST_F(SharedGpuContextTest, contextLossAutoRecovery) {
  EXPECT_TRUE(SharedGpuContext::IsValid());
  unsigned context_id = SharedGpuContext::ContextId();
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  EXPECT_TRUE(SharedGpuContext::IsValid());
  EXPECT_NE(context_id, SharedGpuContext::ContextId());
}

}  // unnamed namespace

}  // blink
