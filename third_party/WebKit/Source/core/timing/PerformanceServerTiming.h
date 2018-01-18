// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceServerTiming_h
#define PerformanceServerTiming_h

#include "bindings/core/v8/V8ObjectBuilder.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebResourceTimingInfo.h"
#include "public/platform/WebVector.h"

namespace blink {

class ResourceTimingInfo;
class PerformanceServerTiming;

class CORE_EXPORT PerformanceServerTiming final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class ShouldAllowTimingDetails {
    Yes,
    No,
  };

  ~PerformanceServerTiming();

  const String& name() const { return name_; }
  double duration() const { return duration_; }
  const String& description() const { return description_; }

  static WebVector<WebServerTimingInfo> ParseServerTiming(
      const ResourceTimingInfo&,
      ShouldAllowTimingDetails);
  static HeapVector<Member<PerformanceServerTiming>> FromParsedServerTiming(
      const WebVector<WebServerTimingInfo>&);

  ScriptValue toJSONForBinding(ScriptState*) const;

 private:
  PerformanceServerTiming(const String& name,
                          double duration,
                          const String& description);

  const String name_;
  double duration_;
  const String description_;
};

}  // namespace blink

#endif  // PerformanceServerTiming_h
