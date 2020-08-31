// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_HANDLE_H_
#define IPC_IPC_CHANNEL_HANDLE_H_

#include <string>

#include "build/build_config.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if defined(OS_NACL_SFI)
#include "base/file_descriptor_posix.h"
#endif  // defined (OS_NACL_SFI)

namespace IPC {

// Note that serialization for this object is defined in the ParamTraits
// template specialization in ipc_message_utils.h.
#if defined(OS_NACL_SFI)
struct ChannelHandle {
  ChannelHandle() {}
  explicit ChannelHandle(const base::FileDescriptor& s) : socket(s) {}

  base::FileDescriptor socket;
};
#else
struct ChannelHandle {
  ChannelHandle() {}
  ChannelHandle(mojo::MessagePipeHandle h) : mojo_handle(h) {}

  bool is_mojo_channel_handle() const { return mojo_handle.is_valid(); }

  mojo::MessagePipeHandle mojo_handle;
};
#endif  // defined(OS_NACL_SFI)

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_HANDLE_H_
