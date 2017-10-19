// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReportBody_h
#define ReportBody_h

#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT ReportBody : public GarbageCollectedFinalized<ReportBody>,
                               public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~ReportBody() {}

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // ReportBody_h
