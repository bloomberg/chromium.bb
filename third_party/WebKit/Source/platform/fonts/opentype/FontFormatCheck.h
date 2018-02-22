// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFormatCheck_h
#define FontFormatCheck_h

#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

class FontFormatCheck {
 public:
  FontFormatCheck(sk_sp<SkData>);
  bool IsVariableFont();
  bool IsCbdtCblcColorFont();
  bool IsSbixColorFont();
  bool IsCff2OutlineFont();

  // Still needed in FontCustomPlatformData.
  static bool IsVariableFont(sk_sp<SkTypeface>);

 private:
  // hb-common.h: typedef uint32_t hb_tag_t;
  Vector<uint32_t> table_tags_;
};

}  // namespace blink

#endif
