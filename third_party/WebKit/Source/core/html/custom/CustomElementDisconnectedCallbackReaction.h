// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDisconnectedCallbackReaction_h
#define CustomElementDisconnectedCallbackReaction_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/html/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT CustomElementDisconnectedCallbackReaction final
    : public CustomElementReaction {
 public:
  CustomElementDisconnectedCallbackReaction(CustomElementDefinition*);

 private:
  void Invoke(Element*) override;

  DISALLOW_COPY_AND_ASSIGN(CustomElementDisconnectedCallbackReaction);
};

}  // namespace blink

#endif  // CustomElementDisconnectedCallbackReaction_h
