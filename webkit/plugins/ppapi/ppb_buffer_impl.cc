// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_buffer_impl.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id, uint32_t size) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Buffer_Impl> buffer(new PPB_Buffer_Impl(instance));
  if (!buffer->Init(size))
    return 0;

  return buffer->GetReference();
}

PP_Bool IsPPB_Buffer_Impl(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Buffer_Impl>(resource));
}

PP_Bool Describe(PP_Resource resource, uint32_t* size_in_bytes) {
  scoped_refptr<PPB_Buffer_Impl> buffer(
      Resource::GetAs<PPB_Buffer_Impl>(resource));
  if (!buffer)
    return PP_FALSE;
  buffer->Describe(size_in_bytes);
  return PP_TRUE;
}

void* Map(PP_Resource resource) {
  scoped_refptr<PPB_Buffer_Impl> buffer(
      Resource::GetAs<PPB_Buffer_Impl>(resource));
  if (!buffer)
    return NULL;
  return buffer->Map();
}

void Unmap(PP_Resource resource) {
  scoped_refptr<PPB_Buffer_Impl> buffer(
      Resource::GetAs<PPB_Buffer_Impl>(resource));
  if (!buffer)
    return;
  return buffer->Unmap();
}

const PPB_Buffer_Dev ppb_buffer = {
  &Create,
  &IsPPB_Buffer_Impl,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

PPB_Buffer_Impl::PPB_Buffer_Impl(PluginInstance* instance)
    : Resource(instance),
      size_(0) {
}

PPB_Buffer_Impl::~PPB_Buffer_Impl() {
}

// static
const PPB_Buffer_Dev* PPB_Buffer_Impl::GetInterface() {
  return &ppb_buffer;
}

PPB_Buffer_Impl* PPB_Buffer_Impl::AsPPB_Buffer_Impl() {
  return this;
}

bool PPB_Buffer_Impl::Init(uint32_t size) {
  if (size == 0)
    return false;
  Unmap();
  size_ = size;
  return true;
}

void PPB_Buffer_Impl::Describe(uint32_t* size_in_bytes) const {
  *size_in_bytes = size_;
}

void* PPB_Buffer_Impl::Map() {
  if (size_ == 0)
    return NULL;

  if (!is_mapped()) {
    mem_buffer_.reset(new unsigned char[size_]);
    memset(mem_buffer_.get(), 0, size_);
  }
  return mem_buffer_.get();
}

void PPB_Buffer_Impl::Unmap() {
  mem_buffer_.reset();
}

}  // namespace ppapi
}  // namespace webkit

