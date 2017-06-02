// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintImages_h
#define PaintImages_h

#include <memory>
#include "core/style/StyleImage.h"
#include "platform/heap/Persistent.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"

namespace blink {

// Not to be deleted through a pointer to the base class.
class PaintImages : public Vector<Persistent<StyleImage>> {
 public:
  std::unique_ptr<PaintImages> Clone() const {
    return WTF::WrapUnique(new PaintImages(*this));
  }
};

}  // namespace blink

#endif  // PaintImages_h
