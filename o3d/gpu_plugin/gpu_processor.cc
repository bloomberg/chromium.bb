// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_processor.h"
#include "o3d/gpu_plugin/system_services/shared_memory_public.h"

namespace o3d {
namespace gpu_plugin {

namespace {
void InvokeProcessCommands(void* data) {
  static_cast<GPUProcessor*>(data)->ProcessCommands();
}
}  // namespace anonymous

void GPUProcessor::ProcessCommands() {
  if (command_buffer_->GetErrorStatus())
    return;

  parser_->set_put(command_buffer_->GetPutOffset());

  int commands_processed = 0;
  while (commands_processed < commands_per_update_ && !parser_->IsEmpty()) {
    command_buffer::BufferSyncInterface::ParseError parse_error =
        parser_->ProcessCommand();
    switch (parse_error) {
      case command_buffer::BufferSyncInterface::kParseUnknownCommand:
      case command_buffer::BufferSyncInterface::kParseInvalidArguments:
        command_buffer_->SetParseError(parse_error);
        break;

      case command_buffer::BufferSyncInterface::kParseInvalidSize:
      case command_buffer::BufferSyncInterface::kParseOutOfBounds:
        command_buffer_->SetParseError(parse_error);
        command_buffer_->RaiseErrorStatus();
        return;
    }

    ++commands_processed;
  }

  command_buffer_->SetGetOffset(static_cast<int32>(parser_->get()));

  if (!parser_->IsEmpty()) {
    NPBrowser::get()->PluginThreadAsyncCall(npp_, InvokeProcessCommands, this);
  }
}

void *GPUProcessor::GetSharedMemoryAddress(unsigned int shm_id) {
  // TODO(apatrick): Verify that the NPClass is in fact shared memory before
  //    accessing the members.
  NPObjectPointer<CHRSharedMemory> shared_memory(static_cast<CHRSharedMemory*>(
      command_buffer_->GetRegisteredObject(static_cast<int32>(shm_id)).Get()));

  // Return address if shared memory is already mapped to this process.
  if (shared_memory->ptr)
    return shared_memory->ptr;

  // If the call fails or returns false then ptr will still be NULL.
  bool result;
  NPInvoke(npp_, shared_memory, "map", &result);

  return shared_memory->ptr;
}

size_t GPUProcessor::GetSharedMemorySize(unsigned int shm_id) {
  // TODO(apatrick): Verify that the NPClass is in fact shared memory before
  //    accessing the members.
  NPObjectPointer<CHRSharedMemory> shared_memory(static_cast<CHRSharedMemory*>(
      command_buffer_->GetRegisteredObject(static_cast<int32>(shm_id)).Get()));

  return shared_memory->size;
}

void GPUProcessor::set_token(unsigned int token) {
  command_buffer_->SetToken(static_cast<int32>(token));
}

}  // namespace gpu_plugin
}  // namespace o3d
