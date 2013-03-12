// -*- c++ -*-
// Copyright (c) 2013 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_SUBPROCESS_SCOPED_CLEANUP_H_
#define NATIVE_CLIENT_TESTS_SUBPROCESS_SCOPED_CLEANUP_H_

#include "native_client/src/include/nacl_base.h"

template<typename T, void (*cleanup)(T)>
class ScopedNonPtr {
 public:
  explicit ScopedNonPtr(T obj)
      : obj_(obj) {}
  ~ScopedNonPtr() {
    cleanup(obj_);
  }
  operator T() const {
    return obj_;
  }
 private:
  T obj_;
  DISALLOW_COPY_AND_ASSIGN(ScopedNonPtr);
};

#endif  // NATIVE_CLIENT_TESTS_SUBPROCESS_SCOPED_CLEANUP_H_
