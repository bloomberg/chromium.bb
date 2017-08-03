// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterfaceProvider_h
#define InterfaceProvider_h

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"

namespace blink {

// Implementations of blink::InterfaceProvider should be thread safe. As such it
// is okay to call |GetInterface| from any thread, without the thread hopping
// that would have been necesary with service_manager::InterfaceProvider.
class BLINK_PLATFORM_EXPORT InterfaceProvider {
 public:
  virtual void GetInterface(const char* name,
                            mojo::ScopedMessagePipeHandle) = 0;

  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> ptr) {
    GetInterface(Interface::Name_, ptr.PassMessagePipe());
  }

  static InterfaceProvider* GetEmptyInterfaceProvider();
};

}  // namespace blink

#endif
