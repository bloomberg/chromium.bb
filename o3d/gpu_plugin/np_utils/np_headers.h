// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_HEADERS_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_HEADERS_H_

// This is a hack to work around the differing definitions of NPString in the
// Chrome and O3D NPAPI headers.
#define utf8characters UTF8Characters
#define utf8length UTF8Length

#if defined(O3D_IN_CHROME)
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#else
#include "o3d/third_party/npapi/include/npapi.h"
#include "o3d/third_party/npapi/include/npruntime.h"
#endif

// Deliberately not including a directory name because Chromium and O3D put
// these headers in different directories.
//#include "npapi.h"
//#include "npruntime.h"
#undef utf8characters
#undef utf8length

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_HEADERS_H_
