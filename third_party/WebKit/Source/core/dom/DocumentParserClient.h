// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentParserClient_h
#define DocumentParserClient_h

#include "platform/heap/Handle.h"

namespace blink {

class DocumentParserClient : public GarbageCollectedMixin {
 public:
  // This callback is called when all data pushed to parser has been consumed.
  virtual void NotifyParserStopped() = 0;

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  DocumentParserClient() = default;
};

}  // namespace blink

#endif  // DocumentParserClient_h
