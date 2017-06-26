// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/CachingWordShapeIterator.h"

#include "platform/fonts/shaping/HarfBuzzShaper.h"

namespace blink {

PassRefPtr<const ShapeResult> CachingWordShapeIterator::ShapeWordWithoutSpacing(
    const TextRun& word_run,
    const Font* font) {
  ShapeCacheEntry* cache_entry = shape_cache_->Add(word_run, ShapeCacheEntry());
  if (cache_entry && cache_entry->shape_result_)
    return cache_entry->shape_result_;

  unsigned word_length = 0;
  std::unique_ptr<UChar[]> word_text = word_run.NormalizedUTF16(&word_length);

  HarfBuzzShaper shaper(word_text.get(), word_length);
  RefPtr<const ShapeResult> shape_result =
      shaper.Shape(font, word_run.Direction());
  if (!shape_result)
    return nullptr;

  if (cache_entry)
    cache_entry->shape_result_ = shape_result;

  return shape_result;
}

}  // namespace blink
