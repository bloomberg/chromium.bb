// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PluginsChangedObserver_h
#define PluginsChangedObserver_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class Page;

class CORE_EXPORT PluginsChangedObserver : public GarbageCollectedMixin {
 public:
  virtual void PluginsChanged() = 0;

 protected:
  explicit PluginsChangedObserver(Page*);
};

}  // namespace blink

#endif  // PluginsChangedObserver_h
