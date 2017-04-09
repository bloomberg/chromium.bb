// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeprecatedScheduleStyleRecalcDuringLayout_h
#define DeprecatedScheduleStyleRecalcDuringLayout_h

#include "core/dom/DocumentLifecycle.h"
#include "wtf/Allocator.h"

namespace blink {

class DeprecatedScheduleStyleRecalcDuringLayout {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DeprecatedScheduleStyleRecalcDuringLayout);

 public:
  explicit DeprecatedScheduleStyleRecalcDuringLayout(DocumentLifecycle&);
  ~DeprecatedScheduleStyleRecalcDuringLayout();

 private:
  DocumentLifecycle& lifecycle_;
  DocumentLifecycle::DeprecatedTransition deprecated_transition_;
  bool was_in_perform_layout_;
};

}  // namespace blink

#endif
