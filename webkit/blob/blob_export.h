// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BLOB_BLOB_EXPORT_H_
#define WEBKIT_BLOB_BLOB_EXPORT_H_
#pragma once

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(BLOB_IMPLEMENTATION)
#define BLOB_EXPORT __declspec(dllexport)
#else
#define BLOB_EXPORT __declspec(dllimport)
#endif  // defined(BLOB_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(BLOB_IMPLEMENTATION)
#define BLOB_EXPORT __attribute__((visibility("default")))
#else
#define BLOB_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define BLOB_EXPORT
#endif

#endif  // WEBKIT_BLOB_BLOB_EXPORT_H_
