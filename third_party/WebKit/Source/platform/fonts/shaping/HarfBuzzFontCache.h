// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HarfBuzzFontCache_h
#define HarfBuzzFontCache_h

#include <memory>
#include "platform/fonts/UnicodeRangeSet.h"

struct hb_font_t;
struct hb_face_t;

namespace blink {

class SimpleFontData;

struct HbFontDeleter {
  void operator()(hb_font_t* font);
};

using HbFontUniquePtr = std::unique_ptr<hb_font_t, HbFontDeleter>;

struct HbFaceDeleter {
  void operator()(hb_face_t* face);
};

using HbFaceUniquePtr = std::unique_ptr<hb_face_t, HbFaceDeleter>;

// struct to carry user-pointer data for hb_font_t callback functions.
struct HarfBuzzFontData {
  USING_FAST_MALLOC(HarfBuzzFontData);
  WTF_MAKE_NONCOPYABLE(HarfBuzzFontData);

 public:
  HarfBuzzFontData()
      : paint_(), simple_font_data_(nullptr), range_set_(nullptr) {}

  ~HarfBuzzFontData() {
    if (simple_font_data_)
      FontCache::GetFontCache()->ReleaseFontData(simple_font_data_);
  }

  void UpdateSimpleFontData(FontPlatformData* platform_data) {
    SimpleFontData* simple_font_data =
        FontCache::GetFontCache()
            ->FontDataFromFontPlatformData(platform_data)
            .get();
    if (simple_font_data_)
      FontCache::GetFontCache()->ReleaseFontData(simple_font_data_);
    simple_font_data_ = simple_font_data;
  }

  SkPaint paint_;
  SimpleFontData* simple_font_data_;
  scoped_refptr<UnicodeRangeSet> range_set_;
};

// Though we have FontCache class, which provides the cache mechanism for
// WebKit's font objects, we also need additional caching layer for HarfBuzz to
// reduce the number of hb_font_t objects created. Without it, we would create
// an hb_font_t object for every FontPlatformData object. But insted, we only
// need one for each unique SkTypeface.
// FIXME, crbug.com/609099: We should fix the FontCache to only keep one
// FontPlatformData object independent of size, then consider using this here.
class HbFontCacheEntry : public RefCounted<HbFontCacheEntry> {
 public:
  static scoped_refptr<HbFontCacheEntry> Create(hb_font_t* hb_font) {
    DCHECK(hb_font);
    return WTF::AdoptRef(new HbFontCacheEntry(hb_font));
  }

  hb_font_t* HbFont() { return hb_font_.get(); }
  HarfBuzzFontData* HbFontData() { return hb_font_data_.get(); }

 private:
  explicit HbFontCacheEntry(hb_font_t* font)
      : hb_font_(HbFontUniquePtr(font)),
        hb_font_data_(std::make_unique<HarfBuzzFontData>()){};

  HbFontUniquePtr hb_font_;
  std::unique_ptr<HarfBuzzFontData> hb_font_data_;
};

typedef HashMap<uint64_t,
                scoped_refptr<HbFontCacheEntry>,
                WTF::IntHash<uint64_t>,
                WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>
    HarfBuzzFontCache;

}  // namespace blink

#endif
