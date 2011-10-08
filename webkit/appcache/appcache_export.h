// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_EXPORT_H_
#define WEBKIT_APPCACHE_APPCACHE_EXPORT_H_
#pragma once

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(APPCACHE_IMPLEMENTATION)
#define APPCACHE_EXPORT __declspec(dllexport)
#else
#define APPCACHE_EXPORT __declspec(dllimport)
#endif  // defined(APPCACHE_IMPLEMENTATION)

#else // defined(WIN32)
#define APPCACHE_EXPORT __attribute__((visibility("default")))
#endif

#else // defined(COMPONENT_BUILD)
#define APPCACHE_EXPORT
#endif

#endif  // WEBKIT_APPCACHE_APPCACHE_EXPORT_H_
