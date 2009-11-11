// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GPU_PLUGIN_GPU_PLUGIN_H_
#define GPU_GPU_PLUGIN_GPU_PLUGIN_H_

#include "gpu/np_utils/np_headers.h"

typedef struct _NPPluginFuncs NPPluginFuncs;
typedef struct _NPNetscapeFuncs NPNetscapeFuncs;

namespace gpu_plugin {

// Declarations of NPAPI plugin entry points.

NPError NP_GetEntryPoints(NPPluginFuncs* funcs);

#if defined(OS_LINUX)
NPError NP_Initialize(NPNetscapeFuncs *browser_funcs,
                               NPPluginFuncs* plugin_funcs);
#else
NPError NP_Initialize(NPNetscapeFuncs* browser_funcs);
#endif

NPError NP_Shutdown();

}  // namespace gpu_plugin

#endif  // GPU_GPU_PLUGIN_GPU_PLUGIN_H_
