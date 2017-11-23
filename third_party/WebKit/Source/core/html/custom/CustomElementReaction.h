// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReaction_h
#define CustomElementReaction_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class CustomElementDefinition;
class Element;

class CORE_EXPORT CustomElementReaction
    : public GarbageCollectedFinalized<CustomElementReaction> {
 public:
  CustomElementReaction(CustomElementDefinition*);
  virtual ~CustomElementReaction() = default;

  virtual void Invoke(Element*) = 0;

  virtual void Trace(blink::Visitor*);

 protected:
  Member<CustomElementDefinition> definition_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementReaction);
};

}  // namespace blink

#endif  // CustomElementReaction_h
