// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBox_h
#define NGBox_h

#include "core/layout/LayoutBox.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class NGBoxIterator;
class NGConstraintSpace;
class NGFragment;

// Represents a node to be laid out.
class CORE_EXPORT NGBox final {
 public:
  explicit NGBox(LayoutObject* layoutObject)
      : m_layoutBox(toLayoutBox(layoutObject)) {}

  NGBox() : m_layoutBox(nullptr) {}

  // Returns an iterator that will iterate over this box's children, if any.
  NGBoxIterator childIterator();
  operator bool() const { return m_layoutBox; }

  // Returns true when done; when this function returns false, it has to be
  // called again. The out parameter will only be set when this function
  // returns true. The same constraint space has to be passed each time.
  // TODO(layout-ng): Should we have a StartLayout function to avoid passing
  // the same space for each Layout iteration?
  bool Layout(const NGConstraintSpace*, NGFragment**);
  const ComputedStyle* style() const;

  NGBox nextSibling() const;

  NGBox firstChild() const;

  // This is necessary for interop between old and new trees -- after our parent
  // positions us, it calls this function so we can store the position on the
  // underlying LayoutBox.
  void positionUpdated(const NGFragment&);

 private:
  bool canUseNewLayout();

  LayoutBox* m_layoutBox;
};

}  // namespace blink

#endif  // NGBox_h
