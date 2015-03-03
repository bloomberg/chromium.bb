// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_

#include <algorithm>

#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/lib/interface_ptr_internal.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
class ErrorHandler;

// A pointer to a local proxy of a remote Interface implementation. Uses a
// message pipe to communicate with the remote implementation, and automatically
// closes the pipe and deletes the proxy on destruction. The pointer must be
// bound to a message pipe before the interface methods can be called.
//
// This class is thread hostile, as is the local proxy it manages. All calls to
// this class or the proxy should be from the same thread that created it. If
// you need to move the proxy to a different thread, extract the message pipe
// using PassMessagePipe(), pass it to a different thread, and create a new
// InterfacePtr from that thread.
template <typename Interface>
class InterfacePtr {
  MOJO_MOVE_ONLY_TYPE(InterfacePtr)
 public:
  // Constructs an unbound InterfacePtr.
  InterfacePtr() {}
  InterfacePtr(decltype(nullptr)) {}

  // Takes over the binding of another InterfacePtr.
  InterfacePtr(InterfacePtr&& other) {
    internal_state_.Swap(&other.internal_state_);
  }

  // Takes over the binding of another InterfacePtr, and closes any message pipe
  // already bound to this pointer.
  InterfacePtr& operator=(InterfacePtr&& other) {
    reset();
    internal_state_.Swap(&other.internal_state_);
    return *this;
  }

  // Assigning nullptr to this class causes it to close the currently bound
  // message pipe (if any) and returns the pointer to the unbound state.
  InterfacePtr& operator=(decltype(nullptr)) {
    reset();
    return *this;
  }

  // Closes the bound message pipe (if any) on destruction.
  ~InterfacePtr() {}

  // Binds the InterfacePtr to a message pipe that is connected to a remote
  // implementation of Interface. The |waiter| is used for receiving
  // notifications when there is data to read from the message pipe. For most
  // callers, the default |waiter| will be sufficient.
  void Bind(
      ScopedMessagePipeHandle handle,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
    reset();
    internal_state_.Bind(handle.Pass(), waiter);
  }

  // Returns a raw pointer to the local proxy. Caller does not take ownership.
  // Note that the local proxy is thread hostile, as stated above.
  Interface* get() const { return internal_state_.instance(); }

  // Functions like a pointer to Interface. Must already be bound.
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  // Closes the bound message pipe (if any) and returns the pointer to the
  // unbound state.
  void reset() {
    State doomed;
    internal_state_.Swap(&doomed);
  }

  // Blocks the current thread until the next incoming response callback arrives
  // or an error occurs. Returns |true| if a response arrived, or |false| in
  // case of error.
  //
  // This method may only be called after the InterfacePtr has been bound to a
  // message pipe.
  //
  // TODO(jamesr): Rename to WaitForIncomingResponse().
  bool WaitForIncomingMethodCall() {
    return internal_state_.WaitForIncomingMethodCall();
  }

  // Indicates whether the message pipe has encountered an error. If true,
  // method calls made on this interface will be dropped (and may already have
  // been dropped).
  bool encountered_error() const { return internal_state_.encountered_error(); }

  // Registers a handler to receive error notifications. The handler will be
  // called from the thread that owns this InterfacePtr.
  //
  // This method may only be called after the InterfacePtr has been bound to a
  // message pipe.
  void set_error_handler(ErrorHandler* error_handler) {
    internal_state_.set_error_handler(error_handler);
  }

  // Unbinds the InterfacePtr and return the previously bound message pipe (if
  // any). This method may be used to move the proxy to a different thread (see
  // class comments for details).
  ScopedMessagePipeHandle PassMessagePipe() {
    State state;
    internal_state_.Swap(&state);
    return state.PassMessagePipe();
  }

  // DO NOT USE. Exposed only for internal use and for testing.
  internal::InterfacePtrState<Interface>* internal_state() {
    return &internal_state_;
  }

  // Allow InterfacePtr<> to be used in boolean expressions, but not
  // implicitly convertible to a real bool (which is dangerous).
 private:
  typedef internal::InterfacePtrState<Interface> InterfacePtr::*Testable;

 public:
  operator Testable() const {
    return internal_state_.is_bound() ? &InterfacePtr::internal_state_
                                      : nullptr;
  }

 private:
  typedef internal::InterfacePtrState<Interface> State;
  mutable State internal_state_;
};

// If the specified message pipe handle is valid, returns an InterfacePtr bound
// to it. Otherwise, returns an unbound InterfacePtr. The specified |waiter|
// will be used as in the InterfacePtr::Bind() method.
template <typename Interface>
InterfacePtr<Interface> MakeProxy(
    ScopedMessagePipeHandle handle,
    const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter()) {
  InterfacePtr<Interface> ptr;
  if (handle.is_valid())
    ptr.Bind(handle.Pass(), waiter);
  return ptr.Pass();
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_H_
