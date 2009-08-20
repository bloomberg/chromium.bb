// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_GPU_PLUGIN_H_
#define O3D_GPU_PLUGIN_GPU_PLUGIN_H_

#include "third_party/npapi/bindings/npapi.h"

typedef struct _NPPluginFuncs NPPluginFuncs;
typedef struct _NPNetscapeFuncs NPNetscapeFuncs;

namespace o3d {
namespace gpu_plugin {

// Declarations of NPAPI plugin entry points.

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs);

#if defined(OS_LINUX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs,
                               NPPluginFuncs* plugin_funcs);
#else
NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs);
#endif

NPError API_CALL NP_Shutdown();

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_GPU_PLUGIN_H_
