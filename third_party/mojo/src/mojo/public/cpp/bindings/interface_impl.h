// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_IMPL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// DEPRECATED! Please use mojo::Binding instead of InterfaceImpl<> in new code.
//
// InterfaceImpl<..> is designed to be the base class of an interface
// implementation. It may be bound to a pipe or a proxy, see BindToPipe and
// BindToProxy.
template <typename Interface>
class InterfaceImpl : public Interface, public ErrorHandler {
 public:
  using ImplementedInterface = Interface;

  InterfaceImpl() : binding_(this), error_handler_impl_(this) {
    binding_.set_error_handler(&error_handler_impl_);
  }
  virtual ~InterfaceImpl() {}

  void BindToHandle(
      ScopedMessagePipeHandle handle,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    binding_.Bind(handle.Pass(), waiter);
  }

  bool WaitForIncomingMethodCall() {
    return binding_.WaitForIncomingMethodCall();
  }

  internal::Router* internal_router() { return binding_.internal_router(); }

  // Implements ErrorHandler.
  //
  // Called when the underlying pipe is closed. After this point no more
  // calls will be made on Interface and all responses will be silently ignored.
  void OnConnectionError() override {}

  void set_delete_on_error(bool delete_on_error) {
    error_handler_impl_.set_delete_on_error(delete_on_error);
  }

 private:
  class ErrorHandlerImpl : public ErrorHandler {
   public:
    explicit ErrorHandlerImpl(InterfaceImpl* impl) : impl_(impl) {}
    ~ErrorHandlerImpl() override {}

    // ErrorHandler implementation:
    void OnConnectionError() override {
      // If the the instance is not bound to the pipe, the instance might choose
      // to delete the binding in the OnConnectionError handler, which would in
      // turn delete |this|.  Save the error behavior before invoking the error
      // handler so we can correctly decide what to do.
      bool delete_on_error = delete_on_error_;
      impl_->OnConnectionError();
      if (delete_on_error)
        delete impl_;
    }

    void set_delete_on_error(bool delete_on_error) {
      delete_on_error_ = delete_on_error;
    }

   private:
    InterfaceImpl* impl_;
    bool delete_on_error_ = false;

    MOJO_DISALLOW_COPY_AND_ASSIGN(ErrorHandlerImpl);
  };

  Binding<Interface> binding_;
  ErrorHandlerImpl error_handler_impl_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceImpl);
};

// Takes an instance of an InterfaceImpl<..> subclass and binds it to the given
// MessagePipe. The instance is returned for convenience in member initializer
// lists, etc.
//
// If the pipe is closed, the instance's OnConnectionError method will be called
// and then the instance will be deleted.
//
// The instance is also bound to the current thread. Its methods will only be
// called on the current thread, and if the current thread exits, then the end
// point of the pipe will be closed and the error handler's OnConnectionError
// method will be called.
template <typename Impl>
Impl* BindToPipe(
    Impl* instance,
    ScopedMessagePipeHandle handle,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  instance->set_delete_on_error(true);
  instance->BindToHandle(handle.Pass(), waiter);
  return instance;
}

// Like BindToPipe but does not delete the instance after a channel error.
template <typename Impl>
Impl* WeakBindToPipe(
    Impl* instance,
    ScopedMessagePipeHandle handle,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  instance->BindToHandle(handle.Pass(), waiter);
  return instance;
}

// Takes an instance of an InterfaceImpl<..> subclass and binds it to the given
// InterfacePtr<..>. The instance is returned for convenience in member
// initializer lists, etc. If the pipe is closed, the instance's
// OnConnectionError method will be called and then the instance will be
// deleted.
//
// The instance is also bound to the current thread. Its methods will only be
// called on the current thread, and if the current thread exits, then it will
// also be deleted, and along with it, its end point of the pipe will be closed.
template <typename Impl, typename Interface>
Impl* BindToProxy(
    Impl* instance,
    InterfacePtr<Interface>* ptr,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  instance->set_delete_on_error(true);
  WeakBindToProxy(instance, ptr, waiter);
  return instance;
}

// Like BindToProxy but does not delete the instance after a channel error.
template <typename Impl, typename Interface>
Impl* WeakBindToProxy(
    Impl* instance,
    InterfacePtr<Interface>* ptr,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  MessagePipe pipe;
  ptr->Bind(pipe.handle0.Pass(), waiter);
  instance->BindToHandle(pipe.handle1.Pass(), waiter);
  return instance;
}

// Takes an instance of an InterfaceImpl<..> subclass and binds it to the given
// InterfaceRequest<..>. The instance is returned for convenience in member
// initializer lists, etc. If the pipe is closed, the instance's
// OnConnectionError method will be called and then the instance will be
// deleted.
//
// The instance is also bound to the current thread. Its methods will only be
// called on the current thread, and if the current thread exits, then it will
// also be deleted, and along with it, its end point of the pipe will be closed.
template <typename Impl, typename Interface>
Impl* BindToRequest(
    Impl* instance,
    InterfaceRequest<Interface>* request,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  return BindToPipe(instance, request->PassMessagePipe(), waiter);
}

// Like BindToRequest but does not delete the instance after a channel error.
template <typename Impl, typename Interface>
Impl* WeakBindToRequest(
    Impl* instance,
    InterfaceRequest<Interface>* request,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  return WeakBindToPipe(instance, request->PassMessagePipe(), waiter);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_IMPL_H_
