// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(PrecompileCore_h_)
#error You shouldn't include the precompiled header file more than once.
#endif
#define PrecompileCore_h_

#include "build/win/Precompile.h"

// In Blink a lot of operations center around dom and Document, or around
// layout/rendering and LayoutObject. Those two headers are in turn pulling
// in large parts of Blink's other headers which means that every compilation
// unit is compiling large parts of Blink. By precompiling Document.h
// and LayoutObject.h we only have to compile those parts once rather
// than 1500 times. It can make a large difference in compilation
// times (3-4 times faster).
#include "core/dom/Document.h"
#include "core/layout/LayoutObject.h"
