// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Precompiled header for WebKit when built on Windows using
 * GYP-generated project files.  Not used by other build
 * configurations.
 *
 * Using precompiled headers speeds the build up significantly.  On a
 * fast machine (HP Z600, 12 GB of RAM), an ~18% decrease in full
 * build time was measured with only the stdlib headers added. More
 * with the Blink specific headers.
 */

#if defined(PrecompileCore_h_)
#error You shouldn't include the precompiled header file more than once.
#endif

#define PrecompileCore_h_

#include "build/win/Precompile.h"

// In Blink a lot of operations center around dom and Document, or around
// layout/rendering and LayoutObject. Those two headers are in turn pulling
// in large parts of Blink's other headers which means that every compilation
// unit is compiling large parts of Blink. By precompiling Document.h and
// LayoutObject.h we only have to compile those parts once rather than
// 1500 times. It can make a large difference in compilation times.
// config.h contains some base macros that headers can expect are present.
#include "config.h"
#include "core/dom/Document.h"
#include "core/layout/LayoutObject.h"
