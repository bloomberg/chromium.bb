// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementUpgradeReaction_h
#define CustomElementUpgradeReaction_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/html/custom/CustomElementReaction.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;

class CORE_EXPORT CustomElementUpgradeReaction final
    : public CustomElementReaction {
 public:
  CustomElementUpgradeReaction(CustomElementDefinition*);

 private:
  void Invoke(Element*) override;

  DISALLOW_COPY_AND_ASSIGN(CustomElementUpgradeReaction);
};

}  // namespace blink

#endif  // CustomElementUpgradeReaction_h
