// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_EXPORT_H_
#define PDF_PDF_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PDF_IMPLEMENTATION)
#define PDF_EXPORT __declspec(dllexport)
#else
#define PDF_EXPORT __declspec(dllimport)
#endif  // defined(PDF_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PDF_IMPLEMENTATION)
#define PDF_EXPORT __attribute__((visibility("default")))
#else
#define PDF_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PDF_EXPORT
#endif

#endif  // PDF_PDF_EXPORT_H_
