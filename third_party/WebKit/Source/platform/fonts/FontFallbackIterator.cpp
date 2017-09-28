// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontFallbackIterator.h"

#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/text/ICUError.h"

namespace blink {

RefPtr<FontFallbackIterator> FontFallbackIterator::Create(
    const FontDescription& description,
    RefPtr<FontFallbackList> fallback_list,
    FontFallbackPriority font_fallback_priority) {
  return WTF::AdoptRef(new FontFallbackIterator(
      description, std::move(fallback_list), font_fallback_priority));
}

FontFallbackIterator::FontFallbackIterator(
    const FontDescription& description,
    RefPtr<FontFallbackList> fallback_list,
    FontFallbackPriority font_fallback_priority)
    : font_description_(description),
      font_fallback_list_(std::move(fallback_list)),
      current_font_data_index_(0),
      segmented_face_index_(0),
      fallback_stage_(kFontGroupFonts),
      font_fallback_priority_(font_fallback_priority) {}

bool FontFallbackIterator::AlreadyLoadingRangeForHintChar(UChar32 hint_char) {
  for (auto it = tracked_loading_range_sets_.begin();
       it != tracked_loading_range_sets_.end(); ++it) {
    if ((*it)->Contains(hint_char))
      return true;
  }
  return false;
}

bool FontFallbackIterator::RangeSetContributesForHint(
    const Vector<UChar32> hint_list,
    const FontDataForRangeSet* segmented_face) {
  for (auto it = hint_list.begin(); it != hint_list.end(); ++it) {
    if (segmented_face->Contains(*it)) {
      if (!AlreadyLoadingRangeForHintChar(*it))
        return true;
    }
  }
  return false;
}

void FontFallbackIterator::WillUseRange(const AtomicString& family,
                                        const FontDataForRangeSet& range_set) {
  FontSelector* selector = font_fallback_list_->GetFontSelector();
  if (!selector)
    return;

  selector->WillUseRange(font_description_, family, range_set);
}

RefPtr<FontDataForRangeSet> FontFallbackIterator::UniqueOrNext(
    RefPtr<FontDataForRangeSet> candidate,
    const Vector<UChar32>& hint_list) {
  SkTypeface* candidate_typeface =
      candidate->FontData()->PlatformData().Typeface();
  if (!candidate_typeface)
    return Next(hint_list);

  uint32_t candidate_id = candidate_typeface->uniqueID();
  if (unique_font_data_for_range_sets_returned_.Contains(candidate_id)) {
    return Next(hint_list);
  }

  // We don't want to skip subsetted ranges because HarfBuzzShaper's behavior
  // depends on the subsetting.
  if (candidate->IsEntireRange())
    unique_font_data_for_range_sets_returned_.insert(candidate_id);
  return candidate;
}

RefPtr<FontDataForRangeSet> FontFallbackIterator::Next(
    const Vector<UChar32>& hint_list) {
  if (fallback_stage_ == kOutOfLuck)
    return WTF::AdoptRef(new FontDataForRangeSet());

  if (fallback_stage_ == kFallbackPriorityFonts) {
    // Only try one fallback priority font,
    // then proceed to regular system fallback.
    fallback_stage_ = kSystemFonts;
    RefPtr<FontDataForRangeSet> fallback_priority_font_range = WTF::AdoptRef(
        new FontDataForRangeSet(FallbackPriorityFont(hint_list[0])));
    if (fallback_priority_font_range->HasFontData())
      return UniqueOrNext(std::move(fallback_priority_font_range), hint_list);
    return Next(hint_list);
  }

  if (fallback_stage_ == kSystemFonts) {
    // We've reached pref + system fallback.
    RefPtr<SimpleFontData> system_font = UniqueSystemFontForHintList(hint_list);
    if (system_font) {
      // Fallback fonts are not retained in the FontDataCache.
      return UniqueOrNext(WTF::AdoptRef(new FontDataForRangeSet(system_font)),
                          hint_list);
    }

    // If we don't have options from the system fallback anymore or had
    // previously returned them, we only have the last resort font left.
    // TODO: crbug.com/42217 Improve this by doing the last run with a last
    // resort font that has glyphs for everything, for example the Unicode
    // LastResort font, not just Times or Arial.
    FontCache* font_cache = FontCache::GetFontCache();
    fallback_stage_ = kOutOfLuck;
    RefPtr<SimpleFontData> last_resort =
        font_cache->GetLastResortFallbackFont(font_description_).get();
    if (!last_resort)
      FontCache::CrashWithFontInfo(&font_description_);
    // Don't skip the LastResort font in uniqueOrNext() since HarfBuzzShaper
    // needs to use this one to place missing glyph boxes.
    return WTF::AdoptRef(new FontDataForRangeSetFromCache(last_resort));
  }

  DCHECK(fallback_stage_ == kFontGroupFonts ||
         fallback_stage_ == kSegmentedFace);
  const FontData* font_data = font_fallback_list_->FontDataAt(
      font_description_, current_font_data_index_);

  if (!font_data) {
    // If there is no fontData coming from the fallback list, it means
    // we are now looking at system fonts, either for prioritized symbol
    // or emoji fonts or by calling system fallback API.
    fallback_stage_ = IsNonTextFallbackPriority(font_fallback_priority_)
                          ? kFallbackPriorityFonts
                          : kSystemFonts;
    return Next(hint_list);
  }

  // Otherwise we've received a fontData from the font-family: set of fonts,
  // and a non-segmented one in this case.
  if (!font_data->IsSegmented()) {
    // Skip forward to the next font family for the next call to next().
    current_font_data_index_++;
    if (!font_data->IsLoading()) {
      RefPtr<SimpleFontData> non_segmented =
          const_cast<SimpleFontData*>(ToSimpleFontData(font_data));
      // The fontData object that we have here is tracked in m_fontList of
      // FontFallbackList and gets released in the font cache when the
      // FontFallbackList is destroyed.
      return UniqueOrNext(WTF::AdoptRef(new FontDataForRangeSet(non_segmented)),
                          hint_list);
    }
    return Next(hint_list);
  }

  // Iterate over ranges of a segmented font below.

  const SegmentedFontData* segmented = ToSegmentedFontData(font_data);
  if (fallback_stage_ != kSegmentedFace) {
    segmented_face_index_ = 0;
    fallback_stage_ = kSegmentedFace;
  }

  DCHECK_LT(segmented_face_index_, segmented->NumFaces());
  RefPtr<FontDataForRangeSet> current_segmented_face =
      segmented->FaceAt(segmented_face_index_);
  segmented_face_index_++;

  if (segmented_face_index_ == segmented->NumFaces()) {
    // Switch from iterating over a segmented face to the next family from
    // the font-family: group of fonts.
    fallback_stage_ = kFontGroupFonts;
    current_font_data_index_++;
  }

  if (RangeSetContributesForHint(hint_list, current_segmented_face.get())) {
    const SimpleFontData* font_data = current_segmented_face->FontData();
    if (const CustomFontData* custom_font_data = font_data->GetCustomFontData())
      custom_font_data->BeginLoadIfNeeded();
    if (!font_data->IsLoading())
      return UniqueOrNext(current_segmented_face, hint_list);
    tracked_loading_range_sets_.push_back(current_segmented_face);
  }

  return Next(hint_list);
}

RefPtr<SimpleFontData> FontFallbackIterator::FallbackPriorityFont(
    UChar32 hint) {
  return FontCache::GetFontCache()->FallbackFontForCharacter(
      font_description_, hint,
      font_fallback_list_->PrimarySimpleFontData(font_description_),
      font_fallback_priority_);
}

static inline unsigned ChooseHintIndex(const Vector<UChar32>& hint_list) {
  // crbug.com/618178 has a test case where no Myanmar font is ever found,
  // because the run starts with a punctuation character with a script value of
  // common. Our current font fallback code does not find a very meaningful
  // result for this.
  // TODO crbug.com/668706 - Improve this situation.
  // So if we have multiple hint characters (which indicates that a
  // multi-character grapheme or more failed to shape, then we can try to be
  // smarter and select the first character that has an actual script value.
  DCHECK(hint_list.size());
  if (hint_list.size() <= 1)
    return 0;

  ICUError err;
  UScriptCode hint_char_script = uscript_getScript(hint_list[0], &err);
  if (!U_SUCCESS(err) || hint_char_script > USCRIPT_INHERITED)
    return 0;

  for (size_t i = 1; i < hint_list.size(); ++i) {
    UScriptCode new_hint_script = uscript_getScript(hint_list[i], &err);
    if (!U_SUCCESS(err))
      return 0;
    if (new_hint_script > USCRIPT_INHERITED)
      return i;
  }
  return 0;
}

RefPtr<SimpleFontData> FontFallbackIterator::UniqueSystemFontForHintList(
    const Vector<UChar32>& hint_list) {
  // When we're asked for a fallback for the same characters again, we give up
  // because the shaper must have previously tried shaping with the font
  // already.
  if (!hint_list.size())
    return nullptr;

  FontCache* font_cache = FontCache::GetFontCache();
  UChar32 hint = hint_list[ChooseHintIndex(hint_list)];

  if (!hint || previously_asked_for_hint_.Contains(hint))
    return nullptr;
  previously_asked_for_hint_.insert(hint);
  return font_cache->FallbackFontForCharacter(
      font_description_, hint,
      font_fallback_list_->PrimarySimpleFontData(font_description_));
}

}  // namespace blink
