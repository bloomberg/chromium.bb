// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleEngineContext_h
#define StyleEngineContext_h

#include "core/dom/Document.h"

namespace blink {

class CORE_EXPORT StyleEngineContext {
 public:
  StyleEngineContext();
  ~StyleEngineContext() {}
  bool AddedPendingSheetBeforeBody() const {
    return added_pending_sheet_before_body_;
  }
  void AddingPendingSheet(const Document&);

 private:
  bool added_pending_sheet_before_body_ : 1;
};

}  // namespace blink

#endif
