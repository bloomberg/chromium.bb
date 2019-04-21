// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_INTERFACE_PROVIDER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_GPU_INTERFACE_PROVIDER_FACTORY_H_

#include <memory>

#include "content/common/content_export.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"

namespace content {

// Creates a GpuInterfaceProvider that forwards to the Gpu implementation in
// content.
CONTENT_EXPORT std::unique_ptr<ws::GpuInterfaceProvider>
CreateGpuInterfaceProvider();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_INTERFACE_PROVIDER_FACTORY_H_
