// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontSelectorClient_h
#define FontSelectorClient_h

#include "platform/heap/Handle.h"

namespace blink {

class FontSelector;

class FontSelectorClient : public GarbageCollectedMixin {
 public:
  virtual ~FontSelectorClient() {}

  virtual void FontsNeedUpdate(FontSelector*) = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // FontSelectorClient_h
