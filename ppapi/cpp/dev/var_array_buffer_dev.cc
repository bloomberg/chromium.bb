// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/var_array_buffer_dev.h"

#include "ppapi/c/dev/ppb_var_array_buffer_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarArrayBuffer_Dev>() {
  return PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE;
}

}  // namespace

VarArrayBuffer_Dev::VarArrayBuffer_Dev(const Var& var)
    : Var(var), buffer_(NULL) {
  if (var.is_array_buffer()) {
    buffer_ = Map();
  } else {
    PP_NOTREACHED();
    var_ = PP_MakeNull();
  }
}

VarArrayBuffer_Dev::VarArrayBuffer_Dev(uint32_t size_in_bytes)
    : buffer_(NULL) {
  if (has_interface<PPB_VarArrayBuffer_Dev>()) {
    var_ = get_interface<PPB_VarArrayBuffer_Dev>()->Create(size_in_bytes);
    buffer_ = Map();
  } else {
    PP_NOTREACHED();
    var_ = PP_MakeNull();
  }
}

uint32_t VarArrayBuffer_Dev::ByteLength() const {
  if (has_interface<PPB_VarArrayBuffer_Dev>()) {
    return get_interface<PPB_VarArrayBuffer_Dev>()->ByteLength(var_);
  }
  PP_NOTREACHED();
  return 0;
}

const void* VarArrayBuffer_Dev::Map() const {
  return const_cast<VarArrayBuffer_Dev*>(this)->Map();
}

void* VarArrayBuffer_Dev::Map() {
  // TODO(dmichael): This is not thread-safe.
  if (buffer_)
    return buffer_;
  if (has_interface<PPB_VarArrayBuffer_Dev>()) {
    buffer_ = get_interface<PPB_VarArrayBuffer_Dev>()->Map(var_);
    return buffer_;
  }
  PP_NOTREACHED();
  return 0;
}

}  // namespace pp
