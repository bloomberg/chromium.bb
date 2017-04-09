// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedText_h
#define DetectedText_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DOMRect;

class MODULES_EXPORT DetectedText final
    : public GarbageCollectedFinalized<DetectedText>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedText* Create();
  static DetectedText* Create(String, DOMRect*);

  const String& rawValue() const;
  DOMRect* boundingBox() const;
  DECLARE_TRACE();

 private:
  DetectedText(String, DOMRect*);

  const String raw_value_;
  const Member<DOMRect> bounding_box_;
};

}  // namespace blink

#endif  // DetectedText_h
