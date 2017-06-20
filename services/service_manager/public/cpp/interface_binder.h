// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace service_manager {

template <typename... BinderArgs>
void BindCallbackAdapter(
    const base::Callback<void(mojo::ScopedMessagePipeHandle, BinderArgs...)>&
        callback,
    const BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle,
    BinderArgs... args) {
  callback.Run(std::move(handle), args...);
}

template <typename... BinderArgs>
class InterfaceBinder {
 public:
  virtual ~InterfaceBinder() {}

  // Asks the InterfaceBinder to bind an implementation of the specified
  // interface to the request passed via |handle|. If the InterfaceBinder binds
  // an implementation it must take ownership of the request handle.
  virtual void BindInterface(const BindSourceInfo& source_info,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle handle,
                             BinderArgs... args) = 0;
};

template <typename Interface, typename... BinderArgs>
class CallbackBinder : public InterfaceBinder<BinderArgs...> {
 public:
  using BindCallback = base::Callback<void(const BindSourceInfo&,
                                           mojo::InterfaceRequest<Interface>)>;

  CallbackBinder(const BindCallback& callback,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  ~CallbackBinder() override {}

 private:
  // InterfaceBinder:
  void BindInterface(const BindSourceInfo& source_info,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle,
                     BinderArgs... args) override {
    mojo::InterfaceRequest<Interface> request(std::move(handle));
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CallbackBinder::RunCallback, callback_,
                                    source_info, std::move(request), args...));
    } else {
      RunCallback(callback_, source_info, std::move(request), args...);
    }
  }

  static void RunCallback(const BindCallback& callback,
                          const BindSourceInfo& source_info,
                          mojo::InterfaceRequest<Interface> request,
                          BinderArgs... args) {
    callback.Run(source_info, std::move(request), args...);
  }

  const BindCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CallbackBinder);
};

template <typename... BinderArgs>
class GenericCallbackBinder : public InterfaceBinder<BinderArgs...> {
 public:
  using BindCallback = base::Callback<void(const BindSourceInfo&,
                                           const std::string&,
                                           mojo::ScopedMessagePipeHandle,
                                           BinderArgs...)>;

  GenericCallbackBinder(
      const BindCallback& callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  GenericCallbackBinder(
      const base::Callback<void(mojo::ScopedMessagePipeHandle, BinderArgs...)>&
          callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(base::Bind(&BindCallbackAdapter<BinderArgs...>, callback)),
        task_runner_(task_runner) {}
  ~GenericCallbackBinder() override {}

 private:
  // InterfaceBinder:
  void BindInterface(const BindSourceInfo& source_info,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle,
                     BinderArgs... args) override {
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&GenericCallbackBinder::RunCallback, callback_,
                                source_info, interface_name,
                                base::Passed(&handle), args...));
      return;
    }
    RunCallback(callback_, source_info, interface_name, std::move(handle),
                args...);
  }

  static void RunCallback(const BindCallback& callback,
                          const BindSourceInfo& source_info,
                          const std::string& interface_name,
                          mojo::ScopedMessagePipeHandle handle,
                          BinderArgs... args) {
    callback.Run(source_info, interface_name, std::move(handle), args...);
  }

  const BindCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(GenericCallbackBinder);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
