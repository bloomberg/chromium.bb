// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_buffer_impl.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ::ppapi::thunk::PPB_Buffer_API;

namespace webkit {
namespace ppapi {

PPB_Buffer_Impl::PPB_Buffer_Impl(PluginInstance* instance)
    : Resource(instance), size_(0) {
}

PPB_Buffer_Impl::~PPB_Buffer_Impl() {
}

// static
PP_Resource PPB_Buffer_Impl::Create(PP_Instance pp_instance, uint32_t size) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0;
  scoped_refptr<PPB_Buffer_Impl> buffer(new PPB_Buffer_Impl(instance));
  if (!buffer->Init(size))
    return 0;
  return buffer->GetReference();
}

PPB_Buffer_Impl* PPB_Buffer_Impl::AsPPB_Buffer_Impl() {
  return this;
}

PPB_Buffer_API* PPB_Buffer_Impl::AsBuffer_API() {
  return this;
}

bool PPB_Buffer_Impl::Init(uint32_t size) {
  if (size == 0 || !instance())
    return false;
  size_ = size;
  shared_memory_.reset(
      instance()->delegate()->CreateAnonymousSharedMemory(size));
  return shared_memory_.get() != NULL;
}

PP_Bool PPB_Buffer_Impl::Describe(uint32_t* size_in_bytes) {
  *size_in_bytes = size_;
  return PP_TRUE;
}

PP_Bool PPB_Buffer_Impl::IsMapped() {
  return PP_FromBool(!!shared_memory_->memory());
}

void* PPB_Buffer_Impl::Map() {
  DCHECK(size_);
  DCHECK(shared_memory_.get());
  if (!shared_memory_->Map(size_))
    return NULL;
  return shared_memory_->memory();
}

void PPB_Buffer_Impl::Unmap() {
  shared_memory_->Unmap();
}

BufferAutoMapper::BufferAutoMapper(PPB_Buffer_API* api) : api_(api) {
  needs_unmap_ = !PP_ToBool(api->IsMapped());
  data_ = api->Map();
  api->Describe(&size_);
}

BufferAutoMapper::~BufferAutoMapper() {
  if (needs_unmap_)
    api_->Unmap();
}

}  // namespace ppapi
}  // namespace webkit
