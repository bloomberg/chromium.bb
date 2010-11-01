// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MODULE_IMPL_H_
#define PPAPI_CPP_MODULE_IMPL_H_

#include "ppapi/cpp/module.h"

namespace {

template <typename T> class DeviceFuncs {
 public:
  explicit DeviceFuncs(const char* ifname) : ifname_(ifname), funcs_(NULL) {}

  operator T const*() {
    if (!funcs_) {
      funcs_ = reinterpret_cast<T const*>(
          pp::Module::Get()->GetBrowserInterface(ifname_));
    }
    return funcs_;
  }

  // This version doesn't check for existence of the function object. It is
  // used so that, for DeviceFuncs f, the expression:
  // if (f) f->doSomething();
  // checks the existence only once.
  T const* operator->() const { return funcs_; }

 private:
  DeviceFuncs(const DeviceFuncs&other);
  DeviceFuncs &operator=(const DeviceFuncs &other);

  const char* ifname_;
  T const* funcs_;
};

}  // namespace

#endif  // PPAPI_CPP_MODULE_IMPL_H_

