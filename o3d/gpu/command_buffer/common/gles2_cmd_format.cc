// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the binary format definition of the command buffer and
// command buffer commands.

// We explicitly do NOT include gles2_cmd_format.h here because client side
// and service side have different requirements.
#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace command_buffer {
namespace gles2 {

#include "gpu/command_buffer/common/gles2_cmd_ids_autogen.h"

const char* GetCommandName(CommandId id) {
  static const char* const names[] = {
  #define GLES2_CMD_OP(name) "k" # name,

  GLES2_COMMAND_LIST(GLES2_CMD_OP)

  #undef GLES2_CMD_OP
  };

  return (static_cast<int>(id) >= 0 && static_cast<int>(id) < kNumCommands) ?
      names[id] : "*unknown-command*";
}

}  // namespace gles2
}  // namespace command_buffer


