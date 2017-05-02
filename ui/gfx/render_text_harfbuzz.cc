// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_harfbuzz.h"

#include <limits>
#include <set>

#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/bidi_line_iterator.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/harfbuzz-ng/src/hb.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/decorated_text.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/harfbuzz_font_skia.h"
#include "ui/gfx/range/range_f.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/utf16_indexing.h"

#if defined(OS_WIN)
#include "ui/gfx/font_fallback_win.h"
#endif

namespace gfx {

namespace {

// Text length limit. Longer strings are slow and not fully tested.
const size_t kMaxTextLength = 10000;

// The maximum number of scripts a Unicode character can belong to. This value
// is arbitrarily chosen to be a good limit because it is unlikely for a single
// character to belong to more scripts.
const size_t kMaxScripts = 5;

// Returns true if characters of |block_code| may trigger font fallback.
bool IsUnusualBlockCode(UBlockCode block_code) {
  return block_code == UBLOCK_GEOMETRIC_SHAPES ||
         block_code == UBLOCK_MISCELLANEOUS_SYMBOLS;
}

bool IsBracket(UChar32 character) {
  static const char kBrackets[] = { '(', ')', '{', '}', '<', '>', };
  static const char* kBracketsEnd = kBrackets + arraysize(kBrackets);
  return std::find(kBrackets, kBracketsEnd, character) != kBracketsEnd;
}

// If the given scripts match, returns the one that isn't USCRIPT_INHERITED,
// i.e. the more specific one. Otherwise returns USCRIPT_INVALID_CODE. This
// function is used to split runs between characters of different script codes,
// unless either character has USCRIPT_INHERITED property. See crbug.com/448909.
UScriptCode ScriptIntersect(UScriptCode first, UScriptCode second) {
  if (first == second || second == USCRIPT_INHERITED)
    return first;
  if (first == USCRIPT_INHERITED)
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

// Returns true if |first_char| and |current_char| both have "COMMON" script
// property but only one of them is an ASCII character. By doing this ASCII
// characters will be put into a separate run and be rendered using its default
// font. See crbug.com/530021 and crbug.com/533721 for more details.
bool AsciiBreak(UChar32 first_char, UChar32 current_char) {
  if (isascii(first_char) == isascii(current_char))
    return false;

  size_t scripts_size = 1;
  UScriptCode scripts[kMaxScripts] = { USCRIPT_COMMON };
  ScriptSetIntersect(first_char, scripts, &scripts_size);
  if (scripts_size == 0)
    return false;
  ScriptSetIntersect(current_char, scripts, &scripts_size);
  return scripts_size != 0;
}

// Returns the boundary between a special and a regular character. Special
// characters are brackets or characters that satisfy |IsUnusualBlockCode|.
size_t FindRunBreakingCharacter(const base::string16& text,
                                size_t run_start,
                                size_t run_break) {
  const int32_t run_length = static_cast<int32_t>(run_break - run_start);
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
        first_bracket != IsBracket(current_char) ||
        AsciiBreak(first_char, current_char)) {
      return run_start + iter.array_pos();
    }
  }
  return run_break;
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
    // Special handling to merge white space into the previous run.
    if (u_isUWhiteSpace(char_iterator.get()))
      continue;
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

// Whether |segment| corresponds to the newline character.
bool IsNewlineSegment(const base::string16& text,
                      const internal::LineSegment& segment) {
  return text[segment.char_range.start()] == '\n';
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

// Returns the line segment index for the |line|, |text_x| pair. |text_x| is
// relative to text in the given line. Returns -1 if |text_x| is to the left
// of text in the line and |line|.segments.size() if it's to the right.
// |offset_relative_segment| will contain the offset of |text_x| relative to
// the start of the segment it is contained in.
int GetLineSegmentContainingXCoord(const internal::Line& line,
                                   float line_x,
                                   float* offset_relative_segment) {
  DCHECK(offset_relative_segment);

  *offset_relative_segment = 0;
  if (line_x < 0)
    return -1;
  for (size_t i = 0; i < line.segments.size(); i++) {
    const internal::LineSegment& segment = line.segments[i];

    // segment.x_range is not used because it is in text space.
    if (line_x < segment.width()) {
      *offset_relative_segment = line_x;
      return i;
    }
    line_x -= segment.width();
  }
  return line.segments.size();
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
                      WordWrapBehavior word_wrap_behavior,
                      const base::string16& text,
                      const BreakList<size_t>* words,
                      const internal::TextRunList& run_list)
      : max_width_((max_width == 0) ? SK_ScalarMax : SkIntToScalar(max_width)),
        min_baseline_(min_baseline),
        min_height_(min_height),
        word_wrap_behavior_(word_wrap_behavior),
        text_(text),
        words_(words),
        run_list_(run_list),
        max_descent_(0),
        max_ascent_(0),
        text_x_(0),
        available_width_(max_width_) {
    AdvanceLine();
  }

  // Constructs a single line for |text_| using |run_list_|.
  void ConstructSingleLine() {
    for (size_t i = 0; i < run_list_.size(); i++) {
      const internal::TextRunHarfBuzz& run = *(run_list_.runs()[i]);
      internal::LineSegment segment;
      segment.run = i;
      segment.char_range = run.range;
      segment.x_range = RangeF(SkScalarToFloat(text_x_),
                               SkScalarToFloat(text_x_) + run.width);
      AddLineSegment(segment);
    }
  }

  // Constructs multiple lines for |text_| based on words iteration approach.
  void ConstructMultiLines() {
    DCHECK(words_);
    for (auto iter = words_->breaks().begin(); iter != words_->breaks().end();
         iter++) {
      const Range word_range = words_->GetRange(iter);
      std::vector<internal::LineSegment> word_segments;
      SkScalar word_width = GetWordWidth(word_range, &word_segments);

      // If the last word is '\n', we should advance a new line after adding
      // the word to the current line.
      bool new_line = false;
      if (!word_segments.empty() &&
          IsNewlineSegment(text_, word_segments.back())) {
        new_line = true;

        // Since the line should at least contain some information regarding the
        // text range it corresponds to, don't pop the newline segment, if it's
        // the only segment in the line. This ensures that every line has a non-
        // empty segments vector (except the last in some cases). This segment
        // won't be drawn though.
        if (word_segments.size() != 1u || available_width_ != max_width_) {
          word_width -= word_segments.back().width();
          word_segments.pop_back();
        }
      }

      // If the word is not the first word in the line and it can't fit into
      // the current line, advance a new line.
      if (word_width > available_width_ && available_width_ != max_width_)
        AdvanceLine();
      if (!word_segments.empty())
        AddWordToLine(word_segments);
      if (new_line)
        AdvanceLine();
    }
  }

  // Finishes line breaking and outputs the results. Can be called at most once.
  void FinalizeLines(std::vector<internal::Line>* lines, SizeF* size) {
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
      line->baseline = std::max(min_baseline_, SkScalarRoundToInt(max_ascent_));
      line->preceding_heights = std::ceil(total_size_.height());
      total_size_.set_height(total_size_.height() + line->size.height());
      total_size_.set_width(std::max(total_size_.width(), line->size.width()));
    }
    max_descent_ = 0;
    max_ascent_ = 0;
    available_width_ = max_width_;
    lines_.push_back(internal::Line());
  }

  // Adds word to the current line. A word may contain multiple segments. If the
  // word is the first word in line and its width exceeds |available_width_|,
  // ignore/truncate/wrap it according to |word_wrap_behavior_|.
  void AddWordToLine(const std::vector<internal::LineSegment>& word_segments) {
    DCHECK(!lines_.empty());
    DCHECK(!word_segments.empty());

    bool has_truncated = false;
    for (const internal::LineSegment& segment : word_segments) {
      if (has_truncated)
        break;
      if (segment.width() <= available_width_ ||
          word_wrap_behavior_ == IGNORE_LONG_WORDS) {
        AddLineSegment(segment);
      } else {
        DCHECK(word_wrap_behavior_ == TRUNCATE_LONG_WORDS ||
               word_wrap_behavior_ == WRAP_LONG_WORDS);
        has_truncated = (word_wrap_behavior_ == TRUNCATE_LONG_WORDS);

        const internal::TextRunHarfBuzz& run = *(run_list_.runs()[segment.run]);
        internal::LineSegment remaining_segment = segment;
        while (!remaining_segment.char_range.is_empty()) {
          size_t cutoff_pos = GetCutoffPos(remaining_segment);
          SkScalar width = run.GetGlyphWidthForCharRange(
              Range(remaining_segment.char_range.start(), cutoff_pos));
          if (width > 0) {
            internal::LineSegment cut_segment;
            cut_segment.run = remaining_segment.run;
            cut_segment.char_range =
                Range(remaining_segment.char_range.start(), cutoff_pos);
            cut_segment.x_range = RangeF(SkScalarToFloat(text_x_),
                                         SkScalarToFloat(text_x_ + width));
            AddLineSegment(cut_segment);
            // Updates old segment range.
            remaining_segment.char_range.set_start(cutoff_pos);
            remaining_segment.x_range.set_start(SkScalarToFloat(text_x_));
          }
          if (has_truncated)
            break;
          if (!remaining_segment.char_range.is_empty())
            AdvanceLine();
        }
      }
    }
  }

  // Add a line segment to the current line. Note that, in order to keep the
  // visual order correct for ltr and rtl language, we need to merge segments
  // that belong to the same run.
  void AddLineSegment(const internal::LineSegment& segment) {
    DCHECK(!lines_.empty());
    internal::Line* line = &lines_.back();
    const internal::TextRunHarfBuzz& run = *(run_list_.runs()[segment.run]);
    if (!line->segments.empty()) {
      internal::LineSegment& last_segment = line->segments.back();
      // Merge segments that belong to the same run.
      if (last_segment.run == segment.run) {
        DCHECK_EQ(last_segment.char_range.end(), segment.char_range.start());
        // Check there is less than a pixel between one run and the next.
        DCHECK_LE(
            std::abs(last_segment.x_range.end() - segment.x_range.start()),
            1.0f);
        last_segment.char_range.set_end(segment.char_range.end());
        last_segment.x_range.set_end(SkScalarToFloat(text_x_) +
                                     segment.width());
        if (run.is_rtl && last_segment.char_range.end() == run.range.end())
          UpdateRTLSegmentRanges();
        line->size.set_width(line->size.width() + segment.width());
        text_x_ += segment.width();
        available_width_ -= segment.width();
        return;
      }
    }
    line->segments.push_back(segment);

    SkPaint paint;
    paint.setTypeface(run.skia_face);
    paint.setTextSize(SkIntToScalar(run.font_size));
    paint.setAntiAlias(run.render_params.antialiasing);
    SkPaint::FontMetrics metrics;
    paint.getFontMetrics(&metrics);

    line->size.set_width(line->size.width() + segment.width());
    // TODO(dschuyler): Account for stylized baselines in string sizing.
    max_descent_ = std::max(max_descent_, metrics.fDescent);
    // fAscent is always negative.
    max_ascent_ = std::max(max_ascent_, -metrics.fAscent);

    if (run.is_rtl) {
      rtl_segments_.push_back(
          SegmentHandle(lines_.size() - 1, line->segments.size() - 1));
      // If this is the last segment of an RTL run, reprocess the text-space x
      // ranges of all segments from the run.
      if (segment.char_range.end() == run.range.end())
        UpdateRTLSegmentRanges();
    }
    text_x_ += segment.width();
    available_width_ -= segment.width();
  }

  // Finds the end position |end_pos| in |segment| where the preceding width is
  // no larger than |available_width_|.
  size_t GetCutoffPos(const internal::LineSegment& segment) const {
    DCHECK(!segment.char_range.is_empty());
    const internal::TextRunHarfBuzz& run =
        *(run_list_.runs()[segment.run]).get();
    size_t end_pos = segment.char_range.start();
    SkScalar width = 0;
    while (end_pos < segment.char_range.end()) {
      const SkScalar char_width =
          run.GetGlyphWidthForCharRange(Range(end_pos, end_pos + 1));
      if (width + char_width > available_width_)
        break;
      width += char_width;
      end_pos++;
    }

    const size_t valid_end_pos = std::max(
        segment.char_range.start(),
        static_cast<uint32_t>(FindValidBoundaryBefore(text_, end_pos)));
    if (end_pos != valid_end_pos) {
      end_pos = valid_end_pos;
      width = run.GetGlyphWidthForCharRange(
          Range(segment.char_range.start(), end_pos));
    }

    // |max_width_| might be smaller than a single character. In this case we
    // need to put at least one character in the line. Note that, we should
    // not separate surrogate pair or combining characters.
    // See RenderTextHarfBuzzTest.Multiline_MinWidth for an example.
    if (width == 0 && available_width_ == max_width_) {
      end_pos = std::min(
          segment.char_range.end(),
          static_cast<uint32_t>(FindValidBoundaryAfter(text_, end_pos + 1)));
    }

    return end_pos;
  }

  // Gets the glyph width for |word_range|, and splits the |word| into different
  // segments based on its runs.
  SkScalar GetWordWidth(const Range& word_range,
                        std::vector<internal::LineSegment>* segments) const {
    DCHECK(words_);
    if (word_range.is_empty() || segments == nullptr)
      return 0;
    size_t run_start_index = run_list_.GetRunIndexAt(word_range.start());
    size_t run_end_index = run_list_.GetRunIndexAt(word_range.end() - 1);
    SkScalar width = 0;
    for (size_t i = run_start_index; i <= run_end_index; i++) {
      const internal::TextRunHarfBuzz& run = *(run_list_.runs()[i]);
      const Range char_range = run.range.Intersect(word_range);
      DCHECK(!char_range.is_empty());
      const SkScalar char_width = run.GetGlyphWidthForCharRange(char_range);
      width += char_width;

      internal::LineSegment segment;
      segment.run = i;
      segment.char_range = char_range;
      segment.x_range = RangeF(SkScalarToFloat(text_x_ + width - char_width),
                               SkScalarToFloat(text_x_ + width));
      segments->push_back(segment);
    }
    return width;
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
      const float segment_width = segment->width();
      segment->x_range = RangeF(x, x + segment_width);
      x += segment_width;
    }
    rtl_segments_.clear();
  }

  const SkScalar max_width_;
  const int min_baseline_;
  const float min_height_;
  const WordWrapBehavior word_wrap_behavior_;
  const base::string16& text_;
  const BreakList<size_t>* const words_;
  const internal::TextRunList& run_list_;

  // Stores the resulting lines.
  std::vector<internal::Line> lines_;

  float max_descent_;
  float max_ascent_;

  // Text space x coordinates of the next segment to be added.
  SkScalar text_x_;
  // Stores available width in the current line.
  SkScalar available_width_;

  // Size of the multiline text, not including the currently processed line.
  SizeF total_size_;

  // The current RTL run segments, to be applied by |UpdateRTLSegmentRanges()|.
  std::vector<SegmentHandle> rtl_segments_;

  DISALLOW_COPY_AND_ASSIGN(HarfBuzzLineBreaker);
};

// Function object for case insensitive string comparison.
struct CaseInsensitiveCompare {
  bool operator() (const Font& a, const Font& b) const {
    return base::CompareCaseInsensitiveASCII(a.GetFontName(), b.GetFontName()) <
           0;
  }
};

// Applies a forced text rendering direction if specified by a command-line
// switch.
void CheckForcedDirection(UBiDiLevel* level) {
  static bool has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceTextDirection);
  if (!has_switch)
    return;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceTextDirection)) {
    std::string force_flag =
        command_line->GetSwitchValueASCII(switches::kForceTextDirection);

    if (force_flag == switches::kForceDirectionRTL)
      *level = UBIDI_RTL;
    if (force_flag == switches::kForceDirectionLTR)
      *level = UBIDI_LTR;
  }
}

}  // namespace

namespace internal {

#if !defined(OS_MACOSX)
sk_sp<SkTypeface> CreateSkiaTypeface(const Font& font,
                                     bool italic,
                                     Font::Weight weight) {
  SkFontStyle skia_style(
      static_cast<int>(weight), SkFontStyle::kNormal_Width,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
  return sk_sp<SkTypeface>(SkTypeface::MakeFromName(
      font.GetFontName().c_str(), skia_style));
}
#endif

TextRunHarfBuzz::TextRunHarfBuzz(const Font& template_font)
    : width(0.0f),
      preceding_run_widths(0.0f),
      is_rtl(false),
      level(0),
      script(USCRIPT_INVALID_CODE),
      glyph_count(static_cast<size_t>(-1)),
      font(template_font),
      font_size(0),
      baseline_offset(0),
      baseline_type(0),
      italic(false),
      weight(Font::Weight::NORMAL),
      strike(false),
      diagonal_strike(false),
      underline(false) {}

TextRunHarfBuzz::~TextRunHarfBuzz() {}

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

RangeF TextRunHarfBuzz::GetGraphemeBounds(RenderTextHarfBuzz* render_text,
                                          size_t text_index) const {
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
  // Note: The first call to GetGraphemeIterator() can be expensive, so avoid
  // doing it unless it's actually needed (when length > 1).
  if (chars.length() > 1 && render_text->GetGraphemeIterator()) {
    int before = 0;
    int total = 0;
    base::i18n::BreakIterator* grapheme_iterator =
        render_text->GetGraphemeIterator();
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

float TextRunHarfBuzz::GetGraphemeWidthForCharRange(
    RenderTextHarfBuzz* render_text,
    const Range& char_range) const {
  if (char_range.is_empty())
    return 0;
  DCHECK(!char_range.is_reversed());
  DCHECK(range.Contains(char_range));
  size_t left_index = is_rtl ? char_range.end() - 1 : char_range.start();
  size_t right_index = is_rtl ? char_range.start() : char_range.end() - 1;
  return GetGraphemeBounds(render_text, right_index).GetMax() -
         GetGraphemeBounds(render_text, left_index).GetMin();
}

SkScalar TextRunHarfBuzz::GetGlyphWidthForCharRange(
    const Range& char_range) const {
  if (char_range.is_empty())
    return 0;

  DCHECK(range.Contains(char_range));
  Range glyph_range = CharRangeToGlyphRange(char_range);

  // The |glyph_range| might be empty or invalid on Windows if a multi-character
  // grapheme is divided into different runs (e.g., there are two font sizes or
  // colors for a single glyph). In this case it might cause the browser crash,
  // see crbug.com/526234.
  if (glyph_range.start() >= glyph_range.end()) {
    NOTREACHED() << "The glyph range is empty or invalid! Its char range: ["
        << char_range.start() << ", " << char_range.end()
        << "], and its glyph range: [" << glyph_range.start() << ", "
        << glyph_range.end() << "].";
    return 0;
  }

  return ((glyph_range.end() == glyph_count)
              ? SkFloatToScalar(width)
              : positions[glyph_range.end()].x()) -
         positions[glyph_range.start()].x();
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
    const auto& run = runs_[visual_to_logical_[i]];
    run->preceding_run_widths = width_;
    width_ += run->width;
  }
}

size_t TextRunList::GetRunIndexAt(size_t position) const {
  for (size_t i = 0; i < runs_.size(); ++i) {
    if (runs_[i]->range.start() <= position && runs_[i]->range.end() > position)
      return i;
  }
  return runs_.size();
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

std::unique_ptr<RenderText> RenderTextHarfBuzz::CreateInstanceOfSameType()
    const {
  return base::WrapUnique(new RenderTextHarfBuzz);
}

bool RenderTextHarfBuzz::MultilineSupported() const {
  return true;
}

const base::string16& RenderTextHarfBuzz::GetDisplayText() {
  // TODO(krb): Consider other elision modes for multiline.
  if ((multiline() && (max_lines() == 0 || elide_behavior() != ELIDE_TAIL)) ||
      elide_behavior() == NO_ELIDE || elide_behavior() == FADE_TAIL) {
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

SelectionModel RenderTextHarfBuzz::FindCursorPosition(const Point& view_point) {
  EnsureLayout();
  DCHECK(!lines().empty());

  int line_index = GetLineContainingYCoord((view_point - GetLineOffset(0)).y());
  // Clip line index to a valid value in case kDragToEndIfOutsideVerticalBounds
  // is false. Else, drag to end.
  if (line_index < 0) {
    if (RenderText::kDragToEndIfOutsideVerticalBounds)
      return EdgeSelectionModel(GetVisualDirectionOfLogicalBeginning());
    else
      line_index = 0;
  }
  if (line_index >= static_cast<int>(lines().size())) {
    if (RenderText::kDragToEndIfOutsideVerticalBounds)
      return EdgeSelectionModel(GetVisualDirectionOfLogicalEnd());
    else
      line_index = lines().size() - 1;
  }
  const internal::Line& line = lines()[line_index];

  float point_offset_relative_segment = 0;
  const int segment_index = GetLineSegmentContainingXCoord(
      line, (view_point - GetLineOffset(line_index)).x(),
      &point_offset_relative_segment);
  if (segment_index < 0)
    return LineSelectionModel(line_index, CURSOR_LEFT);
  if (segment_index >= static_cast<int>(line.segments.size()))
    return LineSelectionModel(line_index, CURSOR_RIGHT);
  const internal::LineSegment& segment = line.segments[segment_index];

  const internal::TextRunHarfBuzz& run = *GetRunList()->runs()[segment.run];
  const size_t segment_min_glyph_index =
      run.CharRangeToGlyphRange(segment.char_range).GetMin();
  const float segment_offset_relative_run =
      segment_min_glyph_index != 0
          ? SkScalarToFloat(run.positions[segment_min_glyph_index].x())
          : 0;
  const float point_offset_relative_run =
      point_offset_relative_segment + segment_offset_relative_run;

  // TODO(crbug.com/676287): Use offset within the glyph to return the correct
  // grapheme position within a multi-grapheme glyph.
  for (size_t i = 0; i < run.glyph_count; ++i) {
    const float end = i + 1 == run.glyph_count
                          ? run.width
                          : SkScalarToFloat(run.positions[i + 1].x());
    const float middle = (end + SkScalarToFloat(run.positions[i].x())) / 2;
    const size_t index = DisplayIndexToTextIndex(run.glyph_to_char[i]);
    if (point_offset_relative_run < middle) {
      return run.is_rtl ? SelectionModel(
                              IndexOfAdjacentGrapheme(index, CURSOR_FORWARD),
                              CURSOR_BACKWARD)
                        : SelectionModel(index, CURSOR_FORWARD);
    }
    if (point_offset_relative_run < end) {
      return run.is_rtl ? SelectionModel(index, CURSOR_FORWARD)
                        : SelectionModel(
                              IndexOfAdjacentGrapheme(index, CURSOR_FORWARD),
                              CURSOR_BACKWARD);
    }
  }

  return LineSelectionModel(line_index, CURSOR_RIGHT);
}

bool RenderTextHarfBuzz::IsSelectionSupported() const {
  return true;
}

std::vector<RenderText::FontSpan> RenderTextHarfBuzz::GetFontSpansForTesting() {
  EnsureLayout();

  internal::TextRunList* run_list = GetRunList();
  std::vector<RenderText::FontSpan> spans;
  for (const auto& run : run_list->runs()) {
    spans.push_back(RenderText::FontSpan(
        run->font, Range(DisplayIndexToTextIndex(run->range.start()),
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
  internal::TextRunHarfBuzz* run = run_list->runs()[run_index].get();
  RangeF bounds = run->GetGraphemeBounds(this, layout_index);
  // If cursor is enabled, extend the last glyph up to the rightmost cursor
  // position since clients expect them to be contiguous.
  if (cursor_enabled() && run_index == run_list->size() - 1 &&
      index == (run->is_rtl ? run->range.start() : run->range.end() - 1))
    bounds.set_end(std::ceil(bounds.end()));
  return run->is_rtl ? RangeF(bounds.end(), bounds.start()).Round()
                     : bounds.Round();
}

base::i18n::BreakIterator* RenderTextHarfBuzz::GetGraphemeIterator() {
  if (update_grapheme_iterator_) {
    update_grapheme_iterator_ = false;
    grapheme_iterator_.reset(new base::i18n::BreakIterator(
        GetDisplayText(), base::i18n::BreakIterator::BREAK_CHARACTER));
    if (!grapheme_iterator_->Init())
      grapheme_iterator_.reset();
  }
  return grapheme_iterator_.get();
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
    run = run_list->runs()[run_list->visual_to_logical(visual_index)].get();
  } else {
    // If the cursor is moving within the current run, just move it by one
    // grapheme in the appropriate direction.
    run = run_list->runs()[run_index].get();
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
    run = run_list->runs()[run_list->visual_to_logical(visual_index)].get();
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
  const size_t start =
      IsValidCursorIndex(range.GetMin())
          ? range.GetMin()
          : IndexOfAdjacentGrapheme(range.GetMin(), CURSOR_BACKWARD);
  const size_t end =
      IsValidCursorIndex(range.GetMax())
          ? range.GetMax()
          : IndexOfAdjacentGrapheme(range.GetMax(), CURSOR_FORWARD);
  Range display_range(TextIndexToDisplayIndex(start),
                      TextIndexToDisplayIndex(end));
  DCHECK(Range(0, GetDisplayText().length()).Contains(display_range));

  std::vector<Rect> rects;
  if (display_range.is_empty())
    return rects;

  internal::TextRunList* run_list = GetRunList();
  for (size_t line_index = 0; line_index < lines().size(); ++line_index) {
    const internal::Line& line = lines()[line_index];
    // Only the last line can be empty.
    DCHECK(!line.segments.empty() || (line_index == lines().size() - 1));

    float line_x = 0;
    for (const internal::LineSegment& segment : line.segments) {
      const Range intersection = segment.char_range.Intersect(display_range);
      DCHECK(!intersection.is_reversed());
      if (!intersection.is_empty()) {
        const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
        float width = SkScalarToFloat(
            run.GetGraphemeWidthForCharRange(this, intersection));
        float x = line_x;
        if (run.is_rtl) {
          x += SkScalarToFloat(run.GetGraphemeWidthForCharRange(
              this, gfx::Range(intersection.end(), segment.char_range.end())));
        } else {
          x += SkScalarToFloat(run.GetGraphemeWidthForCharRange(
              this,
              gfx::Range(segment.char_range.start(), intersection.start())));
        }
        int end_x = std::ceil(x + width);
        int start_x = std::ceil(x);
        gfx::Rect rect(start_x, 0, end_x - start_x, line.size.height());
        rects.push_back(rect + GetLineOffset(line_index));
      }
      line_x += segment.width();
    }
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
  EnsureLayoutRunList();

  if (update_display_run_list_) {
    DCHECK(text_elided());
    const base::string16& display_text = GetDisplayText();
    display_run_list_.reset(new internal::TextRunList);

    if (!display_text.empty()) {
      TRACE_EVENT0("ui", "RenderTextHarfBuzz:EnsureLayout1");

      ItemizeTextToRuns(display_text, display_run_list_.get());

      // TODO(ckocagil): Remove ScopedTracker below once crbug.com/441028 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile(
          FROM_HERE_WITH_EXPLICIT_FUNCTION("441028 ShapeRunList() 1"));
      ShapeRunList(display_text, display_run_list_.get());
    }
    update_display_run_list_ = false;
    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
  }

  if (lines().empty()) {
    // TODO(ckocagil): Remove ScopedTracker below once crbug.com/441028 is
    // fixed.
    std::unique_ptr<tracked_objects::ScopedTracker> tracking_profile(
        new tracked_objects::ScopedTracker(
            FROM_HERE_WITH_EXPLICIT_FUNCTION("441028 HarfBuzzLineBreaker")));

    internal::TextRunList* run_list = GetRunList();
    const int height = std::max(font_list().GetHeight(), min_line_height());
    HarfBuzzLineBreaker line_breaker(
        display_rect().width(),
        DetermineBaselineCenteringText(height, font_list()), height,
        word_wrap_behavior(), GetDisplayText(),
        multiline() ? &GetLineBreaks() : nullptr, *run_list);

    tracking_profile.reset();

    if (multiline())
      line_breaker.ConstructMultiLines();
    else
      line_breaker.ConstructSingleLine();
    std::vector<internal::Line> lines;
    line_breaker.FinalizeLines(&lines, &total_size_);
    set_lines(&lines);
  }
}

void RenderTextHarfBuzz::DrawVisualText(internal::SkiaTextRenderer* renderer) {
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
      // Don't draw the newline glyph (crbug.com/680430).
      if (IsNewlineSegment(GetDisplayText(), segment))
        continue;

      const internal::TextRunHarfBuzz& run = *run_list->runs()[segment.run];
      renderer->SetTypeface(run.skia_face);
      renderer->SetTextSize(SkIntToScalar(run.font_size));
      renderer->SetFontRenderParams(run.render_params,
                                    subpixel_rendering_suppressed());
      Range glyphs_range = run.CharRangeToGlyphRange(segment.char_range);
      std::unique_ptr<SkPoint[]> positions(new SkPoint[glyphs_range.length()]);
      SkScalar offset_x = preceding_segment_widths -
                          ((glyphs_range.GetMin() != 0)
                               ? run.positions[glyphs_range.GetMin()].x()
                               : 0);
      for (size_t j = 0; j < glyphs_range.length(); ++j) {
        positions[j] = run.positions[(glyphs_range.is_reversed()) ?
                                     (glyphs_range.start() - j) :
                                     (glyphs_range.start() + j)];
        positions[j].offset(SkIntToScalar(origin.x()) + offset_x,
                            SkIntToScalar(origin.y() + run.baseline_offset));
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
                ? (SkFloatToScalar(segment.width()) + preceding_segment_widths +
                   SkIntToScalar(origin.x()))
                : positions[colored_glyphs.end() - glyphs_range.start()].x());
        renderer->DrawDecorations(start_x, origin.y(), end_x - start_x,
                                  run.underline, run.strike,
                                  run.diagonal_strike);
      }
      preceding_segment_widths += SkFloatToScalar(segment.width());
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
    internal::TextRunHarfBuzz* run = run_list->runs()[i].get();
    if (RangeContainsCaret(run->range, layout_position, affinity))
      return i;
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
  DCHECK_NE(0U, text.length());

  // If ICU fails to itemize the text, we create a run that spans the entire
  // text. This is needed because leaving the runs set empty causes some clients
  // to misbehave since they expect non-zero text metrics from a non-empty text.
  base::i18n::BiDiLineIterator bidi_iterator;
  if (!bidi_iterator.Open(text, GetTextDirection(text))) {
    auto run = base::MakeUnique<internal::TextRunHarfBuzz>(
        font_list().GetPrimaryFont());
    run->range = Range(0, text.length());
    run_list_out->Add(std::move(run));
    run_list_out->InitIndexMap();
    return;
  }

  // Temporarily apply composition underlines and selection colors.
  ApplyCompositionAndSelectionStyles();

  // Build the run list from the script items and ranged styles and baselines.
  // Use an empty color BreakList to avoid breaking runs at color boundaries.
  BreakList<SkColor> empty_colors;
  empty_colors.SetMax(colors().max());
  DCHECK_LE(text.size(), baselines().max());
  for (const BreakList<bool>& style : styles())
    DCHECK_LE(text.size(), style.max());
  internal::StyleIterator style(empty_colors, baselines(), weights(), styles());

  for (size_t run_break = 0; run_break < text.length();) {
    auto run = base::MakeUnique<internal::TextRunHarfBuzz>(
        font_list().GetPrimaryFont());
    run->range.set_start(run_break);
    run->italic = style.style(ITALIC);
    run->baseline_type = style.baseline();
    run->strike = style.style(STRIKE);
    run->diagonal_strike = style.style(DIAGONAL_STRIKE);
    run->underline = style.style(UNDERLINE);
    run->weight = style.weight();
    int32_t script_item_break = 0;
    bidi_iterator.GetLogicalRun(run_break, &script_item_break, &run->level);
    CHECK_GT(static_cast<size_t>(script_item_break), run_break);
    CheckForcedDirection(&run->level);
    // Odd BiDi embedding levels correspond to RTL runs.
    run->is_rtl = (run->level % 2) == 1;
    // Find the length and script of this script run.
    script_item_break = ScriptInterval(text, run_break,
        script_item_break - run_break, &run->script) + run_break;

    // Find the next break and advance the iterators as needed.
    const size_t new_run_break = std::min(
        static_cast<size_t>(script_item_break),
        TextIndexToGivenTextIndex(text, style.GetRange().end()));
    CHECK_GT(new_run_break, run_break)
        << "It must proceed! " << text << " " << run_break;
    run_break = new_run_break;

    // Break runs at certain characters that need to be rendered separately to
    // prevent either an unusual character from forcing a fallback font on the
    // entire run, or brackets from being affected by a fallback font.
    // http://crbug.com/278913, http://crbug.com/396776
    if (run_break > run->range.start())
      run_break = FindRunBreakingCharacter(text, run->range.start(), run_break);

    DCHECK(IsValidCodePointIndex(text, run_break));
    style.UpdatePosition(DisplayIndexToTextIndex(run_break));
    run->range.set_end(run_break);

    run_list_out->Add(std::move(run));
  }

  // Undo the temporarily applied composition underlines and selection colors.
  UndoCompositionAndSelectionStyles();

  run_list_out->InitIndexMap();
}

bool RenderTextHarfBuzz::CompareFamily(
    const base::string16& text,
    const Font& font,
    const FontRenderParams& render_params,
    internal::TextRunHarfBuzz* run,
    Font* best_font,
    FontRenderParams* best_render_params,
    size_t* best_missing_glyphs) {
  if (!ShapeRunWithFont(text, font, render_params, run))
    return false;

  const size_t missing_glyphs = run->CountMissingGlyphs();
  if (missing_glyphs < *best_missing_glyphs) {
    *best_font = font;
    *best_render_params = render_params;
    *best_missing_glyphs = missing_glyphs;
  }
  return missing_glyphs == 0;
}

void RenderTextHarfBuzz::ShapeRunList(const base::string16& text,
                                      internal::TextRunList* run_list) {
  for (const auto& run : run_list->runs())
    ShapeRun(text, run.get());
  run_list->ComputePrecedingRunWidths();
}

void RenderTextHarfBuzz::ShapeRun(const base::string16& text,
                                  internal::TextRunHarfBuzz* run) {
  const Font& primary_font = font_list().GetPrimaryFont();
  const std::string primary_family = primary_font.GetFontName();
  run->font_size = primary_font.GetFontSize();
  run->baseline_offset = 0;
  if (run->baseline_type != NORMAL_BASELINE) {
    // Calculate a slightly smaller font. The ratio here is somewhat arbitrary.
    // Proportions from 5/9 to 5/7 all look pretty good.
    const float ratio = 5.0f / 9.0f;
    run->font_size = ToRoundedInt(primary_font.GetFontSize() * ratio);
    switch (run->baseline_type) {
      case SUPERSCRIPT:
        run->baseline_offset =
            primary_font.GetCapHeight() - primary_font.GetHeight();
        break;
      case SUPERIOR:
        run->baseline_offset =
            ToRoundedInt(primary_font.GetCapHeight() * ratio) -
            primary_font.GetCapHeight();
        break;
      case SUBSCRIPT:
        run->baseline_offset =
            primary_font.GetHeight() - primary_font.GetBaseline();
        break;
      case INFERIOR:  // Fall through.
      default:
        break;
    }
  }

  Font best_font(primary_font);
  FontRenderParams best_render_params;
  size_t best_missing_glyphs = std::numeric_limits<size_t>::max();

  for (const Font& font : font_list().GetFonts()) {
    if (CompareFamily(text, font, font.GetFontRenderParams(), run, &best_font,
                      &best_render_params, &best_missing_glyphs))
      return;
  }

#if defined(OS_WIN)
  Font fallback_font(primary_font);
  std::string fallback_family;
  const base::char16* run_text = &(text[run->range.start()]);
  if (GetFallbackFont(primary_font, run_text, run->range.length(),
                      &fallback_font)) {
    fallback_family = fallback_font.GetFontName();
    if (CompareFamily(text, fallback_font, fallback_font.GetFontRenderParams(),
                      run, &best_font, &best_render_params,
                      &best_missing_glyphs))
      return;
  }
#endif

  std::vector<Font> fallback_font_list = GetFallbackFonts(primary_font);

#if defined(OS_WIN)
  // Append fonts in the fallback list of the fallback font.
  if (!fallback_family.empty()) {
    std::vector<Font> fallback_fonts = GetFallbackFonts(fallback_font);
    fallback_font_list.insert(fallback_font_list.end(), fallback_fonts.begin(),
                              fallback_fonts.end());
  }

  // Add Segoe UI and its associated linked fonts to the fallback font list to
  // ensure that the fallback list covers the basic cases.
  // http://crbug.com/467459. On some Windows configurations the default font
  // could be a raster font like System, which would not give us a reasonable
  // fallback font list.
  if (!base::LowerCaseEqualsASCII(primary_font.GetFontName(), "segoe ui") &&
      !base::LowerCaseEqualsASCII(fallback_family, "segoe ui")) {
    std::vector<Font> default_fallback_families =
        GetFallbackFonts(Font("Segoe UI", 13));
    fallback_font_list.insert(fallback_font_list.end(),
        default_fallback_families.begin(), default_fallback_families.end());
  }
#endif

  // Use a set to track the fallback fonts and avoid duplicate entries.
  std::set<Font, CaseInsensitiveCompare> fallback_fonts;

  // Try shaping with the fallback fonts.
  for (const auto& font : fallback_font_list) {
    std::string font_name = font.GetFontName();

    if (font_name == primary_font.GetFontName())
      continue;
#if defined(OS_WIN)
    if (font_name == fallback_family)
      continue;
#endif
    if (fallback_fonts.find(font) != fallback_fonts.end())
      continue;

    fallback_fonts.insert(font);

    FontRenderParamsQuery query;
    query.families.push_back(font_name);
    query.pixel_size = run->font_size;
    query.style = run->italic ? Font::ITALIC : 0;
    FontRenderParams fallback_render_params = GetFontRenderParams(query, NULL);
    if (CompareFamily(text, font, fallback_render_params, run, &best_font,
                      &best_render_params, &best_missing_glyphs))
      return;
  }

  if (best_missing_glyphs != std::numeric_limits<size_t>::max() &&
      (best_font.GetFontName() == run->font.GetFontName() ||
       ShapeRunWithFont(text, best_font, best_render_params, run)))
    return;

  run->glyph_count = 0;
  run->width = 0.0f;
}

bool RenderTextHarfBuzz::ShapeRunWithFont(const base::string16& text,
                                          const Font& font,
                                          const FontRenderParams& params,
                                          internal::TextRunHarfBuzz* run) {
  sk_sp<SkTypeface> skia_face(
      internal::CreateSkiaTypeface(font, run->italic, run->weight));
  if (!skia_face)
    return false;

  run->skia_face = skia_face;
  run->font = font;
  run->render_params = params;

  hb_font_t* harfbuzz_font = CreateHarfBuzzFont(
      run->skia_face, SkIntToScalar(run->font_size), run->render_params,
      subpixel_rendering_suppressed());

  // Create a HarfBuzz buffer and add the string to be shaped. The HarfBuzz
  // buffer holds our text, run information to be used by the shaping engine,
  // and the resulting glyph data.
  hb_buffer_t* buffer = hb_buffer_create();
  hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t*>(text.c_str()),
                      text.length(), run->range.start(), run->range.length());
  hb_buffer_set_script(buffer, ICUScriptToHBScript(run->script));
  hb_buffer_set_direction(buffer,
      run->is_rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  // TODO(ckocagil): Should we determine the actual language?
  hb_buffer_set_language(buffer, hb_language_get_default());

  {
    // TODO(ckocagil): Remove ScopedTracker below once crbug.com/441028 is
    // fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("441028 hb_shape()"));

    // Shape the text.
    hb_shape(harfbuzz_font, buffer, NULL, 0);
  }

  // Populate the run fields with the resulting glyph data in the buffer.
  unsigned int glyph_count = 0;
  hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
  run->glyph_count = glyph_count;
  hb_glyph_position_t* hb_positions =
      hb_buffer_get_glyph_positions(buffer, NULL);
  run->glyphs.reset(new uint16_t[run->glyph_count]);
  run->glyph_to_char.resize(run->glyph_count);
  run->positions.reset(new SkPoint[run->glyph_count]);
  run->width = 0.0f;

  for (size_t i = 0; i < run->glyph_count; ++i) {
    DCHECK_LE(infos[i].codepoint, std::numeric_limits<uint16_t>::max());
    run->glyphs[i] = static_cast<uint16_t>(infos[i].codepoint);
    run->glyph_to_char[i] = infos[i].cluster;
    const SkScalar x_offset =
        HarfBuzzUnitsToSkiaScalar(hb_positions[i].x_offset);
    const SkScalar y_offset =
        HarfBuzzUnitsToSkiaScalar(hb_positions[i].y_offset);
    run->positions[i].set(run->width + x_offset, -y_offset);
    run->width += (glyph_width_for_test_ > 0)
                      ? glyph_width_for_test_
                      : HarfBuzzUnitsToFloat(hb_positions[i].x_advance);
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
      ItemizeTextToRuns(text, &layout_run_list_);

      // TODO(ckocagil): Remove ScopedTracker below once crbug.com/441028 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile(
          FROM_HERE_WITH_EXPLICIT_FUNCTION("441028 ShapeRunList() 2"));
      ShapeRunList(text, &layout_run_list_);
    }

    display_run_list_.reset();
    update_display_text_ = true;
    update_layout_run_list_ = false;
  }
  if (update_display_text_) {
    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
    UpdateDisplayText(multiline() ? 0 : layout_run_list_.width());
    update_display_text_ = false;
    update_display_run_list_ = text_elided();
  }
}

internal::TextRunList* RenderTextHarfBuzz::GetRunList() {
  DCHECK(!update_layout_run_list_);
  DCHECK(!update_display_run_list_);
  return text_elided() ? display_run_list_.get() : &layout_run_list_;
}

const internal::TextRunList* RenderTextHarfBuzz::GetRunList() const {
  return const_cast<RenderTextHarfBuzz*>(this)->GetRunList();
}

bool RenderTextHarfBuzz::GetDecoratedTextForRange(
    const Range& range,
    DecoratedText* decorated_text) {
  if (obscured())
    return false;

  EnsureLayout();

  decorated_text->attributes.clear();
  decorated_text->text = GetTextFromRange(range);

  const internal::TextRunList* run_list = GetRunList();
  for (size_t i = 0; i < run_list->size(); i++) {
    const internal::TextRunHarfBuzz& run = *run_list->runs()[i];

    const Range intersection = range.Intersect(run.range);
    DCHECK(!intersection.is_reversed());

    if (!intersection.is_empty()) {
      int style = Font::NORMAL;
      if (run.italic)
        style |= Font::ITALIC;
      if (run.underline)
        style |= Font::UNDERLINE;

      // Get range relative to the decorated text.
      DecoratedText::RangedAttribute attribute(
          Range(intersection.start() - range.GetMin(),
                intersection.end() - range.GetMin()),
          run.font.Derive(0, style, run.weight));

      attribute.strike = run.strike;
      attribute.diagonal_strike = run.diagonal_strike;
      decorated_text->attributes.push_back(attribute);
    }
  }
  return true;
}

}  // namespace gfx
