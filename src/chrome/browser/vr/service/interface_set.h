// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_INTERFACE_SET_H_
#define CHROME_BROWSER_VR_SERVICE_INTERFACE_SET_H_

#include <set>

#include "base/bind.h"
#include "base/callback.h"

namespace vr {

// Class used to store mojo InterfacePtrs. It will auto-manage its contents
// (ex. on connection error, the InterfacePtr will be removed from the set).
// The class is heavily based on mojo::InterfacePtrSet, but because
// mojo::InterfacePtrSet does not notify on removal of the interface from the
// collection, it cannot be used for some of the scenarios.
// TODO(https://crbug.com/965668) - remove this class and use InterfacePtrSet
// once it starts exposing required functionality.
template <typename InterfacePtr>
class InterfaceSet {
 public:
  ~InterfaceSet() {
    for (auto& it : interface_ptrs_) {
      it.second.reset();
    }
  }

  // Add |interface_ptr| to the set. Returns id that can be used to identify
  // added pointer. The error handler that might have been registered on
  // |interface_ptr| will be replaced. Instead, to get notified on connection
  // error, please register a connection error handler with InterfaceSet.
  size_t AddPtr(InterfacePtr interface_ptr) {
    size_t id = next_id_++;

    auto it = interface_ptrs_.emplace(id, std::move(interface_ptr));
    it.first->second.set_connection_error_handler(base::BindOnce(
        &InterfaceSet::HandleConnectionError, base::Unretained(this),
        id));  // Unretained is safe as this class owns the Ptrs.

    return id;
  }

  template <typename FunctionType>
  void ForAllPtrs(FunctionType function) {
    for (const auto& it : interface_ptrs_) {
      function(it.second.get());
    }
  }

  // Registers a function that will be called when some InterfacePtr gets
  // disconnected. When connection error is detected on one of the InterfacePtrs
  // in the collection, the registered handler will be called with an id of that
  // InterfacePtr.
  void set_connection_error_handler(
      base::RepeatingCallback<void(size_t)> error_handler) {
    error_handler_ = std::move(error_handler);
  }

 private:
  void HandleConnectionError(size_t id) {
    auto it = interface_ptrs_.find(id);

    DCHECK(it != interface_ptrs_.end());

    interface_ptrs_.erase(it);

    error_handler_.Run(id);
  }

  size_t next_id_ = 0;
  std::unordered_map<size_t, InterfacePtr> interface_ptrs_;
  base::RepeatingCallback<void(size_t)> error_handler_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_INTERFACE_SET_H_
