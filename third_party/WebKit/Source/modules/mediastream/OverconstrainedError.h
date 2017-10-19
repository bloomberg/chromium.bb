// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OverconstrainedError_h
#define OverconstrainedError_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class OverconstrainedError final
    : public GarbageCollectedFinalized<OverconstrainedError>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OverconstrainedError* Create(const String& constraint,
                                      const String& message);

  String name() const { return "OverconstrainedError"; }
  const String& constraint() const { return constraint_; }
  const String& message() const { return message_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  OverconstrainedError(const String& constraint, const String& message);

  String constraint_;
  String message_;
};

}  // namespace blink

#endif  // OverconstrainedError_h
