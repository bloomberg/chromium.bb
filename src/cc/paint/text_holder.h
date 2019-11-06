// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TEXT_HOLDER_H_
#define CC_PAINT_TEXT_HOLDER_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"

namespace cc {

// The base class for embedder (blink) to associate DrawTextBlobOp with a text
// node.
// This class has to be RefCountedThreadSafe because the asscociated
// DrawTextBlobOp could be dereferenced in main, compositor or raster worker
// threads.
class CC_PAINT_EXPORT TextHolder
    : public base::RefCountedThreadSafe<TextHolder> {
 protected:
  friend class base::RefCountedThreadSafe<TextHolder>;
  virtual ~TextHolder() = default;
};

}  // namespace cc

#endif  // CC_PAINT_TEXT_HOLDER_H_
