// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_H
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_H

#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"

namespace command_buffer {
namespace gles2 {

// A class that helps write GL command buffers.
class GLES2CmdHelper : public CommandBufferHelper {
 public:
  GLES2CmdHelper(
      NPP npp,
      const gpu_plugin::NPObjectPointer<gpu_plugin::CommandBuffer>&
          command_buffer)
      : CommandBufferHelper(npp, command_buffer) {
  }
  virtual ~GLES2CmdHelper() {
  }

  // Include the auto-generated part of this class. We split this because it
  // means we can easily edit the non-auto generated parts right here in this
  // file instead of having to edit some template or the code generator.
  #include "gpu/command_buffer/client/gles2_cmd_helper_autogen.h"

  DISALLOW_COPY_AND_ASSIGN(GLES2CmdHelper);
};

}  // namespace gles2
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_H

