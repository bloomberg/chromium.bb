// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VectorMathSSE_h
#define VectorMathSSE_h

#include "platform/audio/cpu/x86/VectorMathImpl.cpp"

// Check that VectorMathImpl.cpp defined the blink::VectorMath::SSE  namespace.
static_assert(sizeof(blink::VectorMath::SSE::MType), "");

#endif  // VectorMathSSE_h
