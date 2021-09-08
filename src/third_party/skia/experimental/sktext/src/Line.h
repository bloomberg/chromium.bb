// Copyright 2021 Google LLC.
#ifndef TextLine_DEFINED
#define TextLine_DEFINED

#include "experimental/sktext/include/Types.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"

namespace skia {
namespace text {

class Processor;

class TextMetrics {

public:
  TextMetrics() {
      clean();
  }

  TextMetrics(const SkFont& font) {

      SkFontMetrics metrics;
      font.getMetrics(&metrics);
      fAscent = metrics.fAscent;
      fDescent = metrics.fDescent;
      fLeading = metrics.fLeading;
  }

  TextMetrics(const TextMetrics&) = default;

  void merge(TextMetrics tail) {
      this->fAscent = std::min(this->fAscent, tail.fAscent);
      this->fDescent = std::max(this->fDescent, tail.fDescent);
      this->fLeading = std::max(this->fLeading, tail.fLeading);
  }

  void clean() {
      this->fAscent = 0;
      this->fDescent = 0;
      this->fLeading = 0;
  }

  SkScalar height() const {
      return this->fDescent - this->fAscent + this->fLeading;
  }

SkScalar baseline() const {
      return - this->fAscent + this->fLeading / 2;
  }

private:
    SkScalar fAscent;
    SkScalar fDescent;
    SkScalar fLeading;
};

class GlyphPos {
public:

    GlyphPos() : fRunIndex(EMPTY_INDEX), fGlyphIndex(EMPTY_INDEX) { }
    GlyphPos(size_t runIndex, size_t glyphIndex) : fRunIndex(runIndex), fGlyphIndex(glyphIndex) { }

    bool operator==(const GlyphPos& other) const {
        return this->fRunIndex == other.fRunIndex && this->fGlyphIndex == other.fGlyphIndex;
    }

    size_t runIndex() const { return fRunIndex; }
    size_t glyphIndex() const { return fGlyphIndex; }
    void setGlyphIndex(size_t index) { fGlyphIndex = index; }

    bool isEmpty() const { return fRunIndex == EMPTY_INDEX; }

private:
    size_t fRunIndex;
    size_t fGlyphIndex;
};

class Stretch {
public:

    Stretch() : fGlyphStart(), fGlyphEnd(), fWidth(0), fTextRange(0, 0), fTextMetrics() { }

    Stretch(GlyphPos glyphStart, size_t textIndex, const TextMetrics& metrics)
        : fGlyphStart(glyphStart)
        , fGlyphEnd(glyphStart)
        , fWidth(0.0)
        , fTextRange(textIndex, textIndex)
        , fTextMetrics(metrics) { }

    Stretch(const Stretch&) = default;
    Stretch(Stretch&&) = default;
    Stretch& operator=(Stretch&&) = default;
    Stretch& operator=(const Stretch&) = default;

    bool isEmpty() const {
        if (this->fGlyphStart.isEmpty()) {
            SkASSERT(this->fGlyphEnd.isEmpty());
            return true;
        } else {
            SkASSERT(!this->fGlyphEnd.isEmpty());
            return false;
            //return (this->fGlyphStart.runIndex() == this->fGlyphEnd.runIndex() &&
            //        this->fGlyphStart.glyphIndex() == this->fGlyphEnd.glyphIndex());
        }
    }

    void clean() {
        fGlyphStart = fGlyphEnd;
        fTextRange.fStart = fTextRange.fEnd;
        fWidth = 0.0f;
        fTextMetrics.clean();
    }

    void moveTo(Stretch& tail) {

        if (tail.isEmpty()) {
            return;
        }

        if (this->isEmpty()) {
            if (!tail.isEmpty()) {
                this->fGlyphStart = tail.fGlyphStart;
                this->fGlyphEnd = tail.fGlyphEnd;
                this->fWidth = tail.fWidth;
                this->fTextRange = tail.fTextRange;
                this->fTextMetrics = tail.fTextMetrics;
            }
            tail.clean();
            return;
        }

        SkASSERT(this->fGlyphEnd.runIndex() != tail.fGlyphStart.runIndex() ||
                       this->fGlyphEnd.glyphIndex() == tail.fGlyphStart.glyphIndex());
        this->fGlyphEnd = tail.fGlyphEnd;
        this->fWidth += tail.fWidth;
        this->fTextRange.merge(tail.fTextRange);
        this->fTextMetrics.merge(tail.fTextMetrics);
        tail.clean();
    }

    void finish(size_t glyphIndex, size_t textIndex, SkScalar width) {
        this->fTextRange.fEnd = textIndex;
        this->fGlyphEnd.setGlyphIndex(glyphIndex);
        this->fWidth = width;
    }

    SkScalar width() const { return fWidth; }
    TextRange textRange() const { return fTextRange; }
    void setTextRange(TextRange range) { fTextRange = range; }

    const TextMetrics& textMetrics() const { return fTextMetrics; }
    GlyphPos glyphStart() const { return fGlyphStart; }
    GlyphPos glyphEnd() const { return fGlyphEnd; }
    size_t glyphStartIndex() const { return fGlyphStart.glyphIndex(); }
    size_t textStart() const { return fTextRange.fStart; }

private:
    GlyphPos fGlyphStart;
    GlyphPos fGlyphEnd;
    SkScalar fWidth;
    TextRange fTextRange;
    TextMetrics fTextMetrics;
};

class Line {
public:
    Line(Processor* processor, const Stretch& stretch, const Stretch& spaces);
    ~Line() = default;

private:
    friend class Processor;

    GlyphPos fTextStart;
    GlyphPos fTextEnd;
    GlyphPos fWhitespacesEnd;
    TextRange fText;
    TextRange fWhitespaces;
    SkScalar fTextWidth;
    SkScalar fSpacesWidth;
    TextMetrics fTextMetrics;
    SkSTArray<1, size_t, true> fRunsInVisualOrder;
    Processor* fProcessor;
};

} // namespace text
} // namespace skia
#endif
