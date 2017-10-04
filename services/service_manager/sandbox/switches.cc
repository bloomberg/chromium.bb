// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/switches.h"

namespace service_manager {
namespace switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
const char kServiceSandboxType[] = "service-sandbox-type";

// Must be in sync with "sandbox_type" values as used in service manager's
// manifest.json catalog files.
const char kNoneSandbox[] = "none";
const char kNetworkSandbox[] = "network";
const char kPpapiSandbox[] = "ppapi";
const char kUtilitySandbox[] = "utility";
const char kCdmSandbox[] = "cdm";
const char kPdfCompositorSandbox[] = "pdf_compositor";

// Flags spied upon from other layers.
const char kGpuProcess[] = "gpu-process";
const char kPpapiBrokerProcess[] = "ppapi-broker";
const char kPpapiPluginProcess[] = "ppapi";
const char kRendererProcess[] = "renderer";
const char kUtilityProcess[] = "utility";
const char kDisableGpuSandbox[] = "disable-gpu-sandbox";
const char kNoSandbox[] = "no-sandbox";

}  // namespace switches
}  // namespace service_manager
