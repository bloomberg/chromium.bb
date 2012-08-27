// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_CRYPTO_PPAPI_CDM_EXPORT_H_
#define WEBKIT_MEDIA_CRYPTO_PPAPI_CDM_EXPORT_H_

// Define CDM_EXPORT so that functionality implemented by the CDM module
// can be exported to consumers.

#if defined(WIN32)

#if defined(CDM_IMPLEMENTATION)
#define CDM_EXPORT __declspec(dllexport)
#else
#define CDM_EXPORT __declspec(dllimport)
#endif  // defined(CDM_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CDM_IMPLEMENTATION)
#define CDM_EXPORT __attribute__((visibility("default")))
#else
#define CDM_EXPORT
#endif
#endif

#endif  // WEBKIT_MEDIA_CRYPTO_PPAPI_CDM_EXPORT_H_
