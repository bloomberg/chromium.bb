// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuggestionMarkerReplacementScope_h
#define SuggestionMarkerReplacementScope_h

#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT SuggestionMarkerReplacementScope {
  WTF_MAKE_NONCOPYABLE(SuggestionMarkerReplacementScope);
  STACK_ALLOCATED();

 public:
  SuggestionMarkerReplacementScope();
  ~SuggestionMarkerReplacementScope();

  static bool CurrentlyInScope();

 private:
  static bool currently_in_scope_;
};

}  // namespace blink

#endif
