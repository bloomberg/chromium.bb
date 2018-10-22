// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder.h"

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace webgpu {

class WebGPUDecoderTest : public ::testing::Test {
 public:
  WebGPUDecoderTest() {}

  void SetUp() override {
    command_buffer_service_.reset(new FakeCommandBufferServiceBase());
    decoder_.reset(WebGPUDecoder::Create(nullptr, command_buffer_service_.get(),
                                         &outputter_));
  }

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd)),
                                &entries_processed);
  }

 protected:
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  std::unique_ptr<WebGPUDecoder> decoder_;
  gles2::TraceOutputter outputter_;
  scoped_refptr<gles2::ContextGroup> group_;
};

TEST_F(WebGPUDecoderTest, Dummy) {
  cmds::Dummy dummy;
  dummy.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(dummy));
}
}  // namespace webgpu
}  // namespace gpu
