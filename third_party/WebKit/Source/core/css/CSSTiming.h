// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTiming_h
#define CSSTiming_h

#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PaintTiming;

class CSSTiming : public GarbageCollectedFinalized<CSSTiming>,
                  public Supplement<Document> {
  WTF_MAKE_NONCOPYABLE(CSSTiming);
  USING_GARBAGE_COLLECTED_MIXIN(CSSTiming);

 public:
  virtual ~CSSTiming() {}

  void RecordAuthorStyleSheetParseTime(double seconds);
  void RecordUpdateDuration(double seconds);

  double AuthorStyleSheetParseDurationBeforeFCP() const {
    return parse_time_before_fcp_;
  }

  double UpdateDurationBeforeFCP() const { return update_time_before_fcp_; }

  static CSSTiming& From(Document&);
  virtual void Trace(blink::Visitor*);

 private:
  explicit CSSTiming(Document&);

  double parse_time_before_fcp_ = 0;
  double update_time_before_fcp_ = 0;

  Member<PaintTiming> paint_timing_;
};

}  // namespace blink

#endif  // CSSTiming_h
