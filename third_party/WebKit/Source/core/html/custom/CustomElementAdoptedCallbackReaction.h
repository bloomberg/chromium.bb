// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementAdoptedCallbackReaction_h
#define CustomElementAdoptedCallbackReaction_h

#include "core/CoreExport.h"
#include "core/html/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Document;

class CORE_EXPORT CustomElementAdoptedCallbackReaction final
    : public CustomElementReaction {
  WTF_MAKE_NONCOPYABLE(CustomElementAdoptedCallbackReaction);

 public:
  CustomElementAdoptedCallbackReaction(CustomElementDefinition*,
                                       Document* old_owner,
                                       Document* new_owner);

  virtual void Trace(blink::Visitor*);

 private:
  void Invoke(Element*) override;

  Member<Document> old_owner_;
  Member<Document> new_owner_;
};

}  // namespace blink

#endif  // CustomElementAdoptedCallbackReaction_h
