// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_WEBKIT_PLUGINS_PPAPI_HOST_ARRAY_BUFFER_VAR_H_
#define PPAPI_WEBKIT_PLUGINS_PPAPI_HOST_ARRAY_BUFFER_VAR_H_

#include "base/shared_memory.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"

namespace webkit {
namespace ppapi {

// Represents a host-side ArrayBufferVar.
class HostArrayBufferVar : public ::ppapi::ArrayBufferVar {
 public:
  explicit HostArrayBufferVar(uint32 size_in_bytes);
  explicit HostArrayBufferVar(const WebKit::WebArrayBuffer& buffer);
  explicit HostArrayBufferVar(uint32 size_in_bytes,
                              base::SharedMemoryHandle handle);
  virtual ~HostArrayBufferVar();

  // ArrayBufferVar implementation.
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 ByteLength() OVERRIDE;
  virtual bool CopyToNewShmem(
      PP_Instance instance,
      int* host_shm_handle_id,
      base::SharedMemoryHandle* plugin_shm_handle) OVERRIDE;

  WebKit::WebArrayBuffer& webkit_buffer() { return buffer_; }

 private:
  WebKit::WebArrayBuffer buffer_;
  // Tracks whether the data in the buffer is valid.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(HostArrayBufferVar);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // PPAPI_WEBKIT_PLUGINS_PPAPI_HOST_ARRAY_BUFFER_VAR_H_
