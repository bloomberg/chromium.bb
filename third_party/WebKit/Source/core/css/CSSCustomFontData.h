/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef CSSCustomFontData_h
#define CSSCustomFontData_h

#include "core/css/CSSFontFaceSource.h"
#include "platform/fonts/CustomFontData.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSCustomFontData final : public CustomFontData {
 public:
  enum FallbackVisibility { kInvisibleFallback, kVisibleFallback };

  static scoped_refptr<CSSCustomFontData> Create(
      RemoteFontFaceSource* source,
      FallbackVisibility visibility) {
    return WTF::AdoptRef(new CSSCustomFontData(source, visibility));
  }

  ~CSSCustomFontData() override {}

  bool ShouldSkipDrawing() const override {
    if (font_face_source_)
      font_face_source_->PaintRequested();
    return fallback_visibility_ == kInvisibleFallback && is_loading_;
  }

  void BeginLoadIfNeeded() const override {
    if (!is_loading_ && font_face_source_) {
      is_loading_ = true;
      font_face_source_->BeginLoadIfNeeded();
    }
  }

  bool IsLoading() const override { return is_loading_; }
  bool IsLoadingFallback() const override { return true; }
  void ClearFontFaceSource() override { font_face_source_ = 0; }

 private:
  CSSCustomFontData(RemoteFontFaceSource* source, FallbackVisibility visibility)
      : font_face_source_(source),
        fallback_visibility_(visibility),
        is_loading_(false) {
    if (source)
      is_loading_ = source->IsLoading();
  }

  // TODO(Oilpan): consider moving (Custom)FontFace hierarchy to the heap,
  // thereby making this reference a Member<>.
  WeakPersistent<RemoteFontFaceSource> font_face_source_;
  FallbackVisibility fallback_visibility_;
  mutable bool is_loading_;
};

}  // namespace blink

#endif  // CSSCustomFontData_h
