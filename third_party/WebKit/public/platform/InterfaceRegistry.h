// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterfaceRegistry_h
#define InterfaceRegistry_h

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "public/platform/WebCommon.h"

#if INSIDE_BLINK
#include "mojo/public/cpp/bindings/interface_request.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/wtf/Functional.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

using InterfaceFactory = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

class BLINK_PLATFORM_EXPORT InterfaceRegistry {
 public:
  virtual void AddInterface(
      const char* name,
      const InterfaceFactory&,
      scoped_refptr<base::SingleThreadTaskRunner> = nullptr) = 0;

  static InterfaceRegistry* GetEmptyInterfaceRegistry();

#if INSIDE_BLINK
  template <typename Interface>
  void AddInterface(
      WTF::RepeatingFunction<void(mojo::InterfaceRequest<Interface>)> factory) {
    AddInterface(Interface::Name_,
                 ConvertToBaseCallback(WTF::Bind(
                     &InterfaceRegistry::ForwardToInterfaceFactory<Interface>,
                     std::move(factory))));
  }

  template <typename Interface>
  void AddInterface(WTF::Function<void(mojo::InterfaceRequest<Interface>),
                                  WTF::kCrossThreadAffinity> factory,
                    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    AddInterface(Interface::Name_,
                 ConvertToBaseCallback(blink::CrossThreadBind(
                     &InterfaceRegistry::ForwardToInterfaceFactory<
                         Interface, WTF::kCrossThreadAffinity>,
                     std::move(factory))),
                 std::move(task_runner));
  }

 private:
  template <
      typename Interface,
      WTF::FunctionThreadAffinity ThreadAffinity = WTF::kSameThreadAffinity>
  static void ForwardToInterfaceFactory(
      const WTF::RepeatingFunction<void(mojo::InterfaceRequest<Interface>),
                                   ThreadAffinity>& factory,
      mojo::ScopedMessagePipeHandle handle) {
    factory.Run(mojo::InterfaceRequest<Interface>(std::move(handle)));
  }
#endif  // INSIDE_BLINK
};

}  // namespace blink

#endif
