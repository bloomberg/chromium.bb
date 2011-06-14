// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/buffer_dev.h"

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Buffer_Dev>() {
  return PPB_BUFFER_DEV_INTERFACE;
}

}  // namespace

Buffer_Dev::Buffer_Dev() : data_(NULL), size_(0) {
}

Buffer_Dev::Buffer_Dev(const Buffer_Dev& other)
    : Resource(other) {
  if (!get_interface<PPB_Buffer_Dev>()->Describe(pp_resource(), &size_) ||
      !(data_ = get_interface<PPB_Buffer_Dev>()->Map(pp_resource()))) {
    *this = Buffer_Dev();
  }
}

Buffer_Dev::Buffer_Dev(Instance* instance, uint32_t size)
    : data_(NULL),
      size_(0) {
  if (!has_interface<PPB_Buffer_Dev>())
    return;

  PassRefFromConstructor(get_interface<PPB_Buffer_Dev>()->Create(
      instance->pp_instance(), size));
  if (!get_interface<PPB_Buffer_Dev>()->Describe(pp_resource(), &size_) ||
      !(data_ = get_interface<PPB_Buffer_Dev>()->Map(pp_resource()))) {
    *this = Buffer_Dev();
  }
}

Buffer_Dev::~Buffer_Dev() {
  get_interface<PPB_Buffer_Dev>()->Unmap(pp_resource());
}

}  // namespace pp
