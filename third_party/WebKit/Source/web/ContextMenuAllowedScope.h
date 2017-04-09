// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContextMenuAllowedScope_h
#define ContextMenuAllowedScope_h

#include "platform/wtf/Allocator.h"

namespace blink {

class ContextMenuAllowedScope {
  STACK_ALLOCATED();

 public:
  ContextMenuAllowedScope();
  ~ContextMenuAllowedScope();

  static bool IsContextMenuAllowed();
};

}  // namespace blink

#endif  // ContextMenuAllowedScope_h
