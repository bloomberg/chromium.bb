// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SlotScopedTraversal_h
#define SlotScopedTraversal_h

#include "core/CoreExport.h"

namespace blink {

class Element;
class HTMLSlotElement;

class CORE_EXPORT SlotScopedTraversal {
 public:
  static HTMLSlotElement* FindScopeOwnerSlot(const Element&);
  static Element* NearestInclusiveAncestorAssignedToSlot(const Element&);
  static Element* Next(const Element&);
  static Element* Previous(const Element&);
  static Element* FirstAssignedToSlot(HTMLSlotElement&);
  static Element* LastAssignedToSlot(HTMLSlotElement&);

  static bool IsSlotScoped(const Element&);
};

}  // namespace blink

#endif
