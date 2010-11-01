// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_resource.h"

#define PPB_IMAGEDATA_TRUSTED_INTERFACE "PPB_ImageDataTrusted;0.1"

struct PPB_ImageDataTrusted {
  /**
   * Returns the internal shared memory pointer associated with the given
   * ImageData resource. Used for proxying. Returns the handle or 0 on failure.
   */
  uint64_t (*GetNativeMemoryHandle)(PP_Resource image_data);
};

#endif  // PPAPI_C_TRUSTED_PPB_IMAGE_DATA_TRUSTED_H_
