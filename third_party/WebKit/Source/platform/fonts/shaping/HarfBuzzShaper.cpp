/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include <hb.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <algorithm>
#include <memory>
#include "platform/fonts/Font.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/fonts/opentype/OpenTypeCapsSupport.h"
#include "platform/fonts/shaping/CaseMappingHarfBuzzBufferFiller.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/Unicode.h"

namespace blink {
enum HolesQueueItemAction { kHolesQueueNextFont, kHolesQueueRange };

struct HolesQueueItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  HolesQueueItemAction action_;
  unsigned start_index_;
  unsigned num_characters_;
  HolesQueueItem(HolesQueueItemAction action, unsigned start, unsigned num)
      : action_(action), start_index_(start), num_characters_(num){};
};

template <typename T>
class HarfBuzzScopedPtr {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(HarfBuzzScopedPtr);

 public:
  typedef void (*DestroyFunction)(T*);

  HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
      : ptr_(ptr), destroy_(destroy) {
    DCHECK(destroy_);
  }
  ~HarfBuzzScopedPtr() {
    if (ptr_)
      (*destroy_)(ptr_);
  }

  T* Get() { return ptr_; }
  void Set(T* ptr) { ptr_ = ptr; }

 private:
  T* ptr_;
  DestroyFunction destroy_;
};

HarfBuzzShaper::HarfBuzzShaper(const UChar* text, unsigned length)
    : text_(text), text_length_(length) {}

using FeaturesVector = Vector<hb_feature_t, 6>;
struct HarfBuzzShaper::RangeData {
  hb_buffer_t* buffer;
  const Font* font;
  TextDirection text_direction;
  unsigned start;
  unsigned end;
  FeaturesVector font_features;
  Deque<HolesQueueItem> holes_queue;

  hb_direction_t HarfBuzzDirection(const SimpleFontData* font_data) {
    FontOrientation orientation = font->GetFontDescription().Orientation();
    hb_direction_t direction = IsVerticalAnyUpright(orientation) &&
                                       !font_data->IsTextOrientationFallback()
                                   ? HB_DIRECTION_TTB
                                   : HB_DIRECTION_LTR;
    return text_direction == TextDirection::kRtl
               ? HB_DIRECTION_REVERSE(direction)
               : direction;
  }
};

namespace {

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built
// without hb-icu. See http://crbug.com/356929
static inline hb_script_t ICUScriptToHBScript(UScriptCode script) {
  if (UNLIKELY(script == USCRIPT_INVALID_CODE))
    return HB_SCRIPT_INVALID;

  return hb_script_from_string(uscript_getShortName(script), -1);
}

inline bool ShapeRange(hb_buffer_t* buffer,
                       hb_feature_t* font_features,
                       unsigned font_features_size,
                       const SimpleFontData* current_font,
                       PassRefPtr<UnicodeRangeSet> current_font_range_set,
                       UScriptCode current_run_script,
                       hb_direction_t direction,
                       hb_language_t language) {
  const FontPlatformData* platform_data = &(current_font->PlatformData());
  HarfBuzzFace* face = platform_data->GetHarfBuzzFace();
  if (!face) {
    DLOG(ERROR) << "Could not create HarfBuzzFace from FontPlatformData.";
    return false;
  }

  hb_buffer_set_language(buffer, language);
  hb_buffer_set_script(buffer, ICUScriptToHBScript(current_run_script));
  hb_buffer_set_direction(buffer, direction);

  hb_font_t* hb_font = face->GetScaledFont(std::move(current_font_range_set));
  hb_shape(hb_font, buffer, font_features, font_features_size);

  return true;
}

}  // namespace

void HarfBuzzShaper::ExtractShapeResults(
    RangeData* range_data,
    bool& font_cycle_queued,
    const HolesQueueItem& current_queue_item,
    const SimpleFontData* current_font,
    UScriptCode current_run_script,
    bool is_last_resort,
    ShapeResult* shape_result) const {
  enum ClusterResult { kShaped, kNotDef, kUnknown };
  ClusterResult current_cluster_result = kUnknown;
  ClusterResult previous_cluster_result = kUnknown;
  unsigned previous_cluster = 0;
  unsigned current_cluster = 0;

  // Find first notdef glyph in buffer.
  unsigned num_glyphs = hb_buffer_get_length(range_data->buffer);
  hb_glyph_info_t* glyph_info =
      hb_buffer_get_glyph_infos(range_data->buffer, 0);

  unsigned last_change_position = 0;

  if (!num_glyphs)
    return;

  for (unsigned glyph_index = 0; glyph_index <= num_glyphs; ++glyph_index) {
    // Iterating by clusters, check for when the state switches from shaped
    // to non-shaped and vice versa. Taking into account the edge cases of
    // beginning of the run and end of the run.
    previous_cluster = current_cluster;
    current_cluster = glyph_info[glyph_index].cluster;

    if (glyph_index < num_glyphs) {
      // Still the same cluster, merge shaping status.
      if (previous_cluster == current_cluster && glyph_index != 0) {
        if (glyph_info[glyph_index].codepoint == 0) {
          current_cluster_result = kNotDef;
        } else {
          // We can only call the current cluster fully shapped, if
          // all characters that are part of it are shaped, so update
          // currentClusterResult to kShaped only if the previous
          // characters have been shaped, too.
          current_cluster_result =
              current_cluster_result == kShaped ? kShaped : kNotDef;
        }
        continue;
      }
      // We've moved to a new cluster.
      previous_cluster_result = current_cluster_result;
      current_cluster_result =
          glyph_info[glyph_index].codepoint == 0 ? kNotDef : kShaped;
    } else {
      // The code below operates on the "flanks"/changes between kNotDef
      // and kShaped. In order to keep the code below from explictly
      // dealing with character indices and run end, we explicitly
      // terminate the cluster/run here by setting the result value to the
      // opposite of what it was, leading to atChange turning true.
      previous_cluster_result = current_cluster_result;
      current_cluster_result =
          current_cluster_result == kNotDef ? kShaped : kNotDef;
    }

    bool at_change = (previous_cluster_result != current_cluster_result) &&
                     previous_cluster_result != kUnknown;
    if (!at_change)
      continue;

    // Compute the range indices of consecutive shaped or .notdef glyphs.
    // Cluster information for RTL runs becomes reversed, e.g. character 0
    // has cluster index 5 in a run of 6 characters.
    unsigned num_characters = 0;
    unsigned num_glyphs_to_insert = 0;
    unsigned start_index = 0;
    if (HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(range_data->buffer))) {
      start_index = glyph_info[last_change_position].cluster;
      if (glyph_index == num_glyphs) {
        // Clamp the end offsets of the queue item to the offsets representing
        // the shaping window.
        unsigned shape_end =
            std::min(range_data->end, current_queue_item.start_index_ +
                                          current_queue_item.num_characters_);
        num_characters = shape_end - glyph_info[last_change_position].cluster;
        num_glyphs_to_insert = num_glyphs - last_change_position;
      } else {
        num_characters = glyph_info[glyph_index].cluster -
                         glyph_info[last_change_position].cluster;
        num_glyphs_to_insert = glyph_index - last_change_position;
      }
    } else {
      // Direction Backwards
      start_index = glyph_info[glyph_index - 1].cluster;
      if (last_change_position == 0) {
        // Clamp the end offsets of the queue item to the offsets representing
        // the shaping window.
        unsigned shape_end =
            std::min(range_data->end, current_queue_item.start_index_ +
                                          current_queue_item.num_characters_);
        num_characters = shape_end - glyph_info[glyph_index - 1].cluster;
      } else {
        num_characters = glyph_info[last_change_position - 1].cluster -
                         glyph_info[glyph_index - 1].cluster;
      }
      num_glyphs_to_insert = glyph_index - last_change_position;
    }

    if (current_cluster_result == kShaped && !is_last_resort) {
      // Now it's clear that we need to continue processing.
      if (!font_cycle_queued) {
        range_data->holes_queue.push_back(
            HolesQueueItem(kHolesQueueNextFont, 0, 0));
        font_cycle_queued = true;
      }

      // Here we need to put character positions.
      DCHECK(num_characters);
      range_data->holes_queue.push_back(
          HolesQueueItem(kHolesQueueRange, start_index, num_characters));
    }

    // If numCharacters is 0, that means we hit a NotDef before shaping the
    // whole grapheme. We do not append it here. For the next glyph we
    // encounter, atChange will be true, and the characters corresponding to
    // the grapheme will be added to the TODO queue again, attempting to
    // shape the whole grapheme with the next font.
    // When we're getting here with the last resort font, we have no other
    // choice than adding boxes to the ShapeResult.
    if ((current_cluster_result == kNotDef && num_characters) ||
        is_last_resort) {
      hb_direction_t direction = range_data->HarfBuzzDirection(current_font);
      // Here we need to specify glyph positions.
      ShapeResult::RunInfo* run = new ShapeResult::RunInfo(
          current_font, direction, ICUScriptToHBScript(current_run_script),
          start_index, num_glyphs_to_insert, num_characters);
      shape_result->InsertRun(WTF::WrapUnique(run), last_change_position,
                              num_glyphs_to_insert, range_data->buffer);
      range_data->font->ReportNotDefGlyph();
    }
    last_change_position = glyph_index;
  }
}

static inline const SimpleFontData* FontDataAdjustedForOrientation(
    const SimpleFontData* original_font,
    FontOrientation run_orientation,
    OrientationIterator::RenderOrientation render_orientation) {
  if (!IsVerticalBaseline(run_orientation))
    return original_font;

  if (run_orientation == FontOrientation::kVerticalRotated ||
      (run_orientation == FontOrientation::kVerticalMixed &&
       render_orientation == OrientationIterator::kOrientationRotateSideways))
    return original_font->VerticalRightOrientationFontData().Get();

  return original_font;
}

bool HarfBuzzShaper::CollectFallbackHintChars(
    const Deque<HolesQueueItem>& holes_queue,
    Vector<UChar32>& hint) const {
  if (!holes_queue.size())
    return false;

  hint.clear();

  size_t num_chars_added = 0;
  for (auto it = holes_queue.begin(); it != holes_queue.end(); ++it) {
    if (it->action_ == kHolesQueueNextFont)
      break;

    UChar32 hint_char;
    CHECK_LE((it->start_index_ + it->num_characters_), text_length_);
    UTF16TextIterator iterator(text_ + it->start_index_, it->num_characters_);
    while (iterator.Consume(hint_char)) {
      hint.push_back(hint_char);
      num_chars_added++;
      iterator.Advance();
    }
  }
  return num_chars_added > 0;
}

namespace {

void SplitUntilNextCaseChange(
    const UChar* normalized_buffer,
    Deque<blink::HolesQueueItem>* queue,
    blink::HolesQueueItem& current_queue_item,
    SmallCapsIterator::SmallCapsBehavior& small_caps_behavior) {
  unsigned num_characters_until_case_change = 0;
  SmallCapsIterator small_caps_iterator(
      normalized_buffer + current_queue_item.start_index_,
      current_queue_item.num_characters_);
  small_caps_iterator.Consume(&num_characters_until_case_change,
                              &small_caps_behavior);
  if (num_characters_until_case_change > 0 &&
      num_characters_until_case_change < current_queue_item.num_characters_) {
    queue->push_front(blink::HolesQueueItem(
        blink::HolesQueueItemAction::kHolesQueueRange,
        current_queue_item.start_index_ + num_characters_until_case_change,
        current_queue_item.num_characters_ - num_characters_until_case_change));
    current_queue_item.num_characters_ = num_characters_until_case_change;
  }
}

hb_feature_t CreateFeature(hb_tag_t tag, uint32_t value = 0) {
  return {tag, value, 0 /* start */, static_cast<unsigned>(-1) /* end */};
}

void SetFontFeatures(const Font* font, FeaturesVector* features) {
  const FontDescription& description = font->GetFontDescription();

  static hb_feature_t no_kern = CreateFeature(HB_TAG('k', 'e', 'r', 'n'));
  static hb_feature_t no_vkrn = CreateFeature(HB_TAG('v', 'k', 'r', 'n'));
  switch (description.GetKerning()) {
    case FontDescription::kNormalKerning:
      // kern/vkrn are enabled by default
      break;
    case FontDescription::kNoneKerning:
      features->push_back(description.IsVerticalAnyUpright() ? no_vkrn
                                                             : no_kern);
      break;
    case FontDescription::kAutoKerning:
      break;
  }

  static hb_feature_t no_clig = CreateFeature(HB_TAG('c', 'l', 'i', 'g'));
  static hb_feature_t no_liga = CreateFeature(HB_TAG('l', 'i', 'g', 'a'));
  switch (description.CommonLigaturesState()) {
    case FontDescription::kDisabledLigaturesState:
      features->push_back(no_liga);
      features->push_back(no_clig);
      break;
    case FontDescription::kEnabledLigaturesState:
      // liga and clig are on by default
      break;
    case FontDescription::kNormalLigaturesState:
      break;
  }
  static hb_feature_t dlig = CreateFeature(HB_TAG('d', 'l', 'i', 'g'), 1);
  switch (description.DiscretionaryLigaturesState()) {
    case FontDescription::kDisabledLigaturesState:
      // dlig is off by default
      break;
    case FontDescription::kEnabledLigaturesState:
      features->push_back(dlig);
      break;
    case FontDescription::kNormalLigaturesState:
      break;
  }
  static hb_feature_t hlig = CreateFeature(HB_TAG('h', 'l', 'i', 'g'), 1);
  switch (description.HistoricalLigaturesState()) {
    case FontDescription::kDisabledLigaturesState:
      // hlig is off by default
      break;
    case FontDescription::kEnabledLigaturesState:
      features->push_back(hlig);
      break;
    case FontDescription::kNormalLigaturesState:
      break;
  }
  static hb_feature_t no_calt = CreateFeature(HB_TAG('c', 'a', 'l', 't'));
  switch (description.ContextualLigaturesState()) {
    case FontDescription::kDisabledLigaturesState:
      features->push_back(no_calt);
      break;
    case FontDescription::kEnabledLigaturesState:
      // calt is on by default
      break;
    case FontDescription::kNormalLigaturesState:
      break;
  }

  static hb_feature_t hwid = CreateFeature(HB_TAG('h', 'w', 'i', 'd'), 1);
  static hb_feature_t twid = CreateFeature(HB_TAG('t', 'w', 'i', 'd'), 1);
  static hb_feature_t qwid = CreateFeature(HB_TAG('q', 'w', 'i', 'd'), 1);
  switch (description.WidthVariant()) {
    case kHalfWidth:
      features->push_back(hwid);
      break;
    case kThirdWidth:
      features->push_back(twid);
      break;
    case kQuarterWidth:
      features->push_back(qwid);
      break;
    case kRegularWidth:
      break;
  }

  // font-variant-numeric:
  static hb_feature_t lnum = CreateFeature(HB_TAG('l', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kLiningNums)
    features->push_back(lnum);

  static hb_feature_t onum = CreateFeature(HB_TAG('o', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericFigureValue() ==
      FontVariantNumeric::kOldstyleNums)
    features->push_back(onum);

  static hb_feature_t pnum = CreateFeature(HB_TAG('p', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kProportionalNums)
    features->push_back(pnum);
  static hb_feature_t tnum = CreateFeature(HB_TAG('t', 'n', 'u', 'm'), 1);
  if (description.VariantNumeric().NumericSpacingValue() ==
      FontVariantNumeric::kTabularNums)
    features->push_back(tnum);

  static hb_feature_t afrc = CreateFeature(HB_TAG('a', 'f', 'r', 'c'), 1);
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kStackedFractions)
    features->push_back(afrc);
  static hb_feature_t frac = CreateFeature(HB_TAG('f', 'r', 'a', 'c'), 1);
  if (description.VariantNumeric().NumericFractionValue() ==
      FontVariantNumeric::kDiagonalFractions)
    features->push_back(frac);

  static hb_feature_t ordn = CreateFeature(HB_TAG('o', 'r', 'd', 'n'), 1);
  if (description.VariantNumeric().OrdinalValue() ==
      FontVariantNumeric::kOrdinalOn)
    features->push_back(ordn);

  static hb_feature_t zero = CreateFeature(HB_TAG('z', 'e', 'r', 'o'), 1);
  if (description.VariantNumeric().SlashedZeroValue() ==
      FontVariantNumeric::kSlashedZeroOn)
    features->push_back(zero);

  FontFeatureSettings* settings = description.FeatureSettings();
  if (!settings)
    return;

  // TODO(drott): crbug.com/450619 Implement feature resolution instead of
  // just appending the font-feature-settings.
  unsigned num_features = settings->size();
  for (unsigned i = 0; i < num_features; ++i) {
    hb_feature_t feature;
    const AtomicString& tag = settings->at(i).Tag();
    feature.tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
    feature.value = settings->at(i).Value();
    feature.start = 0;
    feature.end = static_cast<unsigned>(-1);
    features->push_back(feature);
  }
}

class CapsFeatureSettingsScopedOverlay final {
  STACK_ALLOCATED();

 public:
  CapsFeatureSettingsScopedOverlay(FeaturesVector*,
                                   FontDescription::FontVariantCaps);
  CapsFeatureSettingsScopedOverlay() = delete;
  ~CapsFeatureSettingsScopedOverlay();

 private:
  void OverlayCapsFeatures(FontDescription::FontVariantCaps);
  void PrependCounting(const hb_feature_t&);
  FeaturesVector* features_;
  size_t count_features_;
};

CapsFeatureSettingsScopedOverlay::CapsFeatureSettingsScopedOverlay(
    FeaturesVector* features,
    FontDescription::FontVariantCaps variant_caps)
    : features_(features), count_features_(0) {
  OverlayCapsFeatures(variant_caps);
}

void CapsFeatureSettingsScopedOverlay::OverlayCapsFeatures(
    FontDescription::FontVariantCaps variant_caps) {
  static hb_feature_t smcp = CreateFeature(HB_TAG('s', 'm', 'c', 'p'), 1);
  static hb_feature_t pcap = CreateFeature(HB_TAG('p', 'c', 'a', 'p'), 1);
  static hb_feature_t c2sc = CreateFeature(HB_TAG('c', '2', 's', 'c'), 1);
  static hb_feature_t c2pc = CreateFeature(HB_TAG('c', '2', 'p', 'c'), 1);
  static hb_feature_t unic = CreateFeature(HB_TAG('u', 'n', 'i', 'c'), 1);
  static hb_feature_t titl = CreateFeature(HB_TAG('t', 'i', 't', 'l'), 1);
  if (variant_caps == FontDescription::kSmallCaps ||
      variant_caps == FontDescription::kAllSmallCaps) {
    PrependCounting(smcp);
    if (variant_caps == FontDescription::kAllSmallCaps) {
      PrependCounting(c2sc);
    }
  }
  if (variant_caps == FontDescription::kPetiteCaps ||
      variant_caps == FontDescription::kAllPetiteCaps) {
    PrependCounting(pcap);
    if (variant_caps == FontDescription::kAllPetiteCaps) {
      PrependCounting(c2pc);
    }
  }
  if (variant_caps == FontDescription::kUnicase) {
    PrependCounting(unic);
  }
  if (variant_caps == FontDescription::kTitlingCaps) {
    PrependCounting(titl);
  }
}

void CapsFeatureSettingsScopedOverlay::PrependCounting(
    const hb_feature_t& feature) {
  features_->push_front(feature);
  count_features_++;
}

CapsFeatureSettingsScopedOverlay::~CapsFeatureSettingsScopedOverlay() {
  features_->erase(0, count_features_);
}

}  // namespace

void HarfBuzzShaper::ShapeSegment(RangeData* range_data,
                                  RunSegmenter::RunSegmenterRange segment,
                                  ShapeResult* result) const {
  DCHECK(result);
  DCHECK(range_data->buffer);

  const Font* font = range_data->font;
  const FontDescription& font_description = font->GetFontDescription();
  const hb_language_t language =
      font_description.LocaleOrDefault().HarfbuzzLanguage();
  bool needs_caps_handling =
      font_description.VariantCaps() != FontDescription::kCapsNormal;
  OpenTypeCapsSupport caps_support;

  FontOrientation orientation = font->GetFontDescription().Orientation();
  RefPtr<FontFallbackIterator> fallback_iterator =
      font->CreateFontFallbackIterator(segment.font_fallback_priority);

  range_data->holes_queue.push_back(HolesQueueItem(kHolesQueueNextFont, 0, 0));
  range_data->holes_queue.push_back(HolesQueueItem(
      kHolesQueueRange, segment.start, segment.end - segment.start));

  bool font_cycle_queued = false;
  Vector<UChar32> fallback_chars_hint;
  RefPtr<FontDataForRangeSet> current_font_data_for_range_set;
  while (range_data->holes_queue.size()) {
    HolesQueueItem current_queue_item = range_data->holes_queue.TakeFirst();

    if (current_queue_item.action_ == kHolesQueueNextFont) {
      // For now, we're building a character list with which we probe
      // for needed fonts depending on the declared unicode-range of a
      // segmented CSS font. Alternatively, we can build a fake font
      // for the shaper and check whether any glyphs were found, or
      // define a new API on the shaper which will give us coverage
      // information?
      if (!CollectFallbackHintChars(range_data->holes_queue,
                                    fallback_chars_hint)) {
        // Give up shaping since we cannot retrieve a font fallback
        // font without a hintlist.
        range_data->holes_queue.clear();
        break;
      }

      current_font_data_for_range_set =
          fallback_iterator->Next(fallback_chars_hint);
      if (!current_font_data_for_range_set->FontData()) {
        DCHECK(!range_data->holes_queue.size());
        break;
      }
      font_cycle_queued = false;
      continue;
    }

    const SimpleFontData* font_data =
        current_font_data_for_range_set->FontData();
    SmallCapsIterator::SmallCapsBehavior small_caps_behavior =
        SmallCapsIterator::kSmallCapsSameCase;
    if (needs_caps_handling) {
      caps_support = OpenTypeCapsSupport(
          font_data->PlatformData().GetHarfBuzzFace(),
          font_description.VariantCaps(), ICUScriptToHBScript(segment.script));
      if (caps_support.NeedsRunCaseSplitting()) {
        SplitUntilNextCaseChange(text_, &range_data->holes_queue,
                                 current_queue_item, small_caps_behavior);
      }
    }

    DCHECK(current_queue_item.num_characters_);
    const SimpleFontData* smallcaps_adjusted_font =
        needs_caps_handling &&
                caps_support.NeedsSyntheticFont(small_caps_behavior)
            ? font_data->SmallCapsFontData(font_description).Get()
            : font_data;

    // Compatibility with SimpleFontData approach of keeping a flag for
    // overriding drawing direction.
    // TODO: crbug.com/506224 This should go away in favor of storing that
    // information elsewhere, for example in ShapeResult.
    const SimpleFontData* direction_and_small_caps_adjusted_font =
        FontDataAdjustedForOrientation(smallcaps_adjusted_font, orientation,
                                       segment.render_orientation);

    CaseMapIntend case_map_intend = CaseMapIntend::kKeepSameCase;
    if (needs_caps_handling)
      case_map_intend = caps_support.NeedsCaseChange(small_caps_behavior);

    // Clamp the start and end offsets of the queue item to the offsets
    // representing the shaping window.
    unsigned shape_start =
        std::max(range_data->start, current_queue_item.start_index_);
    unsigned shape_end =
        std::min(range_data->end, current_queue_item.start_index_ +
                                      current_queue_item.num_characters_);

    CaseMappingHarfBuzzBufferFiller(
        case_map_intend, font_description.LocaleOrDefault(), range_data->buffer,
        text_, text_length_, shape_start, shape_end - shape_start);

    CapsFeatureSettingsScopedOverlay caps_overlay(
        &range_data->font_features,
        caps_support.FontFeatureToUse(small_caps_behavior));
    hb_direction_t direction =
        range_data->HarfBuzzDirection(direction_and_small_caps_adjusted_font);

    if (!ShapeRange(range_data->buffer,
                    range_data->font_features.IsEmpty()
                        ? 0
                        : range_data->font_features.data(),
                    range_data->font_features.size(),
                    direction_and_small_caps_adjusted_font,
                    current_font_data_for_range_set->Ranges(), segment.script,
                    direction, language))
      DLOG(ERROR) << "Shaping range failed.";

    ExtractShapeResults(range_data, font_cycle_queued, current_queue_item,
                        direction_and_small_caps_adjusted_font, segment.script,
                        !fallback_iterator->HasNext(), result);

    hb_buffer_reset(range_data->buffer);
  }
}

PassRefPtr<ShapeResult> HarfBuzzShaper::Shape(const Font* font,
                                              TextDirection direction,
                                              unsigned start,
                                              unsigned end) const {
  DCHECK(end >= start);
  DCHECK(end <= text_length_);

  unsigned length = end - start;
  RefPtr<ShapeResult> result = ShapeResult::Create(font, length, direction);
  HarfBuzzScopedPtr<hb_buffer_t> buffer(hb_buffer_create(), hb_buffer_destroy);

  // Run segmentation needs to operate on the entire string, regardless of the
  // shaping window (defined by the start and end parameters).
  RunSegmenter::RunSegmenterRange segment_range = RunSegmenter::NullRange();
  RunSegmenter run_segmenter(text_, text_length_,
                             font->GetFontDescription().Orientation());

  RangeData range_data;
  range_data.buffer = buffer.Get();
  range_data.font = font;
  range_data.text_direction = direction;
  range_data.start = start;
  range_data.end = end;
  SetFontFeatures(font, &range_data.font_features);

  while (run_segmenter.Consume(&segment_range)) {
    // Only shape segments overlapping with the range indicated by start and
    // end. Not only those strictly within.
    if (start < segment_range.end && end > segment_range.start)
      ShapeSegment(&range_data, segment_range, result.Get());
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(length, result->NumCharacters());
  if (length) {
    DCHECK_EQ(start, result->StartIndexForResult());
    DCHECK_EQ(end, result->EndIndexForResult());
  }
#endif

  return result;
}

PassRefPtr<ShapeResult> HarfBuzzShaper::Shape(const Font* font,
                                              TextDirection direction) const {
  return Shape(font, direction, 0, text_length_);
}

}  // namespace blink
