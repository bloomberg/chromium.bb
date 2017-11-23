// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementConnectedCallbackReaction_h
#define CustomElementConnectedCallbackReaction_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/html/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT CustomElementConnectedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementConnectedCallbackReaction(CustomElementDefinition*);

 private:
  void Invoke(Element*) override;

  DISALLOW_COPY_AND_ASSIGN(CustomElementConnectedCallbackReaction);
};

}  // namespace blink

#endif  // CustomElementConnectedCallbackReaction_h
