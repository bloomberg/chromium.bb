// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSelection_h
#define WebSelection_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebSelectionBound.h"

namespace blink {

struct CompositedSelection;

// The active selection region, containing compositing data for the selection
// end points as well as metadata for the selection region.
class BLINK_EXPORT WebSelection {
 public:
  enum SelectionType { kNoSelection, kCaretSelection, kRangeSelection };

#if INSIDE_BLINK
  explicit WebSelection(const CompositedSelection&);
#endif
  WebSelection(const WebSelection&);

  const WebSelectionBound& Start() const { return start_; }
  const WebSelectionBound& end() const { return end_; }

  bool IsNone() const { return GetSelectionType() == kNoSelection; }
  bool IsCaret() const { return GetSelectionType() == kCaretSelection; }
  bool IsRange() const { return GetSelectionType() == kRangeSelection; }

 private:
  SelectionType GetSelectionType() const { return selection_type_; }

  SelectionType selection_type_;

  WebSelectionBound start_;
  WebSelectionBound end_;
};

}  // namespace blink

#endif  // WebSelection_h
