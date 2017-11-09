// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformEventDispatcher_h
#define PlatformEventDispatcher_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {
class PlatformEventController;

class CORE_EXPORT PlatformEventDispatcher : public GarbageCollectedMixin {
 public:
  void AddController(PlatformEventController*);
  void RemoveController(PlatformEventController*);

  virtual void Trace(blink::Visitor*);

 protected:
  PlatformEventDispatcher();

  void NotifyControllers();

  virtual void StartListening() = 0;
  virtual void StopListening() = 0;

 private:
  void PurgeControllers();

  HeapHashSet<WeakMember<PlatformEventController>> controllers_;
  bool is_dispatching_;
  bool is_listening_;
};

}  // namespace blink

#endif  // PlatformEventDispatcher_h
