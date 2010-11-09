// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_resource.h"

#define PPB_IMAGEDATA_TRUSTED_INTERFACE "PPB_ImageDataTrusted;0.2"

struct PPB_ImageDataTrusted {
  /**
   * Returns the internal shared memory pointer associated with the given
   * ImageData resource. Used for proxying. Returns the handle or 0 on failure.
   * On success, the size in bytes of the shared memory region will be placed
   * into |*byte_count|.
   */
  uint64_t (*GetNativeMemoryHandle)(PP_Resource image_data,
                                    uint32_t* byte_count);
};

#endif  // PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_
