/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PRIVATE_PPB_GPU_BLACKLIST_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_GPU_BLACKLIST_PRIVATE_H_

#define PPB_GPU_BLACKLIST_INTERFACE "PPB_GpuBlacklist_Private;0.1"

struct PPB_GpuBlacklist_Private {
  /*
   * IsGpuBlacklisted is a pointer to a function which answers if untrusted
   * NativeClient applications can access Pepper 3D or not.
   */
  bool (*IsGpuBlacklisted)();
};

#endif  /* PPAPI_C_PRIVATE_PPB_GPU_BLACKLIST_PRIVATE_H_ */

