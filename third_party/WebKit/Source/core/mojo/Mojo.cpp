// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/mojo/Mojo.h"

#include "core/mojo/MojoCreateMessagePipeResult.h"
#include "core/mojo/MojoHandle.h"

namespace blink {

// static
void Mojo::createMessagePipe(MojoCreateMessagePipeResult& resultDict) {
  MojoCreateMessagePipeOptions options = {0};
  options.struct_size = sizeof(::MojoCreateMessagePipeOptions);
  options.flags = MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE;

  mojo::ScopedMessagePipeHandle handle0, handle1;
  MojoResult result = mojo::CreateMessagePipe(&options, &handle0, &handle1);

  resultDict.setResult(result);
  if (result == MOJO_RESULT_OK) {
    resultDict.setHandle0(
        MojoHandle::create(mojo::ScopedHandle::From(std::move(handle0))));
    resultDict.setHandle1(
        MojoHandle::create(mojo::ScopedHandle::From(std::move(handle1))));
  }
}

}  // namespace blink
