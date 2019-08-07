// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_MAP_INTERNAL_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_MAP_INTERNAL_H_

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace service_manager {
namespace internal {

template <typename ContextType>
struct BinderContextTraits {
  using ValueType = ContextType;

  using GenericBinderType =
      base::RepeatingCallback<void(ContextType, mojo::ScopedMessagePipeHandle)>;

  template <typename Interface>
  using BinderType =
      base::RepeatingCallback<void(ContextType,
                                   mojo::PendingReceiver<Interface> receiver)>;

  template <typename Interface>
  static GenericBinderType MakeGenericBinder(BinderType<Interface> binder) {
    return base::BindRepeating(&BindGenericReceiver<Interface>,
                               std::move(binder));
  }

  template <typename Interface>
  static void BindGenericReceiver(const BinderType<Interface>& binder,
                                  ContextType context,
                                  mojo::ScopedMessagePipeHandle receiver_pipe) {
    binder.Run(std::move(context),
               mojo::PendingReceiver<Interface>(std::move(receiver_pipe)));
  }
};

template <>
struct BinderContextTraits<void> {
  // Not used, but exists so that BinderMapWithContext can define a compilable
  // variant of |TryBind()| which ostensibly takes a context value even for
  // maps with void context. The implementation will always fail a static_assert
  // at compile time if actually used on such maps. See |BindMap::TryBind()|.
  using ValueType = bool;

  using GenericBinderType =
      base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

  template <typename Interface>
  using BinderType =
      base::RepeatingCallback<void(mojo::PendingReceiver<Interface> receiver)>;

  template <typename Interface>
  static GenericBinderType MakeGenericBinder(BinderType<Interface> binder) {
    return base::BindRepeating(&BindGenericReceiver<Interface>,
                               std::move(binder));
  }

  template <typename Interface>
  static void BindGenericReceiver(const BinderType<Interface>& binder,
                                  mojo::ScopedMessagePipeHandle receiver_pipe) {
    binder.Run(mojo::PendingReceiver<Interface>(std::move(receiver_pipe)));
  }
};

}  // namespace internal
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_MAP_INTERNAL_H_
