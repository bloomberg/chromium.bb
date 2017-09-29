// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_EXPORT_H_
#define URL_URL_EXPORT_H_

#include "build/build_config.h"

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(URL_IMPLEMENTATION)
#define URL_EXPORT __declspec(dllexport)
#else
#define URL_EXPORT __declspec(dllimport)
#endif  // defined(URL_IMPLEMENTATION)

#define URL_EXTERN_TEMPLATE_EXTERN

#else  // !defined(WIN32)

#if defined(URL_IMPLEMENTATION)
#define URL_EXPORT __attribute__((visibility("default")))
#else
#define URL_EXPORT
#endif  // defined(URL_IMPLEMENTATION)

#define URL_EXTERN_TEMPLATE_EXTERN extern

#endif  // defined(WIN32)

#else  // !defined(COMPONENT_BUILD)

#define URL_EXPORT
#define URL_EXTERN_TEMPLATE_EXTERN extern

#endif  // define(COMPONENT_BUILD)

#if defined(URL_IMPLEMENTATION)

#if defined(COMPILER_MSVC)

#define URL_TEMPLATE_CLASS_EXPORT
#define URL_EXTERN_TEMPLATE_EXPORT URL_EXPORT
#define URL_TEMPLATE_EXPORT URL_EXPORT

#elif defined(COMPILER_GCC)

#define URL_TEMPLATE_CLASS_EXPORT URL_EXPORT
#define URL_EXTERN_TEMPLATE_EXPORT URL_EXPORT
#define URL_TEMPLATE_EXPORT

#endif

#else  // !defined(URL_IMPLEMENTATION)

#define URL_TEMPLATE_CLASS_EXPORT
#define URL_EXTERN_TEMPLATE_EXPORT URL_EXPORT
#define URL_TEMPLATE_EXPORT

#endif  // defined(URL_IMPLEMENTATION)

#endif  // URL_URL_EXPORT_H_
