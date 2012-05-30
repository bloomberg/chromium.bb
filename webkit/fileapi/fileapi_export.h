// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILEAPI_EXPORT_H_
#define WEBKIT_FILEAPI_FILEAPI_EXPORT_H_
#pragma once

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(FILEAPI_IMPLEMENTATION)
#define FILEAPI_EXPORT __declspec(dllexport)
#define FILEAPI_EXPORT_PRIVATE __declspec(dllexport)
#else
#define FILEAPI_EXPORT __declspec(dllimport)
#define FILEAPI_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(FILEAPI_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(FILEAPI_IMPLEMENTATION)
#define FILEAPI_EXPORT __attribute__((visibility("default")))
#define FILEAPI_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define FILEAPI_EXPORT
#define FILEAPI_EXPORT_PRIVATE
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define FILEAPI_EXPORT
#define FILEAPI_EXPORT_PRIVATE
#endif

#endif  // WEBKIT_FILEAPI_FILEAPI_EXPORT_H_
