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

  NGBoxIterator iterator();
  operator bool() const { return m_layoutBox; }

  NGFragment* layout(const NGConstraintSpace&);
  const ComputedStyle* style() const;

  NGBox nextSibling() const;

 private:
  LayoutBox* m_layoutBox;
};

}  // namespace blink

#endif  // NGBox_h
