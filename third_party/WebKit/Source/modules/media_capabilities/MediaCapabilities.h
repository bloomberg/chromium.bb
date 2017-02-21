// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaCapabilities_h
#define MediaCapabilities_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Visitor.h"

namespace blink {

class MediaConfiguration;
class ScriptPromise;
class ScriptState;

class MediaCapabilities final
    : public GarbageCollectedFinalized<MediaCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaCapabilities();

  ScriptPromise query(ScriptState*, const MediaConfiguration&);

  DECLARE_VIRTUAL_TRACE();
};

}  // namespace blink

#endif  // MediaCapabilities_h
