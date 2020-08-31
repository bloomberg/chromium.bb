// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LARGEST_CONTENTFUL_PAINT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LARGEST_CONTENTFUL_PAINT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

// Exposes the Largest Contenful Paint, computed as described in
// https://github.com/WICG/LargestContentfulPaint.
class CORE_EXPORT LargestContentfulPaint final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LargestContentfulPaint(double start_time,
                         base::TimeDelta render_time,
                         uint64_t size,
                         base::TimeDelta load_time,
                         const AtomicString& id,
                         const String& url,
                         Element*);
  ~LargestContentfulPaint() override;

  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  uint64_t size() const { return size_; }
  DOMHighResTimeStamp renderTime() const {
    return render_time_.InMillisecondsF();
  }
  DOMHighResTimeStamp loadTime() const { return load_time_.InMillisecondsF(); }
  const AtomicString& id() const { return id_; }
  const String& url() const { return url_; }
  Element* element() const;

  void Trace(Visitor*) override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  uint64_t size_;
  base::TimeDelta render_time_;
  base::TimeDelta load_time_;
  AtomicString id_;
  String url_;
  WeakMember<Element> element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LARGEST_CONTENTFUL_PAINT_H_
