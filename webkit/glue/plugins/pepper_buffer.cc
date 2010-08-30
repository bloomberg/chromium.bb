// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_buffer.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/dev/ppb_buffer_dev.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_module.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"

namespace pepper {

namespace {

PP_Resource Create(PP_Module module_id, int32_t size) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;

  scoped_refptr<Buffer> buffer(new Buffer(module));
  if (!buffer->Init(size))
    return 0;

  return buffer->GetReference();
}

bool IsBuffer(PP_Resource resource) {
  return !!Resource::GetAs<Buffer>(resource);
}

bool Describe(PP_Resource resource, int32_t* size_in_bytes) {
  scoped_refptr<Buffer> buffer(Resource::GetAs<Buffer>(resource));
  if (!buffer)
    return false;
  buffer->Describe(size_in_bytes);
  return true;
}

void* Map(PP_Resource resource) {
  scoped_refptr<Buffer> buffer(Resource::GetAs<Buffer>(resource));
  if (!buffer)
    return NULL;
  return buffer->Map();
}

void Unmap(PP_Resource resource) {
  scoped_refptr<Buffer> buffer(Resource::GetAs<Buffer>(resource));
  if (!buffer)
    return;
  return buffer->Unmap();
}

const PPB_Buffer_Dev ppb_buffer = {
  &Create,
  &IsBuffer,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

Buffer::Buffer(PluginModule* module)
    : Resource(module),
      size_(0) {
}

Buffer::~Buffer() {
}

// static
const PPB_Buffer_Dev* Buffer::GetInterface() {
  return &ppb_buffer;
}

bool Buffer::Init(int size) {
  if (size == 0)
    return false;
  Unmap();
  size_ = size;
  return true;
}

void Buffer::Describe(int* size_in_bytes) const {
  *size_in_bytes = size_;
}

void* Buffer::Map() {
  if (size_ == 0)
    return NULL;

  if (!is_mapped()) {
    mem_buffer_.reset(new unsigned char[size_]);
    memset(mem_buffer_.get(), 0, size_);
  }
  return mem_buffer_.get();
}

void Buffer::Unmap() {
  mem_buffer_.reset();
}

void Buffer::Swap(Buffer* other) {
  swap(other->mem_buffer_, mem_buffer_);
  std::swap(other->size_, size_);
}

}  // namespace pepper

