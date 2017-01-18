// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Modulator_h
#define Modulator_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;

// A Modulator is an interface for "environment settings object" concept for
// module scripts.
// https://html.spec.whatwg.org/#environment-settings-object
//
// A Modulator also serves as an entry point for various module spec algorithms.
class CORE_EXPORT Modulator : public GarbageCollectedMixin {
 public:
  static Modulator* from(LocalFrame*);
};

}  // namespace blink

#endif
