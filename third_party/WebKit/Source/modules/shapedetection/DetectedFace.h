// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedFace_h
#define DetectedFace_h

#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class DOMRect;

class MODULES_EXPORT DetectedFace final : public GarbageCollected<DetectedFace>,
                                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedFace* Create();
  static DetectedFace* Create(DOMRect*);

  DOMRect* boundingBox() const;
  DECLARE_TRACE();

 private:
  explicit DetectedFace(DOMRect*);

  Member<DOMRect> bounding_box_;
};

}  // namespace blink

#endif  // DetectedFace_h
