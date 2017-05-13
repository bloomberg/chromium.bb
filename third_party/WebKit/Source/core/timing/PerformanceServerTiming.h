// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceServerTiming_h
#define PerformanceServerTiming_h

#include "core/timing/PerformanceEntry.h"

namespace blink {

class CORE_EXPORT PerformanceServerTiming : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PerformanceServerTiming() override;

  static PerformanceServerTiming* create(const String& name,
                                         const String& metric,
                                         double duration,
                                         const String& description) {
    return new PerformanceServerTiming(name, metric, duration, description);
  }

  String metric() const;
  String description() const;

 protected:
  void BuildJSONValue(V8ObjectBuilder&) const override;

 private:
  PerformanceServerTiming(const String& name,
                          const String& metric,
                          double duration,
                          const String& description);

  const String metric_;
  const String description_;
};

}  // namespace blink

#endif  // PerformanceServerTiming_h
