// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameClient_h
#define FrameClient_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/BlameContext.h"

namespace blink {

class Frame;
enum class FrameDetachType;

class CORE_EXPORT FrameClient : public GarbageCollectedFinalized<FrameClient> {
 public:
  virtual bool InShadowTree() const = 0;

  // TODO(dcheng): Move this into LocalFrameClient, since remote frames don't
  // need this.
  virtual void WillBeDetached() = 0;
  virtual void Detached(FrameDetachType) = 0;

  virtual Frame* Opener() const = 0;
  virtual void SetOpener(Frame*) = 0;

  virtual Frame* Parent() const = 0;
  virtual Frame* Top() const = 0;
  virtual Frame* NextSibling() const = 0;
  virtual Frame* FirstChild() const = 0;

  virtual unsigned BackForwardLength() = 0;

  virtual void FrameFocused() const = 0;

  virtual ~FrameClient() {}

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // FrameClient_h
