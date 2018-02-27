// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsServiceWorker_h
#define InternalsServiceWorker_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Internals;
class ScriptState;
class ServiceWorker;

class InternalsServiceWorker {
  STATIC_ONLY(InternalsServiceWorker);

 public:
  static ScriptPromise terminateServiceWorker(ScriptState*,
                                              Internals&,
                                              ServiceWorker*);
};

}  // namespace blink

#endif  // InternalsServiceWorker_h
