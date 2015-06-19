// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _NPHOSTAPI_H_
#define _NPHOSTAPI_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

// Define an OS-neutral wrapper for shared library entry points
#if defined(OS_WIN)
#define API_CALL __stdcall
#else
#define API_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// NPAPI library entry points
//
#if defined(OS_POSIX) && !defined(OS_MACOSX)
typedef NPError (API_CALL * NP_InitializeFunc)(NPNetscapeFuncs* pNFuncs,
                                               NPPluginFuncs* pPFuncs);
#else
typedef NPError (API_CALL * NP_InitializeFunc)(NPNetscapeFuncs* pFuncs);
typedef NPError (API_CALL * NP_GetEntryPointsFunc)(NPPluginFuncs* pFuncs);
#endif
typedef NPError (API_CALL * NP_ShutdownFunc)(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _NPHOSTAPI_H_
