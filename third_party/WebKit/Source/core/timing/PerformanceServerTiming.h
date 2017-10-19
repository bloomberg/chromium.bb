// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceServerTiming_h
#define PerformanceServerTiming_h

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ResourceTimingInfo;
class PerformanceServerTiming;

using PerformanceServerTimingVector =
    HeapVector<Member<PerformanceServerTiming>>;

class CORE_EXPORT PerformanceServerTiming final
    : public GarbageCollectedFinalized<PerformanceServerTiming>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class ShouldAllowTimingDetails {
    Yes,
    No,
  };

  PerformanceServerTiming(const String& name,
                          double duration,
                          const String& description,
                          ShouldAllowTimingDetails);
  ~PerformanceServerTiming();

  String name() const;
  double duration() const;
  String description() const;

  static PerformanceServerTimingVector ParseServerTiming(
      const ResourceTimingInfo&,
      ShouldAllowTimingDetails);

  ScriptValue toJSONForBinding(ScriptState*) const;

  virtual void Trace(blink::Visitor* visitor) {}

 private:
  const String name_;
  double duration_;
  const String description_;
  ShouldAllowTimingDetails shouldAllowTimingDetails_;
};

}  // namespace blink

#endif  // PerformanceServerTiming_h
