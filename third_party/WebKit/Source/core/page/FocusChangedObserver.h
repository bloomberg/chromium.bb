// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FocusChangedObserver_h
#define FocusChangedObserver_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class Page;

class CORE_EXPORT FocusChangedObserver : public GarbageCollectedMixin {
 public:
  explicit FocusChangedObserver(Page*);
  virtual void FocusedFrameChanged() = 0;

 protected:
  bool IsFrameFocused(LocalFrame*);
  virtual ~FocusChangedObserver() = default;
};

}  // namespace blink

#endif  // FocusChangedObserver_h
