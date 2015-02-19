// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_harfbuzz.h"

#include <limits>

#include "base/i18n/bidi_line_iterator.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/harfbuzz-ng/src/hb.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/harfbuzz_font_skia.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/utf16_indexing.h"

#if defined(OS_WIN)
#include "ui/gfx/font_fallback_win.h"
#endif

using gfx::internal::RoundRangeF;

namespace gfx {

namespace {

// Text length limit. Longer strings are slow and not fully tested.
const size_t kMaxTextLength = 10000;

// The maximum number of scripts a Unicode character can belong to. This value
// is arbitrarily chosen to be a good limit because it is unlikely for a single
// character to belong to more scripts.
const size_t kMaxScripts = 5;

// Returns true if characters of |block_code| may trigger font fallback.
// Dingbats and emoticons can be rendered through the color emoji font file,
// therefore it needs to be trigerred as fallbacks. See crbug.com/448909
bool IsUnusualBlockCode(UBlockCode block_code) {
  return block_code == UBLOCK_GEOMETRIC_SHAPES ||
         block_code == UBLOCK_MISCELLANEOUS_SYMBOLS ||
         block_code == UBLOCK_DINGBATS ||
         block_code == UBLOCK_EMOTICONS;
}

bool IsBracket(UChar32 character) {
  static const char kBrackets[] = { '(', ')', '{', '}', '<', '>', };
  static const char* kBracketsEnd = kBrackets + arraysize(kBrackets);
  return std::find(kBrackets, kBracketsEnd, character) != kBracketsEnd;
}

// Returns the boundary between a special and a regular character. Special
// characters are brackets or characters that satisfy |IsUnusualBlockCode|.
size_t FindRunBreakingCharacter(const base::string16& text,
                                size_t run_start,
                                size_t run_break) {
  const int32 run_length = static_cast<int32>(run_break - run_start);
  base::i18n::UTF16CharIterator iter(text.c_str() + run_start, run_length);
  const UChar32 first_char = iter.get();
  // The newline character should form a single run so that the line breaker
  // can handle them easily.
  if (first_char == '\n')
    return run_start + 1;

  const UBlockCode first_block = ublock_getCode(first_char);
  const bool first_block_unusual = IsUnusualBlockCode(first_block);
  const bool first_bracket = IsBracket(first_char);

  while (iter.Advance() && iter.array_pos() < run_length) {
    const UChar32 current_char = iter.get();
    const UBlockCode current_block = ublock_getCode(current_char);
    const bool block_break = current_block != first_block &&
        (first_block_unusual || IsUnusualBlockCode(current_block));
    if (block_break || current_char == '\n' ||
        first_bracket != IsBracket(current_char)) {
      return run_start + iter.array_pos();
    }
  }
  return run_break;
}

// If the given scripts match, returns the one that isn't USCRIPT_COMMON or
// USCRIPT_INHERITED, i.e. the more specific one. Otherwise returns
// USCRIPT_INVALID_CODE.
UScriptCode ScriptIntersect(UScriptCode first, UScriptCode second) {
  if (first == second ||
      (second > USCRIPT_INVALID_CODE && second <= USCRIPT_INHERITED)) {
    return first;
  }
  if (first > USCRIPT_INVALID_CODE && first <= USCRIPT_INHERITED)
    return second;
  return USCRIPT_INVALID_CODE;
}

// Writes the script and the script extensions of the character with the
// Unicode |codepoint|. Returns the number of written scripts.
int GetScriptExtensions(UChar32 codepoint, UScriptCode* scripts) {
  UErrorCode icu_error = U_ZERO_ERROR;
  // ICU documentation incorrectly states that the result of
  // |uscript_getScriptExtensions| will contain the regular script property.
  // Write the character's script property to the first element.
  scripts[0] = uscript_getScript(codepoint, &icu_error);
  if (U_FAILURE(icu_error))
    return 0;
  // Fill the rest of |scripts| with the extensions.
  int count = uscript_getScriptExtensions(codepoint, scripts + 1,
                                          kMaxScripts - 1, &icu_error);
  if (U_FAILURE(icu_error))
    count = 0;
  return count + 1;
}

// Intersects the script extensions set of |codepoint| with |result| and writes
// to |result|, reading and updating |result_size|.
void ScriptSetIntersect(UChar32 codepoint,
                        UScriptCode* result,
                        size_t* result_size) {
  UScriptCode scripts[kMaxScripts] = { USCRIPT_INVALID_CODE };
  int count = GetScriptExtensions(codepoint, scripts);

  size_t out_size = 0;

  for (size_t i = 0; i < *result_size; ++i) {
    for (int j = 0; j < count; ++j) {
      UScriptCode intersection = ScriptIntersect(result[i], scripts[j]);
      if (intersection != USCRIPT_INVALID_CODE) {
        result[out_size++] = intersection;
        break;
      }
    }
  }

  *result_size = out_size;
}

// Find the longest sequence of characters from 0 and up to |length| that
// have at least one common UScriptCode value. Writes the common script value to
// |script| and returns the length of the sequence. Takes the characters' script
// extensions into account. http://www.unicode.org/reports/tr24/#ScriptX
//
// Consider 3 characters with the script values {Kana}, {Hira, Kana}, {Kana}.
// Without script extensions only the first script in each set would be taken
// into account, resulting in 3 runs where 1 would be enough.
// TODO(ckocagil): Write a unit test for the case above.
int ScriptInterval(const base::string16& text,
                   size_t start,
                   size_t length,
                   UScriptCode* script) {
  DCHECK_GT(length, 0U);

  UScriptCode scripts[kMaxScripts] = { USCRIPT_INVALID_CODE };

  base::i18n::UTF16CharIterator char_iterator(text.c_str() + start, length);
  size_t scripts_size = GetScriptExtensions(char_iterator.get(), scripts);
  *script = scripts[0];

  while (char_iterator.Advance()) {
    ScriptSetIntersect(char_iterator.get(), scripts, &scripts_size);
    if (scripts_size == 0U)
      return char_iterator.array_pos();
    *script = scripts[0];
  }

  return length;
}

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built without
// hb-icu. See http://crbug.com/356929
inline hb_script_t ICUScriptToHBScript(UScriptCode script) {
  if (script == USCRIPT_INVALID_CODE)
    return HB_SCRIPT_INVALID;
  return hb_script_from_string(uscript_getShortName(script), -1);
}

// Helper template function for |TextRunHarfBuzz::GetClusterAt()|. |Iterator|
// can be a forward or reverse iterator type depending on the text direction.
template <class Iterator>
void GetClusterAtImpl(size_t pos,
                      Range range,
                      Iterator elements_begin,
                      Iterator elements_end,
                      bool reversed,
                      Range* chars,
                      Range* glyphs) {
  Iterator element = std::upper_bound(elements_begin, elements_end, pos);
  chars->set_end(element == elements_end ? range.end() : *element);
  glyphs->set_end(reversed ? elements_end - element : element - elements_begin);

  DCHECK(element != elements_begin);
  while (--element != elements_begin && *element == *(element - 1));
  chars->set_start(*element);
  glyphs->set_start(
      reversed ? elements_end - element : element - elements_begin);
  if (reversed)
    *glyphs = Range(glyphs->end(), glyphs->start());

  DCHECK(!chars->is_reversed());
  DCHECK(!chars->is_empty());
  DCHECK(!glyphs->is_reversed());
  DCHECK(!glyphs->is_empty());
}

// Internal class to generate Line structures. If |multiline| is true, the text
// is broken into lines at |words| boundaries such that each line is no longer
// than |max_width|. If |multiline| is false, only outputs a single Line from
// the given runs. |min_baseline| and |min_height| are the minimum baseline and
// height for each line.
// TODO(ckocagil): Expose the interface of this class in the header and test
//                 this class directly.
class HarfBuzzLineBreaker {
 public:
  HarfBuzzLineBreaker(size_t max_width,
                      int min_baseline,
                      float min_height,
                      bool multiline,
                      const base::string16& text,
                      const BreakList<size_t>* words,
                      const internal::TextRunList& run_list)
      : max_width_((max_width == 0) ? SK_ScalarMax : SkIntToScalar(max_width)),
        min_baseline_(min_baseline),
        min_height_(min_height),
        multiline_(multiline),
        text_(text),
        words_(words),
        run_list_(run_list),
        text_x_(0),
        line_x_(0),
        max_descent_(0),
        max_ascent_(0) {
    DCHECK_EQ(multiline_, (words_ != nullptr));
    AdvanceLine();
  }

  // Breaks the run at given |run_index| into Line structs.
  void AddRun(int run_index) {
    const internal::TextRunHarfBuzz* run = run_list_.runs()[run_index];
    base::char16 first_char = text_[run->range.start()];
    if (multiline_ && first_char == '\n') {
      AdvanceLine();
    } else if (multiline_ && (line_x_ + SkFloatToScalar(run->width)) >
               max_width_) {
      BreakRun(run_index);
    } else {
      AddSegment(run_index, run->range, run->width);
    }
  }

  // Finishes line breaking and outputs the results. Can be called at most once.
  void Finalize(std::vector<internal::Line>* lines, SizeF* size) {
    DCHECK(!lines_.empty());
    // Add an empty line to finish the line size calculation and remove it.
    AdvanceLine();
    lines_.pop_back();
    *size = total_size_;
    lines->swap(lines_);
  }

 private:
  // A (line index, segment index) pair that specifies a segment in |lines_|.
  typedef std::pair<size_t, size_t> SegmentHandle;

  internal::LineSegment* SegmentFromHandle(const SegmentHandle& handle) {
    return &lines_[handle.first].segments[handle.second];
  }

  // Breaks a run into segments that fit in the last line in |lines_| and adds
  // them. Adds a new Line to the back of |lines_| whenever a new segment can't
  // be added without the Line's width exceeding |max_width_|.
  void BreakRun(int run_index) {
    const internal::TextRunHarfBuzz& run = *(run_list_.runs()[run_index]);
    SkScalar width = 0;
    size_t next_char = run.range.start();

    // Break the run until it fits the current line.
    while (next_char < run.range.end()) {
      const size_t current_char = next_char;
      const bool skip_line =
          BreakRunAtWidth(run, current_char, &width, &next_char);
      AddSegment(run_index, Range(current_char, next_char),
                 SkScalarToFloat(width));
      if (skip_line)
        AdvanceLine();
    }
  }

  // Starting from |start_char|, finds a suitable line break position at or
  // before available width using word break. If the current position is at the
  // beginning of a line, this function will not roll back to |start_char| and
  // |*next_char| will be greater than |start_char| (to avoid constructing empty
  // lines).
  // Returns whether to skip the line before |*next_char|.
  // TODO(ckocagil): Check clusters to avoid breaking ligatures and diacritics.
  // TODO(ckocagil): We might have to reshape after breaking at ligatures.
  //                 See whether resolving the TODO above resolves this too.
  // TODO(ckocagil): Do not reserve width for whitespace at the end of lines.
  bool BreakRunAtWidth(const internal::TextRunHarfBuzz& run,
                       size_t start_char,
                       SkScalar* width,
                       size_t* next_char) {
    DCHECK(words_);
    DCHECK(run.range.Contains(Range(start_char, start_char + 1)));
    SkScalar available_width = max_width_ - line_x_;
    BreakList<size_t>::const_iterator word = words_->GetBreak(start_char);
    BreakList<size_t>::const_iterator next_word = word + 1;
    // Width from |std::max(word->first, start_char)| to the current character.
    SkScalar word_width = 0;
    *width = 0;

    for (size_t i = start_char; i < run.range.end(); ++i) {
      // |word| holds the word boundary at or before |i|, and |next_word| holds
      // the word boundary right after |i|. Advance both |word| and |next_word|
      // when |i| reaches |next_word|.
      if (next_word != words_->breaks().end() && i >= next_word->first) {
        word = next_word++;
        word_width = 0;
      }

      Range glyph_range = run.CharRangeToGlyphRange(Range(i, i + 1));
      SkScalar char_width = ((glyph_range.end() >= run.glyph_count)
                                 ? SkFloatToScalar(run.width)
                                 : run.positions[glyph_range.end()].x()) -
                            run.positions[glyph_range.start()].x();

      *width += char_width;
      word_width += char_width;

      if (*width > available_width) {
        if (line_x_ != 0 || word_width < *width) {
          // Roll back one word.
          *width -= word_width;
          *next_char = std::max(word->first, start_char);
        } else if (char_width < *width) {
          // Roll back one character.
          *width -= char_width;
          *next_char = i;
        } else {
          // Continue from the next character.
          *next_char = i + 1;
        }
        return true;
      }
    }

    *next_char = run.range.end();
    return false;
  }

  // RTL runs are broken in logical order but displayed in visual order. To find
  // the text-space coordinate (where it would fall in a single-line text)
  // |x_range| of RTL segments, segment widths are applied in reverse order.
  // e.g. {[5, 10], [10, 40]} will become {[35, 40], [5, 35]}.
  void UpdateRTLSegmentRanges() {
    if (rtl_segments_.empty())
      return;
    float x = SegmentFromHandle(rtl_segments_[0])->x_range.start();
    for (size_t i = rtl_segments_.size(); i > 0; --i) {
      internal::LineSegment* segment = SegmentFromHandle(rtl_segments_[i - 1]);
      const float segment_width = segment->width;
      segment->x_range = Range(x, x + segment_width);
      x += segment_width;
    }
    rtl_segments_.clear();
  }

  // Finishes the size calculations of the last Line in |lines_|. Adds a new
  // Line to the back of |lines_|.
  void AdvanceLine() {
    if (!lines_.empty()) {
      internal::Line* line = &lines_.back();
      std::sort(line->segments.begin(), line->segments.end(),
                [this](const internal::LineSegment& s1,
                       const internal::LineSegment& s2) -> bool {
                  return run_list_.logical_to_visual(s1.run) <
                         run_list_.logical_to_visual(s2.run);
                });
      line->size.set_height(std::max(min_height_, max_descent_ + max_ascent_));
      line->baseline =
          std::max(min_baseline_, SkScalarRoundToInt(max_ascent_));
      line->preceding_heights = std::ceil(total_size_.height());
      total_size_.set_height(total_size_.height() + line->size.height());
      total_size_.set_width(std::max(total_size_.width(), line->size.width()));
    }
    max_descent_ = 0;
    max_ascent_ = 0;
    line_x_ = 0;
    lines_.push_back(internal::Line());
  }

  // Adds a new segment with the given properties to |lines_.back()|.
  void AddSegment(int run_index, Range char_range, float width) {
    if (char_range.is_empty()) {
      DCHECK_EQ(0, width);
      return;
    }
    const internal::TextRunHarfBuzz& run = *(run_list_.runs()[run_index]);

    internal::LineSegment segment;
    segment.run = run_index;
    segment.char_range = char_range;
    segment.x_range = Range(
        SkScalarCeilToInt(text_x_),
        SkScalarCeilToInt(text_x_ + SkFloatToScalar(width)));
    segment.width = width;

    internal::Line* line = &lines_.back();
    line->segments.push_back(segment);

    SkPaint paint;
    paint.setTypeface(run.skia_face.get());
    paint.setTextSize(SkIntToScalar(run.font_size));
    paint.setAntiAlias(run.render_params.antialiasing);
    SkPaint::FontMetrics metrics;
    paint.getFontMetrics(&metrics);

    line->size.set_width(line->size.width() + width);
    max_descent_ = std::max(max_descent_, metrics.fDescent);
    // fAscent is always negative.
    max_ascent_ = std::max(max_ascent_, -metrics.fAscent);

    if (run.is_rtl) {
      rtl_segments_.push_back(
          SegmentHandle(lines_.size() - 1, line->segments.size() - 1));
      // If this is the last segment of an RTL run, reprocess the text-space x
      // ranges of all segments from the run.
      if (char_range.end() == run.range.end())
        UpdateRTLSegmentRanges();
    }
    text_x_ += SkFloatToScalar(width);
    line_x_ += SkFloatToScalar(width);
  }

  const SkScalar max_width_;
  const int min_baseline_;
  const float min_height_;
  const bool multiline_;
  const base::string16& text_;
  const BreakList<size_t>* const words_;
  const internal::TextRunList& run_list_;

  // Stores the resulting lines.
  std::vector<internal::Line> lines_;

  // Text space and line space x coordinates of the next segment to be added.
  SkScalar text_x_;
  SkScalar line_x_;

  float max_descent_;
  float max_ascent_;

  // Size of the multiline text, not including the currently processed line.
  SizeF total_size_;

  // The current RTL run segments, to be applied by |UpdateRTLSegmentRanges()|.
  std::vector<SegmentHandle> rtl_segments_;

  DISALLOW_COPY_AND_ASSIGN(HarfBuzzLineBreaker);
};

}  // namespace

namespace internal {

Range RoundRangeF(const RangeF& range_f) {
  return Range(std::floor(range_f.start() + 0.5f),
               std::floor(range_f.end() + 0.5f));
}

TextRunHarfBuzz::TextRunHarfBuzz()
    : width(0.0f),
      preceding_run_widths(0.0f),
      is_rtl(false),
      level(0),
      script(USCRIPT_INVALID_CODE),
      glyph_count(static_cast<size_t>(-1)),
      font_size(0),
      font_style(0),
      strike(false),
      diagonal_strike(false),
      underline(false) {}

TextRunHarfBuzz::~TextRunHarfBuzz() {}

void TextRunHarfBuzz::GetClusterAt(size_t pos,
                                   Range* chars,
                                   Range* glyphs) const {
  DCHECK(range.Contains(Range(pos, pos + 1)));
  DCHECK(chars);
  DCHECK(glyphs);

  if (glyph_count == 0) {
    *chars = range;
    *glyphs = Range();
    return;
  }

  if (is_rtl) {
    GetClusterAtImpl(pos, range, glyph_to_char.rbegin(), glyph_to_char.rend(),
        true, chars, glyphs);
    return;
  }

  GetClusterAtImpl(pos, range, glyph_to_char.begin(), glyph_to_char.end(),
      false, chars, glyphs);
}

Range TextRunHarfBuzz::CharRangeToGlyphRange(const Range& char_range) const {
  DCHECK(range.Contains(char_range));
  DCHECK(!char_range.is_reversed());
  DCHECK(!char_range.is_empty());

  Range start_glyphs;
  Range end_glyphs;
  Range temp_range;
  GetClusterAt(char_range.start(), &temp_range, &start_glyphs);
  GetClusterAt(char_range.end() - 1, &temp_range, &end_glyphs);

  return is_rtl ? Range(end_glyphs.start(), start_glyphs.end()) :
      Range(start_glyphs.start(), end_glyphs.end());
}

size_t TextRunHarfBuzz::CountMissingGlyphs() const {
  static const int kMissingGlyphId = 0;
  size_t missing = 0;
  for (size_t i = 0; i < glyph_count; ++i)
    missing += (glyphs[i] == kMissingGlyphId) ? 1 : 0;
  return missing;
}

RangeF TextRunHarfBuzz::GetGraphemeBounds(
    base::i18n::BreakIterator* grapheme_iterator,
    size_t text_index) {
  DCHECK_LT(text_index, range.end());
  if (glyph_count == 0)
    return RangeF(preceding_run_widths, preceding_run_widths + width);

  Range chars;
  Range glyphs;
  GetClusterAt(text_index, &chars, &glyphs);
  const float cluster_begin_x = positions[glyphs.start()].x();
  const float cluster_end_x = glyphs.end() < glyph_count ?
      positions[glyphs.end()].x() : SkFloatToScalar(width);

  // A cluster consists of a number of code points and corresponds to a number
  // of glyphs that should be drawn together. A cluster can contain multiple
  // graphemes. In order to place the cursor at a grapheme boundary inside the
  // cluster, we simply divide the cluster width by the number of graphemes.
  if (chars.length() > 1 && grapheme_iterator) {
    int before = 0;
    int total = 0;
    for (size_t i = chars.start(); i < chars.end(); ++i) {
      if (grapheme_iterator->IsGraphemeBoundary(i)) {
        if (i < text_index)
          ++before;
        ++total;
      }
    }
    DCHECK_GT(total, 0);
    if (total > 1) {
      if (is_rtl)
        before = total - before - 1;
      DCHECK_GE(before, 0);
      DCHECK_LT(before, total);
      const int cluster_width = cluster_end_x - cluster_begin_x;
      const int grapheme_begin_x = cluster_begin_x + static_cast<int>(0.5f +
          cluster_width * before / static_cast<float>(total));
      const int grapheme_end_x = cluster_begin_x + static_cast<int>(0.5f +
          cluster_width * (before + 1) / static_cast<float>(total));
      return RangeF(preceding_run_widths + grapheme_begin_x,
                    preceding_run_widths + grapheme_end_x);
    }
  }

  return RangeF(preceding_run_widths + cluster_begin_x,
                preceding_run_widths + cluster_end_x);
}

TextRunList::TextRunList() : width_(0.0f) {}

TextRunList::~TextRunList() {}

void TextRunList::Reset() {
  runs_.clear();
  width_ = 0.0f;
}

void TextRunList::InitIndexMap() {
  if (runs_.size() == 1) {
    visual_to_logical_ = logical_to_visual_ = std::vector<int32_t>(1, 0);
    return;
  }
  const size_t num_runs = runs_.size();
  std::vector<UBiDiLevel> levels(num_runs);
  for (size_t i = 0; i < num_runs; ++i)
    levels[i] = runs_[i]->level;
  visual_to_logical_.resize(num_runs);
  ubidi_reorderVisual(&levels[0], num_runs, &visual_to_logical_[0]);
  logical_to_visual_.resize(num_runs);
  ubidi_reorderLogical(&levels[0], num_runs, &logical_to_visual_[0]);
}

void TextRunList::ComputePrecedingRunWidths() {
  // Precalculate run width information.
  width_ = 0.0f;
  for (size_t i = 0; i < runs_.size(); ++i) {
    TextRunHarfBuzz* run = runs_[visual_to_logical_[i]];
    run->preceding_run_widths = width_;
    width_ += run->width;
  }
}

}  // namespace internal

RenderTextHarfBuzz::RenderTextHarfBuzz()
    : RenderText(),
      update_layout_run_list_(false),
      update_display_run_list_(false),
      update_grapheme_iterator_(false),
      update_display_text_(false),
      glyph_width_for_test_(0u) {
  set_truncate_length(kMaxTextLength);
}

RenderTextHarfBuzz::~RenderTextHarfBuzz() {}

scoped_ptr<RenderText> RenderTextHarfBuzz::CreateInstanceOfSameType() const {
  return make_scoped_ptr(new RenderTextHarfBuzz);
}

const base::string16& RenderTextHarfBuzz::GetDisplayText() {
  // TODO(oshima): Consider supporting eliding multi-line text.
  // This requires max_line support first.
  if (multiline() ||
      elide_behavior() == NO_ELIDE ||
      elide_behavior() == FADE_TAIL) {
    // Call UpdateDisplayText to clear |display_text_| and |text_elided_|
    // on the RenderText class.
    UpdateDisplayText(0);
    update_display_text_ = false;
    display_run_list_.reset();
    return layout_text();
  }

  EnsureLayoutRunList();
  DCHECK(!update_display_text_);
  return text_elided() ? display_text() : layout_text();
}

Size RenderTextHarfBuzz::GetStringSize() {
  const SizeF size_f = GetStringSizeF();
  return Size(std::ceil(size_f.width()), size_f.height());
}

SizeF RenderTextHarfBuzz::GetStringSizeF() {
  EnsureLayout();
  return total_size_;
}

SelectionModel RenderTextHarfBuzz::FindCursorPosition(const Point& point) {
  EnsureLayout();

  int x = ToTextPoint(point).x();
  float offset = 0;
  size_t run_index = GetRunContainingXCoord(x, &offset);

  internal::TextRunList* run_list = GetRunList();
  if (run_index >= run_list->size())
    return EdgeSelectionModel((x < 0) ? CURSOR_LEFT : CURSOR_RIGHT);
  const internal::TextRunHarfBuzz& run = *run_list->runs()[run_index];
  for (size_t i = 0; i < run.glyph_count; ++i) {
    const SkScalar end =
        i + 1 == run.glyph_count ? run.width : run.positions[i + 1].x();
    const SkScalar middle = (end + run.positions[i].x()) / 2;

    if (offset < middle) {
      return SelectionModel(DisplayIndexToTextIndex(
          run.glyph_to_char[i] + (run.is_rtl ? 1 : 0)),
          (run.is_rtl ? CURSOR_BACKWARD : CURSOR_FORWARD));
    }
    if (offset < end) {
      return SelectionModel(DisplayIndexToTextIndex(
          run.glyph_to_char[i] + (run.is_rtl ? 0 : 1)),
          (run.is_rtl ? CURSOR_FORWARD : CURSOR_BACKWARD));
    }
  }
  return EdgeSelectionModel(CURSOR_RIGHT);
}

std::vector<RenderText::FontSpan> RenderTextHarfBuzz::GetFontSpansForTesting() {
  EnsureLayout();

  internal::TextRunList* run_list = GetRunList();
  std::vector<RenderText::FontSpan> spans;
  for (auto* run : run_list->runs()) {
    SkString family_name;
    run->skia_face->getFamilyName(&family_name);
    Font font(family_name.c_str(), run->font_size);
    spans.push_back(RenderText::FontSpan(
        font,
        Range(DisplayIndexToTextIndex(run->range.start()),
              DisplayIndexToTextIndex(run->range.end()))));
  }

  return spans;
}

Range RenderTextHarfBuzz::GetGlyphBounds(size_t index) {
  EnsureLayout();
  const size_t run_index =
      GetRunContainingCaret(SelectionModel(index, CURSOR_FORWARD));
  internal::TextRunList* run_list = GetRunList();
  // Return edge bounds if the index is invalid or beyond the layout text size.
  if (run_index >= run_list->size())
    return Range(GetStringSize().width());
  const size_t layout_index = TextIndexToDisplayIndex(index);
  internal::TextRunHarfBuzz* run = run_list->runs()[run_index];
  RangeF bounds =
      run->GetGraphemeBounds(GetGraphemeIterator(), layout_index);
  // If cursor is enabled, extend the last glyph up to the rightmost cursor
  // position since clients expect them to be contiguous.
  if (cursor_enabled() && run_index == run_list->size() - 1 &&
      index == (run->is_rtl ? run->range.start() : run->range.end() - 1))
    bounds.set_end(std::ceil(bounds.end()));
  return RoundRangeF(run->is_rtl ?
      RangeF(bounds.end(), bounds.start()) : bounds);
}

int RenderTextHarfBuzz::GetDisplayTextBaseline() {
  EnsureLayout();
  return lines()[0].baseline;
}

SelectionModel RenderTextHarfBuzz::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  DCHECK(!update_display_run_list_);

  internal::TextRunList* run_list = GetRunList();
  internal::TextRunHarfBuzz* run;

  size_t run_index = GetRunContainingCaret(selection);
  if (run_index >= run_list->size()) {
    // The cursor is not in any run: we're at the visual and logical edge.
    SelectionModel edge = EdgeSelectionModel(direction);
    if (edge.caret_pos() == selection.caret_pos())
      return edge;
    int visual_index = (direction == CURSOR_RIGHT) ? 0 : run_list->size() - 1;
    run = run_list->runs()[run_list->visual_to_logical(visual_index)];
  } else {
    // If the cursor is moving within the current run, just move it by one
    // grapheme in the appropriate direction.
    run = run_list->runs()[run_index];
    size_t caret = selection.caret_pos();
    bool forward_motion = run->is_rtl == (direction == CURSOR_LEFT);
    if (forward_motion) {
      if (caret < DisplayIndexToTextIndex(run->range.end())) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
        return SelectionModel(caret, CURSOR_BACKWARD);
      }
    } else {
      if (caret > DisplayIndexToTextIndex(run->range.start())) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_BACKWARD);
        return SelectionModel(caret, CURSOR_FORWARD);
      }
    }
    // The cursor is at the edge of a run; move to the visually adjacent run.
    int visual_index = run_list->logical_to_visual(run_index);
    visual_index += (direction == CURSOR_LEFT) ? -1 : 1;
    if (visual_index < 0 || visual_index >= static_cast<int>(run_list->size()))
      return EdgeSelectionModel(direction);
    run = run_list->runs()[run_list->visual_to_logical(visual_index)];
  }
  bool forward_motion = run->is_rtl == (direction == CURSOR_LEFT);
  return forward_motion ? FirstSelectionModelInsideRun(run) :
                          LastSelectionModelInsideRun(run);
}

SelectionModel RenderTextHarfBuzz::AdjacentWordSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  if (obscured())
    return EdgeSelectionModel(direction);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  // Match OS specific word break behavior.
#if defined(OS_WIN)
  size_t pos;
  if (direction == CURSOR_RIGHT) {
    pos = std::min(selection.caret_pos() + 1, text().length());
    while (iter.Advance()) {
      pos = iter.pos();
      if (iter.IsWord() && pos > selection.caret_pos())
        break;
    }
  } else {  // direction == CURSOR_LEFT
    // Notes: We always iterate words from the beginning.
    // This is probably fast enough for our usage, but we may
    // want to modify WordIterator so that it can start from the
    // middle of string and advance backwards.
    pos = std::max<int>(selection.caret_pos() - 1, 0);
    while (iter.Advance()) {
      if (iter.IsWord()) {
        size_t begin = iter.pos() - iter.GetString().length();
        if (begin == selection.caret_pos()) {
          // The cursor is at the beginning of a word.
          // Move to previous word.
          break;
        } else if (iter.pos() >= selection.caret_pos()) {
          // The cursor is in the middle or at the end of a word.
          // Move to the top of current word.
          pos = begin;
          break;
        }
        pos = iter.pos() - iter.GetString().length();
      }
    }
  }
  return SelectionModel(pos, CURSOR_FORWARD);
#else
  internal::TextRunList* run_list = GetRunList();
  SelectionModel cur(selection);
  for (;;) {
    cur = AdjacentCharSelectionModel(cur, direction);
    size_t run = GetRunContainingCaret(cur);
    if (run == run_list->size())
      break;
    const bool is_forward =
        run_list->runs()[run]->is_rtl == (direction == CURSOR_LEFT);
    size_t cursor = cur.caret_pos();
    if (is_forward ? iter.IsEndOfWord(cursor) : iter.IsStartOfWord(cursor))
      break;
  }
  return cur;
#endif
}

std::vector<Rect> RenderTextHarfBuzz::GetSubstringBounds(const Range& range) {
  DCHECK(!update_display_run_list_);
  DCHECK(Range(0, text().length()).Contains(range));
  Range layout_range(TextIndexToDisplayIndex(range.start()),
                     TextIndexToDisplayIndex(range.end()));
  DCHECK(Range(0, GetDisplayText().length()).Contains(layout_range));

  std::vector<Rect> rects;
  if (layout_range.is_empty())
    return rects;
  std::vector<Range> bounds;

  internal::TextRunList* run_list = GetRunList();

  // Add a Range for each run/selection intersection.
  for (size_t i = 0; i < run_list->size(); ++i) {
    internal::TextRunHarfBuzz* run =
        run_list->runs()[run_list->visual_to_logical(i)];
    Range intersection = run->range.Intersect(layout_range);
    if (!intersection.IsValid())
      continue;
    DCHECK(!intersection.is_reversed());
    const Range leftmost_character_x = RoundRangeF(run->GetGraphemeBounds(
        GetGraphemeIterator(),
        run->is_rtl ? intersection.end() - 1 : intersection.start()));
    const Range rightmost_character_x = RoundRangeF(run->GetGraphemeBounds(
        GetGraphemeIterator(),
        run->is_rtl ? intersection.start() : intersection.end() - 1));
    Range range_x(leftmost_character_x.start(), rightmost_character_x.end());
    DCHECK(!range_x.is_reversed());
    if (range_x.is_empty())
      continue;

    // Union this with the last range if they're adjacent.
    DCHECK(bounds.empty() || bounds.back().GetMax() <= range_x.GetMin());
    if (!bounds.empty() && bounds.back().GetMax() == range_x.GetMin()) {
      range_x = Range(bounds.back().GetMin(), range_x.GetMax());
      bounds.pop_back();
    }
    bounds.push_back(range_x);
  }
  for (Range& bound : bounds) {
    std::vector<Rect> current_rects = TextBoundsToViewBounds(bound);
    rects.insert(rects.end(), current_rects.begin(), current_rects.end());
  }
  return rects;
}

size_t RenderTextHarfBuzz::TextIndexToDisplayIndex(size_t index) {
  return TextIndexToGivenTextIndex(GetDisplayText(), index);
}

size_t RenderTextHarfBuzz::DisplayIndexToTextIndex(size_t index) {
  if (!obscured())
    return index;
  const size_t text_index = UTF16OffsetToIndex(text(), 0, index);
  DCHECK_LE(text_index, text().length());
  return text_index;
}

bool RenderTextHarfBuzz::IsValidCursorIndex(size_t index) {
  if (index == 0 || index == text().length())
    return true;
  if (!IsValidLogicalIndex(index))
    return false;
  base::i18n::BreakIterator* grapheme_iterator = GetGraphemeIterator();
  return !grapheme_iterator || grapheme_iterator->IsGraphemeBoundary(index);
}

void RenderTextHarfBuzz::OnLayoutTextAttributeChanged(bool text_changed) {
  update_layout_run_list_ = true;
  OnDisplayTextAttributeChanged();
}

void RenderTextHarfBuzz::OnDisplayTextAttributeChanged() {
  update_display_text_ = true;
  update_grapheme_iterator_ = true;
}

void RenderTextHarfBuzz::EnsureLayout() {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::EnsureLayout"));

  EnsureLayoutRunList();

  if (update_display_run_list_) {
    DCHECK(text_elided());
    const base::string16& display_text = GetDisplayText();
    display_run_list_.reset(new internal::TextRunList);

    if (!display_text.empty()) {
      TRACE_EVENT0("ui", "RenderTextHarfBuzz:EnsureLayout1");

      // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile1(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "431326 RenderTextHarfBuzz::EnsureLayout1"));
      ItemizeTextToRuns(display_text, display_run_list_.get());

      // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile2(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "431326 RenderTextHarfBuzz::EnsureLayout12"));
      ShapeRunList(display_text, display_run_list_.get());
    }
    update_display_run_list_ = false;

    // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is
    // fixed.
    tracked_objects::ScopedTracker tracking_profile14(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "431326 RenderTextHarfBuzz::EnsureLayout14"));
    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
  }

  if (lines().empty()) {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "431326 RenderTextHarfBuzz::EnsureLayout2"));

    internal::TextRunList* run_list = GetRunList();
    HarfBuzzLineBreaker line_breaker(
        display_rect().width(), font_list().GetBaseline(),
        std::max(font_list().GetHeight(), min_line_height()), multiline(),
        GetDisplayText(), multiline() ? &GetLineBreaks() : nullptr, *run_list);

    // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile3(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "431326 RenderTextHarfBuzz::EnsureLayout3"));

    for (size_t i = 0; i < run_list->size(); ++i)
      line_breaker.AddRun(i);
    std::vector<internal::Line> lines;
    line_breaker.Finalize(&lines, &total_size_);
    set_lines(&lines);
  }
}

void RenderTextHarfBuzz::DrawVisualText(Canvas* canvas) {
  internal::SkiaTextRenderer renderer(canvas);
  DrawVisualTextInternal(&renderer);
}

void RenderTextHarfBuzz::DrawVisualTextInternal(
    internal::SkiaTextRenderer* renderer) {
  DCHECK(!update_layout_run_list_);
  DCHECK(!update_display_run_list_);
  DCHECK(!update_display_text_);
  if (lines().empty())
    return;

  ApplyFadeEffects(renderer);
  ApplyTextShadows(renderer);
  ApplyCompositionAndSelectionStyles();

  internal::TextRunList* run_list = GetRunList();
  for (size_t i = 0; i < lines().size(); ++i) {
    const internal::Line& line = lines()[i];
    const Vector2d origin = GetLineOffset(i) + Vector2d(0, line.baseline);
    SkScalar preceding_segment_widths = 0;
    for (const internal::LineSegment& segment : line.segments) {
      const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
      renderer->SetTypeface(run.skia_face.get());
      renderer->SetTextSize(SkIntToScalar(run.font_size));
      renderer->SetFontRenderParams(run.render_params,
                                    subpixel_rendering_suppressed());
      Range glyphs_range = run.CharRangeToGlyphRange(segment.char_range);
      scoped_ptr<SkPoint[]> positions(new SkPoint[glyphs_range.length()]);
      SkScalar offset_x = preceding_segment_widths -
                          ((glyphs_range.GetMin() != 0)
                               ? run.positions[glyphs_range.GetMin()].x()
                               : 0);
      for (size_t j = 0; j < glyphs_range.length(); ++j) {
        positions[j] = run.positions[(glyphs_range.is_reversed()) ?
                                     (glyphs_range.start() - j) :
                                     (glyphs_range.start() + j)];
        positions[j].offset(SkIntToScalar(origin.x()) + offset_x,
                            SkIntToScalar(origin.y()));
      }
      for (BreakList<SkColor>::const_iterator it =
               colors().GetBreak(segment.char_range.start());
           it != colors().breaks().end() &&
               it->first < segment.char_range.end();
           ++it) {
        const Range intersection =
            colors().GetRange(it).Intersect(segment.char_range);
        const Range colored_glyphs = run.CharRangeToGlyphRange(intersection);
        // The range may be empty if a portion of a multi-character grapheme is
        // selected, yielding two colors for a single glyph. For now, this just
        // paints the glyph with a single style, but it should paint it twice,
        // clipped according to selection bounds. See http://crbug.com/366786
        if (colored_glyphs.is_empty())
          continue;

        renderer->SetForegroundColor(it->second);
        renderer->DrawPosText(
            &positions[colored_glyphs.start() - glyphs_range.start()],
            &run.glyphs[colored_glyphs.start()], colored_glyphs.length());
        int start_x = SkScalarRoundToInt(
            positions[colored_glyphs.start() - glyphs_range.start()].x());
        int end_x = SkScalarRoundToInt(
            (colored_glyphs.end() == glyphs_range.end())
                ? (SkFloatToScalar(segment.width) + preceding_segment_widths +
                   SkIntToScalar(origin.x()))
                : positions[colored_glyphs.end() - glyphs_range.start()].x());
        renderer->DrawDecorations(start_x, origin.y(), end_x - start_x,
                                  run.underline, run.strike,
                                  run.diagonal_strike);
      }
      preceding_segment_widths += SkFloatToScalar(segment.width);
    }
  }

  renderer->EndDiagonalStrike();

  UndoCompositionAndSelectionStyles();
}

size_t RenderTextHarfBuzz::GetRunContainingCaret(
    const SelectionModel& caret) {
  DCHECK(!update_display_run_list_);
  size_t layout_position = TextIndexToDisplayIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  internal::TextRunList* run_list = GetRunList();
  for (size_t i = 0; i < run_list->size(); ++i) {
    internal::TextRunHarfBuzz* run = run_list->runs()[i];
    if (RangeContainsCaret(run->range, layout_position, affinity))
      return i;
  }
  return run_list->size();
}

size_t RenderTextHarfBuzz::GetRunContainingXCoord(float x,
                                                  float* offset) const {
  DCHECK(!update_display_run_list_);
  const internal::TextRunList* run_list = GetRunList();
  if (x < 0)
    return run_list->size();
  // Find the text run containing the argument point (assumed already offset).
  float current_x = 0;
  for (size_t i = 0; i < run_list->size(); ++i) {
    size_t run = run_list->visual_to_logical(i);
    current_x += run_list->runs()[run]->width;
    if (x < current_x) {
      *offset = x - (current_x - run_list->runs()[run]->width);
      return run;
    }
  }
  return run_list->size();
}

SelectionModel RenderTextHarfBuzz::FirstSelectionModelInsideRun(
    const internal::TextRunHarfBuzz* run) {
  size_t position = DisplayIndexToTextIndex(run->range.start());
  position = IndexOfAdjacentGrapheme(position, CURSOR_FORWARD);
  return SelectionModel(position, CURSOR_BACKWARD);
}

SelectionModel RenderTextHarfBuzz::LastSelectionModelInsideRun(
    const internal::TextRunHarfBuzz* run) {
  size_t position = DisplayIndexToTextIndex(run->range.end());
  position = IndexOfAdjacentGrapheme(position, CURSOR_BACKWARD);
  return SelectionModel(position, CURSOR_FORWARD);
}

void RenderTextHarfBuzz::ItemizeTextToRuns(
    const base::string16& text,
    internal::TextRunList* run_list_out) {
  const bool is_text_rtl = GetTextDirection(text) == base::i18n::RIGHT_TO_LEFT;
  DCHECK_NE(0U, text.length());

  // If ICU fails to itemize the text, we create a run that spans the entire
  // text. This is needed because leaving the runs set empty causes some clients
  // to misbehave since they expect non-zero text metrics from a non-empty text.
  base::i18n::BiDiLineIterator bidi_iterator;
  if (!bidi_iterator.Open(text, is_text_rtl, false)) {
    internal::TextRunHarfBuzz* run = new internal::TextRunHarfBuzz;
    run->range = Range(0, text.length());
    run_list_out->add(run);
    run_list_out->InitIndexMap();
    return;
  }

  // Temporarily apply composition underlines and selection colors.
  ApplyCompositionAndSelectionStyles();

  // Build the list of runs from the script items and ranged styles. Use an
  // empty color BreakList to avoid breaking runs at color boundaries.
  BreakList<SkColor> empty_colors;
  empty_colors.SetMax(text.length());
  internal::StyleIterator style(empty_colors, styles());

  for (size_t run_break = 0; run_break < text.length();) {
    internal::TextRunHarfBuzz* run = new internal::TextRunHarfBuzz;
    run->range.set_start(run_break);
    run->font_style = (style.style(BOLD) ? Font::BOLD : 0) |
                      (style.style(ITALIC) ? Font::ITALIC : 0);
    run->strike = style.style(STRIKE);
    run->diagonal_strike = style.style(DIAGONAL_STRIKE);
    run->underline = style.style(UNDERLINE);
    int32 script_item_break = 0;
    bidi_iterator.GetLogicalRun(run_break, &script_item_break, &run->level);
    // Odd BiDi embedding levels correspond to RTL runs.
    run->is_rtl = (run->level % 2) == 1;
    // Find the length and script of this script run.
    script_item_break = ScriptInterval(text, run_break,
        script_item_break - run_break, &run->script) + run_break;

    // Find the next break and advance the iterators as needed.
    run_break = std::min(
        static_cast<size_t>(script_item_break),
        TextIndexToGivenTextIndex(text, style.GetRange().end()));

    // Break runs at certain characters that need to be rendered separately to
    // prevent either an unusual character from forcing a fallback font on the
    // entire run, or brackets from being affected by a fallback font.
    // http://crbug.com/278913, http://crbug.com/396776
    if (run_break > run->range.start())
      run_break = FindRunBreakingCharacter(text, run->range.start(), run_break);

    DCHECK(IsValidCodePointIndex(text, run_break));
    style.UpdatePosition(DisplayIndexToTextIndex(run_break));
    run->range.set_end(run_break);

    run_list_out->add(run);
  }

  // Undo the temporarily applied composition underlines and selection colors.
  UndoCompositionAndSelectionStyles();

  run_list_out->InitIndexMap();
}

bool RenderTextHarfBuzz::CompareFamily(
    const base::string16& text,
    const std::string& family,
    const gfx::FontRenderParams& render_params,
    internal::TextRunHarfBuzz* run,
    std::string* best_family,
    gfx::FontRenderParams* best_render_params,
    size_t* best_missing_glyphs) {
  if (!ShapeRunWithFont(text, family, render_params, run))
    return false;

  const size_t missing_glyphs = run->CountMissingGlyphs();
  if (missing_glyphs < *best_missing_glyphs) {
    *best_family = family;
    *best_render_params = render_params;
    *best_missing_glyphs = missing_glyphs;
  }
  return missing_glyphs == 0;
}

void RenderTextHarfBuzz::ShapeRunList(const base::string16& text,
                                      internal::TextRunList* run_list) {
  for (auto* run : run_list->runs())
    ShapeRun(text, run);
  run_list->ComputePrecedingRunWidths();
}

void RenderTextHarfBuzz::ShapeRun(const base::string16& text,
                                  internal::TextRunHarfBuzz* run) {
  const Font& primary_font = font_list().GetPrimaryFont();
  const std::string primary_family = primary_font.GetFontName();
  run->font_size = primary_font.GetFontSize();

  std::string best_family;
  FontRenderParams best_render_params;
  size_t best_missing_glyphs = std::numeric_limits<size_t>::max();

  for (const Font& font : font_list().GetFonts()) {
    if (CompareFamily(text, font.GetFontName(), font.GetFontRenderParams(),
                      run, &best_family, &best_render_params,
                      &best_missing_glyphs))
      return;
  }

#if defined(OS_WIN)
  Font uniscribe_font;
  std::string uniscribe_family;
  const base::char16* run_text = &(text[run->range.start()]);
  if (GetUniscribeFallbackFont(primary_font, run_text, run->range.length(),
                               &uniscribe_font)) {
    uniscribe_family = uniscribe_font.GetFontName();
    if (CompareFamily(text, uniscribe_family,
                      uniscribe_font.GetFontRenderParams(), run,
                      &best_family, &best_render_params, &best_missing_glyphs))
      return;
  }
#endif

  std::vector<std::string> fallback_families =
      GetFallbackFontFamilies(primary_family);

#if defined(OS_WIN)
  // Append fonts in the fallback list of the Uniscribe font.
  if (!uniscribe_family.empty()) {
    std::vector<std::string> uniscribe_fallbacks =
        GetFallbackFontFamilies(uniscribe_family);
    fallback_families.insert(fallback_families.end(),
        uniscribe_fallbacks.begin(), uniscribe_fallbacks.end());
  }
#endif

  // Try shaping with the fallback fonts.
  for (const auto& family : fallback_families) {
    if (family == primary_family)
      continue;
#if defined(OS_WIN)
    if (family == uniscribe_family)
      continue;
#endif
    FontRenderParamsQuery query(false);
    query.families.push_back(family);
    query.pixel_size = run->font_size;
    query.style = run->font_style;
    FontRenderParams fallback_render_params = GetFontRenderParams(query, NULL);
    if (CompareFamily(text, family, fallback_render_params, run, &best_family,
                      &best_render_params, &best_missing_glyphs))
      return;
  }

  if (!best_family.empty() &&
      (best_family == run->family ||
       ShapeRunWithFont(text, best_family, best_render_params, run)))
    return;

  run->glyph_count = 0;
  run->width = 0.0f;
}

bool RenderTextHarfBuzz::ShapeRunWithFont(const base::string16& text,
                                          const std::string& font_family,
                                          const FontRenderParams& params,
                                          internal::TextRunHarfBuzz* run) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile0(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont0"));

  skia::RefPtr<SkTypeface> skia_face =
      internal::CreateSkiaTypeface(font_family, run->font_style);
  if (skia_face == NULL)
    return false;
  run->skia_face = skia_face;
  run->family = font_family;
  run->render_params = params;

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile01(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont01"));

  hb_font_t* harfbuzz_font = CreateHarfBuzzFont(
      run->skia_face.get(), SkIntToScalar(run->font_size), run->render_params,
      subpixel_rendering_suppressed());

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont1"));

  // Create a HarfBuzz buffer and add the string to be shaped. The HarfBuzz
  // buffer holds our text, run information to be used by the shaping engine,
  // and the resulting glyph data.
  hb_buffer_t* buffer = hb_buffer_create();

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile1q(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont11"));

  hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16*>(text.c_str()),
                      text.length(), run->range.start(), run->range.length());

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile12(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont12"));

  hb_buffer_set_script(buffer, ICUScriptToHBScript(run->script));

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile13(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont13"));

  hb_buffer_set_direction(buffer,
      run->is_rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile14(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont14"));

  // TODO(ckocagil): Should we determine the actual language?
  hb_buffer_set_language(buffer, hb_language_get_default());

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile15(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont15"));

  // Shape the text.
  hb_shape(harfbuzz_font, buffer, NULL, 0);

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont2"));

  // Populate the run fields with the resulting glyph data in the buffer.
  unsigned int glyph_count = 0;
  hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
  run->glyph_count = glyph_count;
  hb_glyph_position_t* hb_positions =
      hb_buffer_get_glyph_positions(buffer, NULL);
  run->glyphs.reset(new uint16[run->glyph_count]);
  run->glyph_to_char.resize(run->glyph_count);
  run->positions.reset(new SkPoint[run->glyph_count]);
  run->width = 0.0f;

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "431326 RenderTextHarfBuzz::ShapeRunWithFont3"));

  for (size_t i = 0; i < run->glyph_count; ++i) {
    DCHECK_LE(infos[i].codepoint, std::numeric_limits<uint16>::max());
    run->glyphs[i] = static_cast<uint16>(infos[i].codepoint);
    run->glyph_to_char[i] = infos[i].cluster;
    const SkScalar x_offset = SkFixedToScalar(hb_positions[i].x_offset);
    const SkScalar y_offset = SkFixedToScalar(hb_positions[i].y_offset);
    run->positions[i].set(run->width + x_offset, -y_offset);
    run->width += (glyph_width_for_test_ > 0)
                      ? glyph_width_for_test_
                      : SkFixedToFloat(hb_positions[i].x_advance);
    // Round run widths if subpixel positioning is off to match native behavior.
    if (!run->render_params.subpixel_positioning)
      run->width = std::floor(run->width + 0.5f);
  }

  hb_buffer_destroy(buffer);
  hb_font_destroy(harfbuzz_font);
  return true;
}

void RenderTextHarfBuzz::EnsureLayoutRunList() {
  if (update_layout_run_list_) {
    layout_run_list_.Reset();

    const base::string16& text = layout_text();
    if (!text.empty()) {
      TRACE_EVENT0("ui", "RenderTextHarfBuzz:EnsureLayoutRunList");
      // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile1(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "431326 RenderTextHarfBuzz::EnsureLayout1"));
      ItemizeTextToRuns(text, &layout_run_list_);

      // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile2(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "431326 RenderTextHarfBuzz::EnsureLayout12"));
      ShapeRunList(text, &layout_run_list_);
    }

    // TODO(vadimt): Remove ScopedTracker below once crbug.com/431326 is
    // fixed.
    tracked_objects::ScopedTracker tracking_profile14(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "431326 RenderTextHarfBuzz::EnsureLayout14"));

    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
    display_run_list_.reset();
    update_display_text_ = true;
    update_layout_run_list_ = false;
  }
  if (update_display_text_) {
    UpdateDisplayText(multiline() ? 0 : layout_run_list_.width());
    update_display_text_ = false;
    update_display_run_list_ = text_elided();
  }
}

base::i18n::BreakIterator* RenderTextHarfBuzz::GetGraphemeIterator() {
  if (update_grapheme_iterator_) {
    update_grapheme_iterator_ = false;
    grapheme_iterator_.reset(new base::i18n::BreakIterator(
        GetDisplayText(),
        base::i18n::BreakIterator::BREAK_CHARACTER));
    if (!grapheme_iterator_->Init())
      grapheme_iterator_.reset();
  }
  return grapheme_iterator_.get();
}

size_t RenderTextHarfBuzz::TextIndexToGivenTextIndex(
    const base::string16& given_text,
    size_t index) {
  DCHECK(given_text == layout_text() || given_text == display_text());
  DCHECK_LE(index, text().length());
  ptrdiff_t i = obscured() ? UTF16IndexToOffset(text(), 0, index) : index;
  CHECK_GE(i, 0);
  // Clamp indices to the length of the given layout or display text.
  return std::min<size_t>(given_text.length(), i);
}

internal::TextRunList* RenderTextHarfBuzz::GetRunList() {
  DCHECK(!update_layout_run_list_);
  DCHECK(!update_display_run_list_);
  return text_elided() ? display_run_list_.get() : &layout_run_list_;
}

const internal::TextRunList* RenderTextHarfBuzz::GetRunList() const {
  return const_cast<RenderTextHarfBuzz*>(this)->GetRunList();
}

}  // namespace gfx
