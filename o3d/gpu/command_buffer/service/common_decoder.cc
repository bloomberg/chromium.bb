/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gpu/command_buffer/service/precompile.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"

namespace command_buffer {

void* CommonDecoder::GetAddressAndCheckSize(unsigned int shm_id,
                                            unsigned int offset,
                                            unsigned int size) {
  void* shm_addr = engine_->GetSharedMemoryAddress(shm_id);
  if (!shm_addr) return NULL;
  size_t shm_size = engine_->GetSharedMemorySize(shm_id);
  unsigned int end = offset + size;
  if (end > shm_size || end < offset) {
    return NULL;
  }
  return static_cast<int8 *>(shm_addr) + offset;
}

const char* CommonDecoder::GetCommonCommandName(
    cmd::CommandId command_id) const {
  return cmd::GetCommandName(command_id);
}

namespace {

// A struct to hold info about each command.
struct CommandInfo {
  int arg_flags;  // How to handle the arguments for this command
  int arg_count;  // How many arguments are expected for this command.
};

// A table of CommandInfo for all the commands.
const CommandInfo g_command_info[] = {
  #define COMMON_COMMAND_BUFFER_CMD_OP(name) {                           \
    cmd::name::kArgFlags,                                                \
    sizeof(cmd::name) / sizeof(CommandBufferEntry) - 1, },  /* NOLINT */ \

  COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

  #undef COMMON_COMMAND_BUFFER_CMD_OP
};

}  // anonymous namespace.

// Decode command with its arguments, and call the corresponding method.
// Note: args is a pointer to the command buffer. As such, it could be changed
// by a (malicious) client at any time, so if validation has to happen, it
// should operate on a copy of them.
parse_error::ParseError CommonDecoder::DoCommonCommand(
    unsigned int command,
    unsigned int arg_count,
    const void* cmd_data) {
  if (command < arraysize(g_command_info)) {
    const CommandInfo& info = g_command_info[command];
    unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
    if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
        (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
      switch (command) {
        #define COMMON_COMMAND_BUFFER_CMD_OP(name)                      \
          case cmd::name::kCmdId:                                       \
            return Handle ## name(                                      \
                arg_count,                                              \
                *static_cast<const cmd::name*>(cmd_data));              \

        COMMON_COMMAND_BUFFER_CMDS(COMMON_COMMAND_BUFFER_CMD_OP)

        #undef COMMON_COMMAND_BUFFER_CMD_OP
      }
    } else {
      return parse_error::kParseInvalidArguments;
    }
  }
  return DoCommonCommand(command, arg_count, cmd_data);
  return parse_error::kParseUnknownCommand;
}

parse_error::ParseError CommonDecoder::HandleNoop(
    uint32 arg_count,
    const cmd::Noop& args) {
  return parse_error::kParseNoError;
}

parse_error::ParseError CommonDecoder::HandleSetToken(
    uint32 arg_count,
    const cmd::SetToken& args) {
  engine_->set_token(args.token);
  return parse_error::kParseNoError;
}

}  // namespace command_buffer
