// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NPN_TEST_STUB_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NPN_TEST_STUB_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

// Simple implementation of subset of the NPN functions for testing.
void InitializeNPNTestStub();
void ShutdownNPNTestStub();

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NPN_TEST_STUB_H_
