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
  static DetectedText* create();
  static DetectedText* create(String, DOMRect*);

  const String& rawValue() const;
  DOMRect* boundingBox() const;
  DECLARE_TRACE();

 private:
  DetectedText(String, DOMRect*);

  const String m_rawValue;
  const Member<DOMRect> m_boundingBox;
};

}  // namespace blink

#endif  // DetectedText_h
