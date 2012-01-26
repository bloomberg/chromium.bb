// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_
#define PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_

#include <vector>

#include "base/basictypes.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

// Represents a plugin-side ArrayBufferVar. In the plugin process, it's
// owned as a vector.
class PluginArrayBufferVar : public ArrayBufferVar {
 public:
  explicit PluginArrayBufferVar(uint32 size_in_bytes);
  virtual ~PluginArrayBufferVar();

  // ArrayBufferVar implementation.
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual uint32 ByteLength() OVERRIDE;

 private:
  // TODO(dmichael): Use shared memory for this.
  std::vector<uint8> buffer_;

  DISALLOW_COPY_AND_ASSIGN(PluginArrayBufferVar);
};

}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_ARRAY_BUFFER_VAR_H_
