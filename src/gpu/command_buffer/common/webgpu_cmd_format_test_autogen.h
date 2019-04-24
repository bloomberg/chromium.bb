// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file contains unit tests for webgpu commands
// It is included by webgpu_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(WebGPUFormatTest, Dummy) {
  cmds::Dummy& cmd = *GetBufferAs<cmds::Dummy>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Dummy::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_WEBGPU_CMD_FORMAT_TEST_AUTOGEN_H_
