// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FORMS_WEBKIT_FORMS_EXPORT_H_
#define WEBKIT_FORMS_WEBKIT_FORMS_EXPORT_H_
#pragma once

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBKIT_FORMS_IMPLEMENTATION)
#define WEBKIT_FORMS_EXPORT __declspec(dllexport)
#else
#define WEBKIT_FORMS_EXPORT __declspec(dllimport)
#endif  // defined(WEBKIT_FORMS_IMPLEMENTATION)

#else // defined(WIN32)
#if defined(WEBKIT_FORMS_IMPLEMENTATION)
#define WEBKIT_FORMS_EXPORT __attribute__((visibility("default")))
#else
#define WEBKIT_FORMS_EXPORT
#endif
#endif

#else // defined(COMPONENT_BUILD)
#define WEBKIT_FORMS_EXPORT
#endif

#endif  // WEBKIT_FORMS_WEBKIT_FORMS_EXPORT_H_
