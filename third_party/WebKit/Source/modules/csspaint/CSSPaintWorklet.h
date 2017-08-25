// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintWorklet_h
#define CSSPaintWorklet_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptState;
class Worklet;

class MODULES_EXPORT CSSPaintWorklet {
  STATIC_ONLY(CSSPaintWorklet);

 public:
  static Worklet* paintWorklet(ScriptState*);
};

}  // namespace blink

#endif  // CSSPaintWorklet_h
