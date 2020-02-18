// Copyright 2019 Google LLC.
#include "modules/skparagraph/src/ParagraphImpl.h"
#include <unicode/brkiter.h>
#include <unicode/ubidi.h>
#include <unicode/unistr.h>
#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPictureRecorder.h"
#include "modules/skparagraph/src/Iterators.h"
#include "modules/skparagraph/src/Run.h"
#include "modules/skparagraph/src/TextWrapper.h"
#include "src/core/SkSpan.h"
#include "src/utils/SkUTF.h"

namespace {

SkUnichar utf8_next(const char** ptr, const char* end) {
    SkUnichar val = SkUTF::NextUTF8(ptr, end);
    return val < 0 ? 0xFFFD : val;
}

class TextBreaker {
public:
    TextBreaker() : fPos(-1) {}

    bool initialize(SkSpan<const char> text, UBreakIteratorType type) {
        UErrorCode status = U_ZERO_ERROR;

        fSize = text.size();
        UText utf8UText = UTEXT_INITIALIZER;
        utext_openUTF8(&utf8UText, text.begin(), text.size(), &status);
        fAutoClose =
                std::unique_ptr<UText, SkFunctionWrapper<UText*, UText, utext_close>>(&utf8UText);
        if (U_FAILURE(status)) {
            SkDebugf("Could not create utf8UText: %s", u_errorName(status));
            return false;
        }
        fIterator = ubrk_open(type, "en", nullptr, 0, &status);
        if (U_FAILURE(status)) {
            SkDebugf("Could not create line break iterator: %s", u_errorName(status));
            SK_ABORT("");
        }

        ubrk_setUText(fIterator, &utf8UText, &status);
        if (U_FAILURE(status)) {
            SkDebugf("Could not setText on break iterator: %s", u_errorName(status));
            return false;
        }

        fPos = 0;
        return true;
    }

    size_t first() {
        fPos = ubrk_first(fIterator);
        return eof() ? fSize : fPos;
    }

    size_t next() {
        fPos = ubrk_next(fIterator);
        return eof() ? fSize : fPos;
    }

    int32_t status() { return ubrk_getRuleStatus(fIterator); }

    bool eof() { return fPos == icu::BreakIterator::DONE; }

    ~TextBreaker() = default;

private:
    std::unique_ptr<UText, SkFunctionWrapper<UText*, UText, utext_close>> fAutoClose;
    UBreakIterator* fIterator;
    int32_t fPos;
    size_t fSize;
};
}  // namespace

namespace skia {
namespace textlayout {

TextRange operator*(const TextRange& a, const TextRange& b) {
    auto begin = SkTMax(a.start, b.start);
    auto end = SkTMin(a.end, b.end);
    return end > begin ? TextRange(begin, end) : EMPTY_TEXT;
}

ParagraphCache ParagraphImpl::fParagraphCache;

ParagraphImpl::ParagraphImpl(const SkString& text,
                             ParagraphStyle style,
                             SkTArray<Block, true> blocks,
                             sk_sp<FontCollection> fonts)
        : Paragraph(std::move(style), std::move(fonts))
        , fTextStyles(std::move(blocks))
        , fText(text)
        , fTextSpan(fText.c_str(), fText.size())
        , fState(kUnknown)
        , fPicture(nullptr)
        , fOldWidth(0)
        , fOldHeight(0)
        , fParagraphCacheOn(false) {
    // TODO: extractStyles();
}

ParagraphImpl::ParagraphImpl(const std::u16string& utf16text,
                             ParagraphStyle style,
                             SkTArray<Block, true> blocks,
                             sk_sp<FontCollection> fonts)
        : Paragraph(std::move(style), std::move(fonts))
        , fTextStyles(std::move(blocks))
        , fState(kUnknown)
        , fPicture(nullptr)
        , fOldWidth(0)
        , fOldHeight(0)
        , fParagraphCacheOn(false) {
    icu::UnicodeString unicode((UChar*)utf16text.data(), SkToS32(utf16text.size()));
    std::string str;
    unicode.toUTF8String(str);
    fText = SkString(str.data(), str.size());
    fTextSpan = SkSpan<const char>(fText.c_str(), fText.size());
    // TODO: extractStyles();
}

ParagraphImpl::~ParagraphImpl() = default;

void ParagraphImpl::layout(SkScalar width) {

    if (fState < kShaped) {
        // Layout marked as dirty for performance/testing reasons
        this->fRuns.reset();
        this->fClusters.reset();
    } else if (fState >= kLineBroken && (fOldWidth != width || fOldHeight != fHeight)) {
        // We can use the results from SkShaper but have to break lines again
        fState = kShaped;
    }

    if (fState < kShaped) {
        fClusters.reset();

        if (!this->shapeTextIntoEndlessLine()) {
            // Apply the last style to the empty text
            FontIterator font(SkMakeSpan(" "), &fFontResolver);
            // Get the font metrics
            font.consume();
            LineMetrics lineMetrics(font.currentFont());
            // Set the important values that are not zero
            fHeight = lineMetrics.height();
            fAlphabeticBaseline = lineMetrics.alphabeticBaseline();
            fIdeographicBaseline = lineMetrics.ideographicBaseline();
        }
        if (fState < kShaped) {
            fState = kShaped;
        } else {
            layout(width);
            return;
        }

        if (fState < kMarked) {
            this->buildClusterTable();
            fState = kClusterized;
            this->markLineBreaks();
            fState = kMarked;
            // Add the paragraph to the cache
            if (fParagraphCacheOn) {
                fParagraphCache.updateParagraph(this);
            }
        }
    }

    if (fState >= kLineBroken)  {
        if (fOldWidth != width || fOldHeight != fHeight) {
            fState = kMarked;
        }
    }

    if (fState < kLineBroken) {
        this->resetContext();
        this->resolveStrut();
        this->fLines.reset();
        this->breakShapedTextIntoLines(width);
        fState = kLineBroken;

    }

    if (fState < kFormatted) {
        // Build the picture lazily not until we actually have to paint (or never)
        this->formatLines(fWidth);
        fState = kFormatted;
        // Add the paragraph to the cache
        if (fParagraphCacheOn) {
            fParagraphCache.updateParagraph(this);
        }
    }

    this->fOldWidth = width;
    this->fOldHeight = this->fHeight;
}

void ParagraphImpl::paint(SkCanvas* canvas, SkScalar x, SkScalar y) {

    if (fState < kDrawn) {
        // Record the picture anyway (but if we have some pieces in the cache they will be used)
        this->paintLinesIntoPicture();
        fState = kDrawn;
        if (fParagraphCacheOn) {
            fParagraphCache.updateParagraph(this);
        }
    }

    SkMatrix matrix = SkMatrix::MakeTrans(x, y);
    canvas->drawPicture(fPicture, &matrix, nullptr);
}

void ParagraphImpl::resetContext() {
    fAlphabeticBaseline = 0;
    fHeight = 0;
    fWidth = 0;
    fIdeographicBaseline = 0;
    fMaxIntrinsicWidth = 0;
    fMinIntrinsicWidth = 0;
}

// Clusters in the order of the input text
void ParagraphImpl::buildClusterTable() {

    // Walk through all the run in the direction of input text
    for (RunIndex runIndex = 0; runIndex < fRuns.size(); ++runIndex) {
        auto& run = fRuns[runIndex];
        auto runStart = fClusters.size();
        fClusters.reserve(fClusters.size() + fRuns.size());
        // Walk through the glyph in the direction of input text
        run.iterateThroughClustersInTextOrder([runIndex, this](
                                                      size_t glyphStart,
                                                      size_t glyphEnd,
                                                      size_t charStart,
                                                      size_t charEnd,
                                                      SkScalar width,
                                                      SkScalar height) {
            SkASSERT(charEnd >= charStart);
            SkSpan<const char> text(fTextSpan.begin() + charStart, charEnd - charStart);

            auto& cluster = fClusters.emplace_back(this, runIndex, glyphStart, glyphEnd, text, width, height);
            cluster.setIsWhiteSpaces();
        });

        run.setClusterRange(runStart, fClusters.size());
        fMaxIntrinsicWidth += run.advance().fX;
    }
}

// TODO: we need soft line breaks before for word spacing
void ParagraphImpl::markLineBreaks() {

    // Find all possible (soft) line breaks
    TextBreaker breaker;
    if (!breaker.initialize(fTextSpan, UBRK_LINE)) {
        return;
    }

    Cluster* current = fClusters.begin();
    while (!breaker.eof() && current < fClusters.end()) {
        size_t currentPos = breaker.next();
        while (current < fClusters.end()) {
            if (current->textRange().end > currentPos) {
                break;
            } else if (current->textRange().end == currentPos) {
                current->setBreakType(breaker.status() == UBRK_LINE_HARD
                                      ? Cluster::BreakType::HardLineBreak
                                      : Cluster::BreakType::SoftLineBreak);
                ++current;
                break;
            }
            ++current;
        }
    }

    // Walk through all the clusters in the direction of input text
    Block* currentStyle = this->fTextStyles.begin();
    SkScalar shift = 0;
    for (auto& cluster : fClusters) {
        auto run = cluster.run();

        // Shift the cluster
        run->shift(&cluster, shift);

        // Synchronize styles (one cluster can be covered by few styles)
        while (!cluster.startsIn(currentStyle->fRange)) {
            currentStyle++;
            SkASSERT(currentStyle != this->fTextStyles.end());
        }

        // Take spacing styles in account
        if (currentStyle->fStyle.getWordSpacing() != 0 &&
            fParagraphStyle.getTextAlign() != TextAlign::kJustify) {
            if (cluster.isWhitespaces() && cluster.isSoftBreak()) {
                shift += run->addSpacesAtTheEnd(currentStyle->fStyle.getWordSpacing(), &cluster);
            }
        }
        if (currentStyle->fStyle.getLetterSpacing() != 0) {
            shift += run->addSpacesEvenly(currentStyle->fStyle.getLetterSpacing(), &cluster);
        }
    }

    fClusters.emplace_back(this, EMPTY_RUN, 0, 0, SkSpan<const char>(), 0, 0);
}

bool ParagraphImpl::shapeTextIntoEndlessLine() {

    class ShapeHandler final : public SkShaper::RunHandler {
    public:
        explicit ShapeHandler(ParagraphImpl& paragraph, FontIterator* fontIterator)
                : fParagraph(&paragraph)
                , fFontIterator(fontIterator)
                , fAdvance(SkVector::Make(0, 0)) {}

        SkVector advance() const { return fAdvance; }

    private:
        void beginLine() override {}

        void runInfo(const RunInfo&) override {}

        void commitRunInfo() override {}

        Buffer runBuffer(const RunInfo& info) override {
            auto& run = fParagraph->fRuns.emplace_back(fParagraph,
                                                       info,
                                                       fFontIterator->currentLineHeight(),
                                                       fParagraph->fRuns.count(),
                                                       fAdvance.fX);
            return run.newRunBuffer();
        }

        void commitRunBuffer(const RunInfo&) override {
            auto& run = fParagraph->fRuns.back();
            if (run.size() == 0) {
                fParagraph->fRuns.pop_back();
                return;
            }
            // Carve out the line text out of the entire run text
            fAdvance.fX += run.advance().fX;
            fAdvance.fY = SkMaxScalar(fAdvance.fY, run.descent() - run.ascent());
        }

        void commitLine() override {}

        ParagraphImpl* fParagraph;
        FontIterator* fFontIterator;
        SkVector fAdvance;
    };

    if (fTextSpan.empty()) {
        return false;
    }

    // This is a pretty big step - resolving all characters against all given fonts
    fFontResolver.findAllFontsForAllStyledBlocks(fTextSpan, styles(), fFontCollection);

    // Check the font-resolved text against the cache
    if (fParagraphCacheOn && fParagraphCache.findParagraph(this)) {
        return true;
    }

    LangIterator lang(fTextSpan, styles(), paragraphStyle().getTextStyle());
    FontIterator font(fTextSpan, &fFontResolver);
    ShapeHandler handler(*this, &font);
    std::unique_ptr<SkShaper> shaper = SkShaper::MakeShapeDontWrapOrReorder();
    SkASSERT_RELEASE(shaper != nullptr);
    auto bidi = SkShaper::MakeIcuBiDiRunIterator(
            fTextSpan.begin(), fTextSpan.size(),
            fParagraphStyle.getTextDirection() == TextDirection::kLtr ? (uint8_t)2 : (uint8_t)1);
    if (bidi == nullptr) {
        return false;
    }
    auto script = SkShaper::MakeHbIcuScriptRunIterator(fTextSpan.begin(), fTextSpan.size());

    shaper->shape(fTextSpan.begin(), fTextSpan.size(), font, *bidi, *script, lang,
                  std::numeric_limits<SkScalar>::max(), &handler);

    if (fParagraphStyle.getTextAlign() == TextAlign::kJustify) {
        fRunShifts.reset();
        fRunShifts.push_back_n(fRuns.size(), RunShifts());
        for (size_t i = 0; i < fRuns.size(); ++i) {
            fRunShifts[i].fShifts.push_back_n(fRuns[i].size() + 1, 0.0);
        }
    }

    return true;
}

void ParagraphImpl::breakShapedTextIntoLines(SkScalar maxWidth) {

    TextWrapper textWrapper;
    textWrapper.breakTextIntoLines(
            this,
            maxWidth,
            [&](TextRange text,
                TextRange textWithSpaces,
                ClusterRange clusters,
                size_t startPos,
                size_t endPos,
                SkVector offset,
                SkVector advance,
                LineMetrics metrics,
                bool addEllipsis) {
                // Add the line
                // TODO: Take in account clipped edges
                auto& line = this->addLine(offset, advance, text, textWithSpaces, clusters, metrics);
                if (addEllipsis) {
                    line.createEllipsis(maxWidth, fParagraphStyle.getEllipsis(), true);
                }
            });
    fHeight = textWrapper.height();
    fWidth = maxWidth;  // fTextWrapper.width();
    fMinIntrinsicWidth = textWrapper.minIntrinsicWidth();
    fMaxIntrinsicWidth = textWrapper.maxIntrinsicWidth();
    fAlphabeticBaseline = fLines.empty() ? 0 : fLines.front().alphabeticBaseline();
    fIdeographicBaseline = fLines.empty() ? 0 : fLines.front().ideographicBaseline();
}

void ParagraphImpl::formatLines(SkScalar maxWidth) {
    auto effectiveAlign = fParagraphStyle.effective_align();
    for (auto& line : fLines) {
        line.format(effectiveAlign, maxWidth, &line != &fLines.back());
    }
}

void ParagraphImpl::paintLinesIntoPicture() {
    SkPictureRecorder recorder;
    SkCanvas* textCanvas = recorder.beginRecording(fWidth, fHeight, nullptr, 0);

    for (auto& line : fLines) {
        line.paint(textCanvas);
    }

    fPicture = recorder.finishRecordingAsPicture();
}

void ParagraphImpl::resolveStrut() {
    auto strutStyle = this->paragraphStyle().getStrutStyle();
    if (!strutStyle.getStrutEnabled()) {
        return;
    }

    sk_sp<SkTypeface> typeface;
    for (auto& fontFamily : strutStyle.getFontFamilies()) {
        typeface = fFontCollection->matchTypeface(fontFamily.c_str(), strutStyle.getFontStyle());
        if (typeface.get() != nullptr) {
            break;
        }
    }
    if (typeface.get() == nullptr) {
        typeface = SkTypeface::MakeDefault();
    }

    SkFont font(typeface, strutStyle.getFontSize());
    SkFontMetrics metrics;
    font.getMetrics(&metrics);

    fStrutMetrics = LineMetrics(metrics.fAscent * strutStyle.getHeight(),
                                metrics.fDescent * strutStyle.getHeight(),
                                strutStyle.getLeading() < 0
                                        ? metrics.fLeading
                                        : strutStyle.getLeading() * strutStyle.getFontSize());
}

BlockRange ParagraphImpl::findAllBlocks(TextRange textRange) {
    BlockIndex begin = EMPTY_BLOCK;
    BlockIndex end = EMPTY_BLOCK;
    for (size_t index = 0; index < fTextStyles.size(); ++index) {
        auto& block = fTextStyles[index];
        if (block.fRange.end <= textRange.start) {
            continue;
        }
        if (block.fRange.start >= textRange.end) {
            break;
        }
        if (begin == EMPTY_BLOCK) {
            begin = index;
        }
        end = index;
    }

    return { begin, end + 1 };
}

TextLine& ParagraphImpl::addLine(SkVector offset,
                                 SkVector advance,
                                 TextRange text,
                                 TextRange textWithSpaces,
                                 ClusterRange clusters,
                                 LineMetrics sizes) {
    // Define a list of styles that covers the line
    auto blocks = findAllBlocks(text);

    return fLines.emplace_back(this, offset, advance, blocks, text, textWithSpaces, clusters, sizes);
}

// Returns a vector of bounding boxes that enclose all text between
// start and end glyph indexes, including start and excluding end
std::vector<TextBox> ParagraphImpl::getRectsForRange(unsigned start,
                                                     unsigned end,
                                                     RectHeightStyle rectHeightStyle,
                                                     RectWidthStyle rectWidthStyle) {
    std::vector<TextBox> results;
    if (start >= end || start > fTextSpan.size() || end == 0) {
        return results;
    }

    // Calculate the utf8 substring
    const char* first = fTextSpan.begin();
    for (unsigned i = 0; i < start; ++i) {
        utf8_next(&first, fTextSpan.end());
    }
    const char* last = first;
    for (unsigned i = start; i < end; ++i) {
        utf8_next(&last, fTextSpan.end());
    }
    TextRange text(first - fTextSpan.begin(), last  - fTextSpan.begin());

    for (auto& line : fLines) {
        auto lineText = line.textWithSpaces();
        auto intersect = lineText * text;
        if (intersect.empty() && (!lineText.empty() || lineText.start != text.start)) {
            continue;
        }

        SkScalar runOffset = 0;
        if (lineText.start != intersect.start) {
            TextRange before(lineText.start, intersect.start);
            runOffset = line.iterateThroughRuns(
                    before, 0, true,
                    [](Run*, size_t, size_t, SkRect, SkScalar, bool) { return true; });
        }
        auto firstBox = results.size();
        line.iterateThroughRuns(intersect,
                                runOffset,
                                true,
                                [&results, &line](Run* run, size_t pos, size_t size, SkRect clip,
                                                  SkScalar shift, bool clippingNeeded) {
                                    clip.offset(line.offset());
                                    results.emplace_back(clip, run->leftToRight()
                                                                       ? TextDirection::kLtr
                                                                       : TextDirection::kRtl);
                                    return true;
                                });

        if (rectHeightStyle != RectHeightStyle::kTight) {
            // Align all the rectangles
            for (auto i = firstBox; i < results.size(); ++i) {
                auto& rect = results[i].rect;
                if (rectHeightStyle == RectHeightStyle::kMax) {
                    rect.fTop = line.offset().fY + line.roundingDelta();
                    rect.fBottom = line.offset().fY + line.height();

                } else if (rectHeightStyle == RectHeightStyle::kIncludeLineSpacingTop) {
                    rect.fTop = line.offset().fY;

                } else if (rectHeightStyle == RectHeightStyle::kIncludeLineSpacingMiddle) {
                    rect.fTop -= (rect.fTop - line.offset().fY) / 2;
                    rect.fBottom += (line.offset().fY + line.height() - rect.fBottom) / 2;

                } else if (rectHeightStyle == RectHeightStyle::kIncludeLineSpacingBottom) {
                    rect.fBottom = line.offset().fY + line.height();
                }
            }
        } else {
            // Just leave the boxes the way they are
        }

        if (rectWidthStyle == RectWidthStyle::kMax) {
            for (auto& i = firstBox; i < results.size(); ++i) {
                auto clip = results[i].rect;
                auto dir = results[i].direction;
                if (clip.fLeft > line.offset().fX) {
                    SkRect left = SkRect::MakeXYWH(0, clip.fTop, clip.fLeft - line.offset().fX,
                                                   clip.fBottom);
                    results.insert(results.begin() + i, {left, dir});
                    ++i;
                }
                if (clip.fRight < line.offset().fX + line.width()) {
                    SkRect right = SkRect::MakeXYWH(clip.fRight - line.offset().fX,
                                                    clip.fTop,
                                                    line.width() - (clip.fRight - line.offset().fX),
                                                    clip.fBottom);
                    results.insert(results.begin() + i, {right, dir});
                    ++i;
                }
            }
        }
    }

    return results;
}
// TODO: Deal with RTL here
PositionWithAffinity ParagraphImpl::getGlyphPositionAtCoordinate(SkScalar dx, SkScalar dy) {
    PositionWithAffinity result(0, Affinity::kDownstream);
    for (auto& line : fLines) {
        // This is so far the the line vertically closest to our coordinates
        // (or the first one, or the only one - all the same)
        line.iterateThroughRuns(
                line.textWithSpaces(),
                0,
                true,
                [dx, &result](Run* run, size_t pos, size_t size, SkRect clip, SkScalar shift,
                              bool clippingNeeded) {
                    if (dx < clip.fLeft) {
                        // All the other runs are placed right of this one
                        result = {SkToS32(run->fClusterIndexes[pos]), kDownstream};
                        return false;
                    }

                    if (dx >= clip.fRight) {
                        // We have to keep looking but just in case keep the last one as the closes
                        // so far
                        result = {SkToS32(run->fClusterIndexes[pos + size]), kUpstream};
                        return true;
                    }

                    // So we found the run that contains our coordinates
                    size_t found = pos;
                    for (size_t i = pos; i < pos + size; ++i) {
                        if (run->positionX(i) + shift > dx) {
                            break;
                        }
                        found = i;
                    }

                    if (found == pos) {
                        result = {SkToS32(run->fClusterIndexes[found]), kDownstream};
                    } else if (found == pos + size - 1) {
                        result = {SkToS32(run->fClusterIndexes[found]), kUpstream};
                    } else {
                        auto center = (run->positionX(found + 1) + run->positionX(found)) / 2;
                        if ((dx <= center + shift) == run->leftToRight()) {
                            result = {SkToS32(run->fClusterIndexes[found]), kDownstream};
                        } else {
                            result = {SkToS32(run->fClusterIndexes[found + 1]), kUpstream};
                        }
                    }
                    // No need to continue
                    return false;
                });

        // Let's figure out if we can stop looking
        auto offsetY = line.offset().fY;
        if (dy < offsetY) {
            // The closest position on this line; next line is going to be even lower
            break;
        }
        if (dy >= offsetY + line.height()) {
            // We have the closest position on the lowest line so far, but we have to continue
            continue;
        }

        // We hit the line; nothing else to do
        break;
    }

    // SkDebugf("getGlyphPositionAtCoordinate(%f,%f) = %d\n", dx, dy, result.position);
    return result;
}

// Finds the first and last glyphs that define a word containing
// the glyph at index offset.
// By "glyph" they mean a character index - indicated by Minikin's code
SkRange<size_t> ParagraphImpl::getWordBoundary(unsigned offset) {
    TextBreaker breaker;
    if (!breaker.initialize(fTextSpan, UBRK_WORD)) {
        return {0, 0};
    }

    size_t currentPos = breaker.first();
    while (true) {
        auto start = currentPos;
        currentPos = breaker.next();
        if (breaker.eof()) {
            break;
        }
        if (start <= offset && currentPos > offset) {
            return {start, currentPos};
        }
    }
    return {0, 0};
}

SkSpan<const char> ParagraphImpl::text(TextRange textRange) {
    SkASSERT(textRange.start < fText.size() && textRange.end <= fText.size());
    return SkSpan<const char>(&fText[textRange.start], textRange.width());
}

SkSpan<Cluster> ParagraphImpl::clusters(ClusterRange clusterRange) {
    SkASSERT(clusterRange.start < fClusters.size() && clusterRange.end <= fClusters.size());
    return SkSpan<Cluster>(&fClusters[clusterRange.start], clusterRange.width());
}

Cluster& ParagraphImpl::cluster(ClusterIndex clusterIndex) {
    SkASSERT(clusterIndex < fClusters.size());
    return fClusters[clusterIndex];
}

Run& ParagraphImpl::run(RunIndex runIndex) {
    SkASSERT(runIndex < fRuns.size());
    return fRuns[runIndex];
}

SkSpan<Block> ParagraphImpl::blocks(BlockRange blockRange) {
    SkASSERT(blockRange.start < fTextStyles.size() && blockRange.end <= fTextStyles.size());
    return SkSpan<Block>(&fTextStyles[blockRange.start], blockRange.width());
}

Block& ParagraphImpl::block(BlockIndex blockIndex) {
    SkASSERT(blockIndex < fTextStyles.size());
    return fTextStyles[blockIndex];
}

void ParagraphImpl::resetRunShifts() {
    fRunShifts.reset();
    fRunShifts.push_back_n(fRuns.size(), RunShifts());
    for (size_t i = 0; i < fRuns.size(); ++i) {
        fRunShifts[i].fShifts.push_back_n(fRuns[i].size() + 1, 0.0);
    }
}

void ParagraphImpl::setState(InternalState state) {
    if (fState <= state) {
        fState = state;
        return;
    }

    fState = state;
    switch (fState) {
        case kUnknown:
            fRuns.reset();
        case kShaped:
            fClusters.reset();
        case kClusterized:
        case kMarked:
        case kLineBroken:
            this->resetContext();
            this->resolveStrut();
            this->resetRunShifts();
            fLines.reset();
        case kFormatted:
            fPicture = nullptr;
        case kDrawn:
            break;
    default:
        break;
    }

}

}  // namespace textlayout
}  // namespace skia
