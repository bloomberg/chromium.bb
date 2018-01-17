// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLayoutWorklet_h
#define CSSLayoutWorklet_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptState;
class Worklet;

class CORE_EXPORT CSSLayoutWorklet {
  STATIC_ONLY(CSSLayoutWorklet);

 public:
  static Worklet* layoutWorklet(ScriptState*);
};

}  // namespace blink

#endif  // CSSLayoutWorklet_h
