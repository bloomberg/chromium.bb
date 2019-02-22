// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_TEXT_BLOB_H_
#define CC_PAINT_PAINT_TEXT_BLOB_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace cc {

class CC_PAINT_EXPORT PaintTextBlob
    : public base::RefCountedThreadSafe<PaintTextBlob> {
 public:
  PaintTextBlob();
  PaintTextBlob(sk_sp<SkTextBlob> blob,
                std::vector<sk_sp<SkTypeface>> typefaces);

  const sk_sp<SkTextBlob>& ToSkTextBlob() const { return sk_blob_; }
  const std::vector<sk_sp<SkTypeface>>& typefaces() const { return typefaces_; }

  operator bool() const { return !!sk_blob_; }

 private:
  friend base::RefCountedThreadSafe<PaintTextBlob>;

  ~PaintTextBlob();

  sk_sp<SkTextBlob> sk_blob_;
  std::vector<sk_sp<SkTypeface>> typefaces_;

  DISALLOW_COPY_AND_ASSIGN(PaintTextBlob);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_TEXT_BLOB_H_
