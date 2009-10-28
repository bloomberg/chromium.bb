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


// This file contains the command buffer helper class.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/common/cross/constants.h"
#include "command_buffer/common/cross/cmd_buffer_common.h"
#include "o3d/gpu_plugin/np_utils/np_object_pointer.h"

namespace o3d {
namespace command_buffer {

// Command buffer helper class. This class simplifies ring buffer management:
// it will allocate the buffer, give it to the buffer interface, and let the
// user add commands to it, while taking care of the synchronization (put and
// get). It also provides a way to ensure commands have been executed, through
// the token mechanism:
//
// helper.AddCommand(...);
// helper.AddCommand(...);
// int32 token = helper.InsertToken();
// helper.AddCommand(...);
// helper.AddCommand(...);
// [...]
//
// helper.WaitForToken(token);  // this doesn't return until the first two
//                              // commands have been executed.
class CommandBufferHelper {
 public:
  CommandBufferHelper(
      NPP npp,
      const gpu_plugin::NPObjectPointer<NPObject>& command_buffer);
  virtual ~CommandBufferHelper();

  bool Initialize();

  // Flushes the commands, setting the put pointer to let the buffer interface
  // know that new commands have been added. After a flush returns, the command
  // buffer service is aware of all pending commands and it is guaranteed to
  // have made some progress in processing them. Returns whether the flush was
  // successful. The flush will fail if the command buffer service has
  // disconnected.
  bool Flush();

  // Waits until all the commands have been executed. Returns whether it
  // was successful. The function will fail if the command buffer service has
  // disconnected.
  bool Finish();

  // Waits until a given number of available entries are available.
  // Parameters:
  //   count: number of entries needed. This value must be at most
  //     the size of the buffer minus one.
  void WaitForAvailableEntries(int32 count);

  // Adds a command data to the command buffer. This may wait until sufficient
  // space is available.
  // Parameters:
  //   entries: The command entries to add.
  //   count: The number of entries.
  void AddCommandData(const CommandBufferEntry* entries, int32 count) {
    WaitForAvailableEntries(count);
    for (; count > 0; --count) {
      entries_[put_++] = *entries++;
    }
    DCHECK_LE(put_, entry_count_);
    if (put_ == entry_count_) put_ = 0;
  }

  // A typed version of AddCommandData.
  template <typename T>
  void AddTypedCmdData(const T& cmd) {
    AddCommandData(reinterpret_cast<const CommandBufferEntry*>(&cmd),
                   ComputeNumEntries(sizeof(cmd)));
  }

  // Adds a command to the command buffer. This may wait until sufficient space
  // is available.
  // Parameters:
  //   command: the command index.
  //   arg_count: the number of arguments for the command.
  //   args: the arguments for the command (these are copied before the
  //     function returns).
  void AddCommand(int32 command,
                  int32 arg_count,
                  const CommandBufferEntry *args) {
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    WaitForAvailableEntries(header.size);
    entries_[put_++].value_header = header;
    for (int i = 0; i < arg_count; ++i) {
      entries_[put_++] = args[i];
    }
    DCHECK_LE(put_, entry_count_);
    if (put_ == entry_count_) put_ = 0;
  }

  // Inserts a new token into the command buffer. This token either has a value
  // different from previously inserted tokens, or ensures that previously
  // inserted tokens with that value have already passed through the command
  // stream.
  // Returns:
  //   the value of the new token or -1 if the command buffer reader has
  //   shutdown.
  int32 InsertToken();

  // Waits until the token of a particular value has passed through the command
  // stream (i.e. commands inserted before that token have been executed).
  // NOTE: This will call Flush if it needs to block.
  // Parameters:
  //   the value of the token to wait for.
  void WaitForToken(int32 token);

  // Waits for a certain amount of space to be available. Returns address
  // of space.
  CommandBufferEntry* GetSpace(uint32 entries);

  // Typed version of GetSpace. Gets enough room for the given type and returns
  // a reference to it.
  template <typename T>
  T& GetCmdSpace() {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    uint32 space_needed = ComputeNumEntries(sizeof(T));
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T& GetImmediateCmdSpace(size_t space) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    uint32 space_needed = ComputeNumEntries(sizeof(T) + space);
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  parse_error::ParseError GetParseError();

  // Common Commands
  void Noop(uint32 skip_count) {
    cmd::Noop& cmd = GetImmediateCmdSpace<cmd::Noop>(
        skip_count * sizeof(CommandBufferEntry));
    cmd.Init(skip_count);
  }

  void SetToken(uint32 token) {
    cmd::SetToken& cmd = GetCmdSpace<cmd::SetToken>();
    cmd.Init(token);
  }


 private:
  // Waits until get changes, updating the value of get_.
  void WaitForGetChange();

  // Returns the number of available entries (they may not be contiguous).
  int32 AvailableEntries() {
    return (get_ - put_ - 1 + entry_count_) % entry_count_;
  }

  NPP npp_;
  gpu_plugin::NPObjectPointer<NPObject> command_buffer_;
  gpu_plugin::NPObjectPointer<NPObject> ring_buffer_;
  CommandBufferEntry *entries_;
  int32 entry_count_;
  int32 token_;
  int32 last_token_read_;
  int32 get_;
  int32 put_;

  friend class CommandBufferHelperTest;
  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelper);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
