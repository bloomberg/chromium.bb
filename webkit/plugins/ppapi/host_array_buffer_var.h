// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_WEBKIT_PLUGINS_PPAPI_HOST_ARRAY_BUFFER_VAR_H_
#define PPAPI_WEBKIT_PLUGINS_PPAPI_HOST_ARRAY_BUFFER_VAR_H_

#include "ppapi/shared_impl/var.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"

namespace webkit {
namespace ppapi {

// Represents a host-side ArrayBufferVar.
class HostArrayBufferVar : public ::ppapi::ArrayBufferVar {
 public:
  explicit HostArrayBufferVar(uint32 size_in_bytes);
  explicit HostArrayBufferVar(const WebKit::WebArrayBuffer& buffer);
  virtual ~HostArrayBufferVar();

  // ArrayBufferVar implementation.
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 ByteLength() OVERRIDE;

  WebKit::WebArrayBuffer& webkit_buffer() { return buffer_; }

 private:
  WebKit::WebArrayBuffer buffer_;

  DISALLOW_COPY_AND_ASSIGN(HostArrayBufferVar);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // PPAPI_WEBKIT_PLUGINS_PPAPI_HOST_ARRAY_BUFFER_VAR_H_
