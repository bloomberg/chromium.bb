// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_NVML_INFO_H_
#define GPU_CONFIG_NVML_INFO_H_

#include "build/build_config.h"

#include <stdint.h>
#include <string>

// Queries driver version and places it into driver_version. Writes
// major_cuda_compute_capability and minor_cuda_compute_capability of the GPU
// matching pci_device_id if one is found, or writes 0 into these outputs
// otherwise. Returns true on success.
bool GetNvmlDeviceInfo(uint32_t pci_device_id,
                       std::string* driver_version,
                       int* major_cuda_compute_capability,
                       int* minor_cuda_compute_capability);

#endif  // GPU_CONFIG_NVML_INFO_H_