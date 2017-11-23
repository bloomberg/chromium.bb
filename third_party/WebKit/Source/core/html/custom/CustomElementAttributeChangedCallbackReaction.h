// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementAttributeChangedCallbackReaction_h
#define CustomElementAttributeChangedCallbackReaction_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/dom/QualifiedName.h"
#include "core/html/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT CustomElementAttributeChangedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementAttributeChangedCallbackReaction(CustomElementDefinition*,
                                                const QualifiedName&,
                                                const AtomicString& old_value,
                                                const AtomicString& new_value);

 private:
  void Invoke(Element*) override;

  QualifiedName name_;
  AtomicString old_value_;
  AtomicString new_value_;

  DISALLOW_COPY_AND_ASSIGN(CustomElementAttributeChangedCallbackReaction);
};

}  // namespace blink

#endif  // CustomElementAttributeChangedCallbackReaction_h
