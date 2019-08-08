// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_METAL_API_PROXY_H_
#define COMPONENTS_VIZ_COMMON_GPU_METAL_API_PROXY_H_

#import <Metal/Metal.h>

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"

namespace gl {
class ProgressReporter;
}  // namespace gl

// The MTLDeviceProxy wraps all calls to an MTLDevice. It reports progress
// to the GPU watchdog to prevent the watchdog from killing the GPU process
// when progress is being made.
API_AVAILABLE(macos(10.11))
@interface MTLDeviceProxy : NSObject <MTLDevice> {
  base::scoped_nsprotocol<id<MTLDevice>> device_;

  // Weak pointer to the progress reporter used to avoid watchdog timeouts.
  // This must be re-set to nullptr when it is no longer known to be valid.
  gl::ProgressReporter* progressReporter_;
}

- (id)initWithDevice:(id<MTLDevice>)device;
- (void)setProgressReporter:(gl::ProgressReporter*)progressReporter;
@end

#endif  // COMPONENTS_VIZ_COMMON_GPU_METAL_API_PROXY_H_
