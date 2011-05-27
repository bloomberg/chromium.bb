// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "webkit/plugins/ppapi/resource.h"

struct PPB_Buffer_Dev;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Buffer_Impl : public Resource,
                        public ::ppapi::thunk::PPB_Buffer_API {
 public:
  virtual ~PPB_Buffer_Impl();

  static PP_Resource Create(PP_Instance instance, uint32_t size);

  virtual PPB_Buffer_Impl* AsPPB_Buffer_Impl();

  base::SharedMemory* shared_memory() const { return shared_memory_.get(); }
  uint32_t size() const { return size_; }

  // ResourceObjectBase overries.
  virtual ::ppapi::thunk::PPB_Buffer_API* AsBuffer_API() OVERRIDE;

  // PPB_Buffer_API implementation.
  virtual PP_Bool Describe(uint32_t* size_in_bytes) OVERRIDE;
  virtual PP_Bool IsMapped() OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;

 private:
  explicit PPB_Buffer_Impl(PluginInstance* instance);
  bool Init(uint32_t size);

  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32_t size_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Buffer_Impl);
};

// Ensures that the given buffer is mapped, and retursn it to its previous
// mapped state in the destructor.
class BufferAutoMapper {
 public:
  BufferAutoMapper(::ppapi::thunk::PPB_Buffer_API* api);
  ~BufferAutoMapper();

  // Will be NULL on failure to map.
  void* data() { return data_; }
  uint32_t size() { return size_; }

 private:
  ::ppapi::thunk::PPB_Buffer_API* api_;

  bool needs_unmap_;

  void* data_;
  uint32_t size_;

  DISALLOW_COPY_AND_ASSIGN(BufferAutoMapper);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_
