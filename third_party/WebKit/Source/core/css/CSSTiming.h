// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTiming_h
#define CSSTiming_h

#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PaintTiming;

class CSSTiming : public GarbageCollectedFinalized<CSSTiming>,
                  public Supplement<Document> {
  WTF_MAKE_NONCOPYABLE(CSSTiming);
  USING_GARBAGE_COLLECTED_MIXIN(CSSTiming);

 public:
  virtual ~CSSTiming() {}

  void recordAuthorStyleSheetParseTime(double seconds);
  void recordUpdateDuration(double seconds);

  double authorStyleSheetParseDurationBeforeFCP() const {
    return m_parseTimeBeforeFCP;
  }

  double updateDurationBeforeFCP() const { return m_updateTimeBeforeFCP; }

  static CSSTiming& from(Document&);
  DECLARE_VIRTUAL_TRACE();

 private:
  explicit CSSTiming(Document&);

  double m_parseTimeBeforeFCP = 0;
  double m_updateTimeBeforeFCP = 0;

  Member<PaintTiming> m_paintTiming;
};

}  // namespace blink

#endif  // CSSTiming_h
