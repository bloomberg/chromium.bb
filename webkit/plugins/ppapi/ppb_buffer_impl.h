// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "webkit/plugins/ppapi/resource.h"

struct PPB_Buffer_Dev;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Buffer_Impl : public Resource {
 public:
  explicit PPB_Buffer_Impl(PluginInstance* instance);
  virtual ~PPB_Buffer_Impl();

  uint32_t size() const { return size_; }
  unsigned char* mapped_buffer() const {
    return static_cast<unsigned char*>(shared_memory_->memory());
  }
  base::SharedMemoryHandle handle() const { return shared_memory_->handle(); }

  // Returns true if this buffer is mapped. False means that the buffer is
  // either invalid or not mapped.
  bool is_mapped() const { return mapped_buffer() != NULL; }

  // Returns a pointer to the interface implementing PPB_Buffer_Impl that is
  // exposed to the plugin.
  static const PPB_Buffer_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_Buffer_Impl* AsPPB_Buffer_Impl();

  // PPB_Buffer_Impl implementation.
  bool Init(uint32_t size);
  void Describe(uint32_t* size_in_bytes) const;
  void* Map();
  void Unmap();

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32_t size_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Buffer_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_
