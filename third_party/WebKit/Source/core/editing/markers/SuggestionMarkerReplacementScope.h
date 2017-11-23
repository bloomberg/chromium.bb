// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SuggestionMarkerReplacementScope_h
#define SuggestionMarkerReplacementScope_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CORE_EXPORT SuggestionMarkerReplacementScope {
  STACK_ALLOCATED();

 public:
  SuggestionMarkerReplacementScope();
  ~SuggestionMarkerReplacementScope();

  static bool CurrentlyInScope();

 private:
  static bool currently_in_scope_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionMarkerReplacementScope);
};

}  // namespace blink

#endif
