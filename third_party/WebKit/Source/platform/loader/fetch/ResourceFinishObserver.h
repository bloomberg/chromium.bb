// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResourceFinishObserver_h
#define ResourceFinishObserver_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

// ResourceFinishObserver is different from ResourceClient in several ways.
//  - NotifyFinished is dispatched asynchronously.
//  - ResourceFinishObservers will be removed from Resource when the load
//  finishes. - This class is not intended to be "subclassed" per each Resource
//  subclass.
//    There is no ImageResourceFinishObserver, for example.
// ResourceFinishObserver should be quite simple. All notifications must be
// notified AFTER the loading finishes.
class PLATFORM_EXPORT ResourceFinishObserver : public GarbageCollectedMixin {
 public:
  virtual ~ResourceFinishObserver() = default;

  // Called asynchronously when loading finishes.
  // Note that this can be dispatched after removing |this| client from a
  // Resource, because of the asynchronicity.
  virtual void NotifyFinished() = 0;
  // Name for debugging
  virtual String DebugName() const = 0;

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // ResourceFinishObserver_h
