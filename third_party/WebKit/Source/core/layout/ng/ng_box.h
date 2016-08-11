// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBox_h
#define NGBox_h

#include "core/layout/ng/ng_box_iterator.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class NGConstraintSpace;
class NGFragment;

// Represents a node to be laid out.
class CORE_EXPORT NGBox final {
 public:
  explicit NGBox(const LayoutObject* layoutObject)
      : m_layoutObject(layoutObject) {}

  NGBoxIterator iterator() { return NGBoxIterator(m_layoutObject); }
  operator bool() const { return m_layoutObject; }

  NGFragment* layout(const NGConstraintSpace&);
  const ComputedStyle* style() const;

 private:
  const LayoutObject* m_layoutObject;
};

}  // namespace blink

#endif  // NGBox_h
