// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_BUFFER_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/shared_impl/resource.h"

namespace webkit {
namespace ppapi {

class PPB_Buffer_Impl : public ::ppapi::Resource,
                        public ::ppapi::thunk::PPB_Buffer_API,
                        public ::ppapi::thunk::PPB_BufferTrusted_API {
 public:
  virtual ~PPB_Buffer_Impl();

  static PP_Resource Create(PP_Instance instance, uint32_t size);

  virtual PPB_Buffer_Impl* AsPPB_Buffer_Impl();

  base::SharedMemory* shared_memory() const { return shared_memory_.get(); }
  uint32_t size() const { return size_; }

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_Buffer_API* AsPPB_Buffer_API() OVERRIDE;
  virtual ::ppapi::thunk::PPB_BufferTrusted_API* AsPPB_BufferTrusted_API();

  // PPB_Buffer_API implementation.
  virtual PP_Bool Describe(uint32_t* size_in_bytes) OVERRIDE;
  virtual PP_Bool IsMapped() OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;

  // PPB_BufferTrusted_API implementation.
  virtual int32_t GetSharedMemory(int* handle) OVERRIDE;

 private:
  explicit PPB_Buffer_Impl(PP_Instance instance);
  bool Init(uint32_t size);

  scoped_ptr<base::SharedMemory> shared_memory_;
  uint32_t size_;
  int map_count_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Buffer_Impl);
};

// Ensures that the given buffer is mapped, and returns it to its previous
// mapped state in the destructor.
class BufferAutoMapper {
 public:
  explicit BufferAutoMapper(::ppapi::thunk::PPB_Buffer_API* api);
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
