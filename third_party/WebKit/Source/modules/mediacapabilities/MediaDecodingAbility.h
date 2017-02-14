// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDecodingAbility_h
#define MediaDecodingAbility_h

#include "bindings/core/v8/ScriptWrappable.h"

namespace blink {

// Implementation of the MediaDecodingAbility interface.
class MediaDecodingAbility final
    : public GarbageCollected<MediaDecodingAbility>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaDecodingAbility();

  bool supported() const;
  bool smooth() const;
  bool powerEfficient() const;

  DECLARE_VIRTUAL_TRACE();
};

}  // namespace blink

#endif  // MediaDecodingAbility_h
