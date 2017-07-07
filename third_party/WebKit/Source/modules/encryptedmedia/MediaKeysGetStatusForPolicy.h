// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysGetStatusForPolicy_h
#define MediaKeysGetStatusForPolicy_h

#include "bindings/core/v8/ScriptPromise.h"

namespace blink {

class MediaKeys;
class MediaKeysPolicy;
class ScriptState;

class MediaKeysGetStatusForPolicy {
 public:
  static ScriptPromise getStatusForPolicy(ScriptState*,
                                          MediaKeys&,
                                          MediaKeysPolicy*);
};

}  // namespace blink

#endif  // MediaKeysGetStatusForPolicy_h
