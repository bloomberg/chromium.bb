// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/var_array_buffer_dev.h"

#include <limits>

#include "ppapi/c/dev/ppb_var_array_buffer_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarArrayBuffer_Dev>() {
  return PPB_VAR_ARRAY_BUFFER_DEV_INTERFACE;
}

}  // namespace

VarArrayBuffer_Dev::VarArrayBuffer_Dev(const Var& var) : Var(var) {
  if (!var.is_array_buffer()) {
    PP_NOTREACHED();
    var_ = PP_MakeNull();
  }
}

VarArrayBuffer_Dev::VarArrayBuffer_Dev(uint32_t size_in_bytes) {
  if (has_interface<PPB_VarArrayBuffer_Dev>()) {
    var_ = get_interface<PPB_VarArrayBuffer_Dev>()->Create(size_in_bytes);
    needs_release_ = true;
  } else {
    PP_NOTREACHED();
    var_ = PP_MakeNull();
  }
}

pp::VarArrayBuffer_Dev& VarArrayBuffer_Dev::operator=(
    const VarArrayBuffer_Dev& other) {
  Var::operator=(other);
  return *this;
}

pp::Var& VarArrayBuffer_Dev::operator=(const Var& other) {
  if (other.is_array_buffer()) {
    return Var::operator=(other);
  } else {
    PP_NOTREACHED();
    return *this;
  }
}

uint32_t VarArrayBuffer_Dev::ByteLength() const {
  uint32_t byte_length = std::numeric_limits<uint32_t>::max();
  if (is_array_buffer() && has_interface<PPB_VarArrayBuffer_Dev>())
    get_interface<PPB_VarArrayBuffer_Dev>()->ByteLength(var_, &byte_length);
  else
    PP_NOTREACHED();
  return byte_length;
}

void* VarArrayBuffer_Dev::Map() {
  if (is_array_buffer() && has_interface<PPB_VarArrayBuffer_Dev>())
    return get_interface<PPB_VarArrayBuffer_Dev>()->Map(var_);
  PP_NOTREACHED();
  return NULL;
}

void VarArrayBuffer_Dev::Unmap() {
  if (is_array_buffer() && has_interface<PPB_VarArrayBuffer_Dev>())
    get_interface<PPB_VarArrayBuffer_Dev>()->Unmap(var_);
  else
    PP_NOTREACHED();
}

}  // namespace pp
