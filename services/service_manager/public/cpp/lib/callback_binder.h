// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CALLBACK_BINDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CALLBACK_BINDER_H_

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_binder.h"

namespace service_manager {
namespace internal {

template <typename Interface>
class CallbackBinderWithSourceInfo : public InterfaceBinder {
 public:
  using BindCallback = base::Callback<void(const BindSourceInfo&,
                                           mojo::InterfaceRequest<Interface>)>;

  CallbackBinderWithSourceInfo(
      const BindCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  ~CallbackBinderWithSourceInfo() override {}

 private:
  // InterfaceBinder:
  void BindInterface(const BindSourceInfo& source_info,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle) override {
    mojo::InterfaceRequest<Interface> request =
        mojo::MakeRequest<Interface>(std::move(handle));
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&CallbackBinderWithSourceInfo::RunCallback, callback_,
                     source_info, base::Passed(&request)));
    } else {
      RunCallback(callback_, source_info, std::move(request));
    }
  }

  static void RunCallback(const BindCallback& callback,
                          const BindSourceInfo& source_info,
                          mojo::InterfaceRequest<Interface> request) {
    callback.Run(source_info, std::move(request));
  }

  const BindCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CallbackBinderWithSourceInfo);
};

template <typename Interface>
class CallbackBinder : public InterfaceBinder {
 public:
  using BindCallback = base::Callback<void(mojo::InterfaceRequest<Interface>)>;

  CallbackBinder(const BindCallback& callback,
                 const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  ~CallbackBinder() override {}

 private:
  // InterfaceBinder:
  void BindInterface(const BindSourceInfo& source_info,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle) override {
    mojo::InterfaceRequest<Interface> request =
        mojo::MakeRequest<Interface>(std::move(handle));
    if (task_runner_) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&CallbackBinder::RunCallback, callback_,
                                        base::Passed(&request)));
    } else {
      RunCallback(callback_, std::move(request));
    }
  }

  static void RunCallback(const BindCallback& callback,
                          mojo::InterfaceRequest<Interface> request) {
    callback.Run(std::move(request));
  }

  const BindCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CallbackBinder);
};

class GenericCallbackBinder : public InterfaceBinder {
 public:
  using BindCallback = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

  GenericCallbackBinder(
      const BindCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~GenericCallbackBinder() override;

 private:
  // InterfaceBinder:
  void BindInterface(const BindSourceInfo& source_info,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle) override;

  static void RunCallback(const BindCallback& callback,
                          mojo::ScopedMessagePipeHandle client_handle);

  const BindCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(GenericCallbackBinder);
};

}  // namespace internal
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LIB_CALLBACK_BINDER_H_
