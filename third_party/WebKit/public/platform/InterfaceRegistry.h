// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterfaceRegistry_h
#define InterfaceRegistry_h

#include "base/callback_forward.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"

#if INSIDE_BLINK
#include "mojo/public/cpp/bindings/interface_request.h"
#include "platform/wtf/Functional.h"
#endif

namespace blink {

using InterfaceFactory = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

class BLINK_PLATFORM_EXPORT InterfaceRegistry {
 public:
  virtual void AddInterface(const char* name, const InterfaceFactory&) = 0;

#if INSIDE_BLINK
  static InterfaceRegistry* GetEmptyInterfaceRegistry();

  template <typename Interface>
  void AddInterface(
      std::unique_ptr<WTF::Function<void(mojo::InterfaceRequest<Interface>)>>
          factory) {
    AddInterface(Interface::Name_,
                 ConvertToBaseCallback(WTF::Bind(
                     &InterfaceRegistry::ForwardToInterfaceFactory<Interface>,
                     std::move(factory))));
  }

 private:
  template <typename Interface>
  static void ForwardToInterfaceFactory(
      const std::unique_ptr<
          WTF::Function<void(mojo::InterfaceRequest<Interface>)>>& factory,
      mojo::ScopedMessagePipeHandle handle) {
    (*factory)(mojo::InterfaceRequest<Interface>(std::move(handle)));
  }
#endif  // INSIDE_BLINK
};

}  // namespace blink

#endif
