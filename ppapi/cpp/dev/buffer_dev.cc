// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/buffer_dev.h"

#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_Buffer_Dev> buffer_f(PPB_BUFFER_DEV_INTERFACE);

}  // namespace

namespace pp {

Buffer_Dev::Buffer_Dev() : data_(NULL), size_(0) {
}

Buffer_Dev::Buffer_Dev(const Buffer_Dev& other)
    : Resource(other),
      data_(other.data_),
      size_(other.size_) {
}

Buffer_Dev::Buffer_Dev(int32_t size) : data_(NULL), size_(0) {
  if (!buffer_f)
    return;

  PassRefFromConstructor(buffer_f->Create(Module::Get()->pp_module(), size));
  if (!buffer_f->Describe(pp_resource(), &size_) ||
      !(data_ = buffer_f->Map(pp_resource())))
    *this = Buffer_Dev();
}

Buffer_Dev::~Buffer_Dev() {
}

Buffer_Dev& Buffer_Dev::operator=(const Buffer_Dev& other) {
  Buffer_Dev copy(other);
  swap(copy);
  return *this;
}

void Buffer_Dev::swap(Buffer_Dev& other) {
  Resource::swap(other);
  std::swap(size_, other.size_);
  std::swap(data_, other.data_);
}

}  // namespace pp

