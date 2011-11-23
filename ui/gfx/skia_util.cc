// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_util.h"

#include "base/i18n/char_iterator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/rect.h"

namespace gfx {

SkRect RectToSkRect(const gfx::Rect& rect) {
  SkRect r;
  r.set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
        SkIntToScalar(rect.right()), SkIntToScalar(rect.bottom()));
  return r;
}

gfx::Rect SkRectToRect(const SkRect& rect) {
  return gfx::Rect(static_cast<int>(rect.fLeft),
                   static_cast<int>(rect.fTop),
                   static_cast<int>(rect.width()),
                   static_cast<int>(rect.height()));
}

SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color) {
  SkColor grad_colors[2] = { start_color, end_color};
  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(start_point));
  grad_points[1].set(SkIntToScalar(0), SkIntToScalar(end_point));

  return SkGradientShader::CreateLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kRepeat_TileMode);
}

bool BitmapsAreEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2) {
  void* addr1 = NULL;
  void* addr2 = NULL;
  size_t size1 = 0;
  size_t size2 = 0;

  bitmap1.lockPixels();
  addr1 = bitmap1.getAddr32(0, 0);
  size1 = bitmap1.getSize();
  bitmap1.unlockPixels();

  bitmap2.lockPixels();
  addr2 = bitmap2.getAddr32(0, 0);
  size2 = bitmap2.getSize();
  bitmap2.unlockPixels();

  return (size1 == size2) && (0 == memcmp(addr1, addr2, bitmap1.getSize()));
}

string16 RemoveAcceleratorChar(const string16& s,
                               char16 accelerator_char,
                               int *accelerated_char_pos,
                               int *accelerated_char_span) {
  bool escaped = false;
  int last_char_pos = -1;
  int last_char_span = 0;
  base::i18n::UTF16CharIterator chars(&s);
  string16 accelerator_removed;

  accelerator_removed.reserve(s.size());
  while (!chars.end()) {
    int32 c = chars.get();
    int array_pos = chars.array_pos();
    chars.Advance();

    if (c != accelerator_char || escaped) {
      int span = chars.array_pos() - array_pos;
      if (escaped && c != accelerator_char) {
        last_char_pos = accelerator_removed.size();
        last_char_span = span;
      }
      for (int i = 0; i < span; i++)
        accelerator_removed.push_back(s[array_pos + i]);
      escaped = false;
    } else {
      escaped = true;
    }
  }

  if (accelerated_char_pos)
    *accelerated_char_pos = last_char_pos;
  if (accelerated_char_span)
    *accelerated_char_span = last_char_span;

  return accelerator_removed;
}

}  // namespace gfx
