// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxIterator_h
#define NGBoxIterator_h

#include "core/layout/LayoutObject.h"

namespace blink {

class NGBox;

// NG Box iterator over siblings of the layout object.
class NGBoxIterator {
 public:
  explicit NGBoxIterator(const LayoutObject*);
  NGBox first();
  NGBox next();

 private:
  void reset();

  const LayoutObject* m_layoutObject;
  const LayoutObject* m_currentChild;
};

}  // namespace blink
#endif  // NGBoxIterator_h
