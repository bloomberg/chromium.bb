// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/Mojo.h"

#include "core/mojo/MojoCreateDataPipeOptions.h"
#include "core/mojo/MojoCreateDataPipeResult.h"
#include "core/mojo/MojoCreateMessagePipeResult.h"
#include "core/mojo/MojoCreateSharedBufferResult.h"
#include "core/mojo/MojoHandle.h"

namespace blink {

// static
void Mojo::createMessagePipe(MojoCreateMessagePipeResult& result_dict) {
  MojoCreateMessagePipeOptions options = {0};
  options.struct_size = sizeof(::MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE;

  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult result = mojo::CreateMessagePipe(&options, &handle0, &handle1);

  result_dict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict.setHandle0(
        MojoHandle::Create(mojo::ScopedHandle::From(std::move(handle0))));
    result_dict.setHandle1(
        MojoHandle::Create(mojo::ScopedHandle::From(std::move(handle1))));
  }
}

// static
void Mojo::createDataPipe(const MojoCreateDataPipeOptions& options_dict,
                          MojoCreateDataPipeResult& result_dict) {
  ::MojoCreateDataPipeOptions options = {0};
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = options_dict.elementNumBytes();
  options.capacity_num_bytes = options_dict.capacityNumBytes();

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult result = mojo::CreateDataPipe(&options, &producer, &consumer);
  result_dict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict.setProducer(
        MojoHandle::Create(mojo::ScopedHandle::From(std::move(producer))));
    result_dict.setConsumer(
        MojoHandle::Create(mojo::ScopedHandle::From(std::move(consumer))));
  }
}

// static
void Mojo::createSharedBuffer(unsigned num_bytes,
                              MojoCreateSharedBufferResult& result_dict) {
  MojoCreateSharedBufferOptions* options = nullptr;
  mojo::Handle handle;
  MojoResult result =
      MojoCreateSharedBuffer(options, num_bytes, handle.mutable_value());

  result_dict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict.setHandle(MojoHandle::Create(mojo::MakeScopedHandle(handle)));
  }
}

}  // namespace blink
