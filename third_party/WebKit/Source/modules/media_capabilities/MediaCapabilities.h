// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaCapabilities_h
#define MediaCapabilities_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Visitor.h"

namespace blink {

class MediaDecodingConfiguration;
class MediaEncodingConfiguration;
class ScriptPromise;
class ScriptState;

class MediaCapabilities final
    : public GarbageCollectedFinalized<MediaCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaCapabilities();

  ScriptPromise decodingInfo(ScriptState*, const MediaDecodingConfiguration&);
  ScriptPromise encodingInfo(ScriptState*, const MediaEncodingConfiguration&);

  virtual void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // MediaCapabilities_h
