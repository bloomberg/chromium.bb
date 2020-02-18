// Copyright 2019 Google LLC.
#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPictureRecorder.h"
#include "modules/skparagraph/src/Iterators.h"
#include "modules/skparagraph/src/ParagraphImpl.h"
#include "modules/skparagraph/src/Run.h"
#include "modules/skparagraph/src/TextWrapper.h"
#include "src/core/SkSpan.h"
#include "src/utils/SkUTF.h"
#include <algorithm>

namespace skia {
namespace textlayout {

TextRange operator*(const TextRange& a, const TextRange& b) {
    if (a.start == b.start && a.end == b.end) return a;
    auto begin = SkTMax(a.start, b.start);
    auto end = SkTMin(a.end, b.end);
    return end > begin ? TextRange(begin, end) : EMPTY_TEXT;
}

bool TextBreaker::initialize(SkSpan<const char> text, UBreakIteratorType type) {
    UErrorCode status = U_ZERO_ERROR;
    fIterator = nullptr;
    fSize = text.size();
    UText sUtf8UText = UTEXT_INITIALIZER;
    std::unique_ptr<UText, SkFunctionWrapper<decltype(utext_close), utext_close>> utf8UText(
        utext_openUTF8(&sUtf8UText, text.begin(), text.size(), &status));
    if (U_FAILURE(status)) {
        SkDebugf("Could not create utf8UText: %s", u_errorName(status));
        return false;
    }
    fIterator.reset(ubrk_open(type, "en", nullptr, 0, &status));
    if (U_FAILURE(status)) {
        SkDebugf("Could not create line break iterator: %s", u_errorName(status));
        SK_ABORT("");
    }

    ubrk_setUText(fIterator.get(), utf8UText.get(), &status);
    if (U_FAILURE(status)) {
        SkDebugf("Could not setText on break iterator: %s", u_errorName(status));
        return false;
    }

    fInitialized = true;
    fPos = 0;
    return true;
}

ParagraphImpl::ParagraphImpl(const SkString& text,
                             ParagraphStyle style,
                             SkTArray<Block, true> blocks,
                             SkTArray<Placeholder, true> placeholders,
                             sk_sp<FontCollection> fonts)
        : Paragraph(std::move(style), std::move(fonts))
        , fTextStyles(std::move(blocks))
        , fPlaceholders(std::move(placeholders))
        , fText(text)
        , fState(kUnknown)
        , fPicture(nullptr)
        , fStrutMetrics(false)
        , fOldWidth(0)
        , fOldHeight(0) {
    // TODO: extractStyles();
}

ParagraphImpl::ParagraphImpl(const std::u16string& utf16text,
                             ParagraphStyle style,
                             SkTArray<Block, true> blocks,
                             SkTArray<Placeholder, true> placeholders,
                             sk_sp<FontCollection> fonts)
        : Paragraph(std::move(style), std::move(fonts))
        , fTextStyles(std::move(blocks))
        , fPlaceholders(std::move(placeholders))
        , fState(kUnknown)
        , fPicture(nullptr)
        , fStrutMetrics(false)
        , fOldWidth(0)
        , fOldHeight(0) {
    icu::UnicodeString unicode((UChar*)utf16text.data(), SkToS32(utf16text.size()));
    std::string str;
    unicode.toUTF8String(str);
    fText = SkString(str.data(), str.size());
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
            SkFont font;
            SkScalar height;
            fFontResolver.getFirstFont(&font, &height);
            LineMetrics lineMetrics(font, paragraphStyle().getStrutStyle().getForceStrutHeight());
            // Set the important values that are not zero
            fWidth = 0;
            fHeight = lineMetrics.height() * (height == 0 || height == 1 ? 1 : height);
            fAlphabeticBaseline = lineMetrics.alphabeticBaseline();
            fIdeographicBaseline = lineMetrics.ideographicBaseline();
            this->fOldWidth = width;
            this->fOldHeight = this->fHeight;
            return;
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
            fFontCollection->getParagraphCache()->updateParagraph(this);
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
    }

    this->fOldWidth = width;
    this->fOldHeight = this->fHeight;
}

void ParagraphImpl::paint(SkCanvas* canvas, SkScalar x, SkScalar y) {

    if (fState < kDrawn) {
        // Record the picture anyway (but if we have some pieces in the cache they will be used)
        this->paintLinesIntoPicture();
        fState = kDrawn;
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
    fMaxWidthWithTrailingSpaces = 0;
}

// Clusters in the order of the input text
void ParagraphImpl::buildClusterTable() {

    // Walk through all the run in the direction of input text
    for (RunIndex runIndex = 0; runIndex < fRuns.size(); ++runIndex) {
        auto& run = fRuns[runIndex];
        auto runStart = fClusters.size();
        if (run.isPlaceholder()) {
            // There are no glyphs but we want to have one cluster
            SkSpan<const char> text = this->text(run.textRange());
            if (!fClusters.empty()) {
                fClusters.back().setBreakType(Cluster::SoftLineBreak);
            }
            auto& cluster = fClusters.emplace_back(this, runIndex, 0ul, 0ul, text,
                                                   run.advance().fX, run.advance().fY);
            cluster.setBreakType(Cluster::SoftLineBreak);
        } else {
            fClusters.reserve(fClusters.size() + fRuns.size());
            // Walk through the glyph in the direction of input text
            run.iterateThroughClustersInTextOrder([runIndex, this](size_t glyphStart,
                                                                   size_t glyphEnd,
                                                                   size_t charStart,
                                                                   size_t charEnd,
                                                                   SkScalar width,
                                                                   SkScalar height) {
                SkASSERT(charEnd >= charStart);
                SkSpan<const char> text(fText.c_str() + charStart, charEnd - charStart);

                auto& cluster = fClusters.emplace_back(this, runIndex, glyphStart, glyphEnd, text,
                                                       width, height);
                cluster.setIsWhiteSpaces();
            });
        }

        run.setClusterRange(runStart, fClusters.size());
        fMaxIntrinsicWidth += run.advance().fX;
    }
}

void ParagraphImpl::markLineBreaks() {

    // Find all possible (soft) line breaks
    // This iterator is used only once for a paragraph so we don't have to keep it
    TextBreaker breaker;
    if (!breaker.initialize(this->text(), UBRK_LINE)) {
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


    // Walk through all the clusters in the direction of shaped text
    // (we have to walk through the styles in the same order, too)
    SkScalar shift = 0;
    for (auto& run : fRuns) {

        // Skip placeholder runs
        if (run.isPlaceholder()) {
            continue;
        }

        bool soFarWhitespacesOnly = true;
        for (size_t index = 0; index != run.clusterRange().width(); ++index) {
            auto correctIndex = run.leftToRight()
                    ? index + run.clusterRange().start
                    : run.clusterRange().end - index - 1;
            const auto cluster = &this->cluster(correctIndex);

            // Shift the cluster (shift collected from the previous clusters)
            run.shift(cluster, shift);

            // Synchronize styles (one cluster can be covered by few styles)
            Block* currentStyle = this->fTextStyles.begin();
            while (!cluster->startsIn(currentStyle->fRange)) {
                currentStyle++;
                SkASSERT(currentStyle != this->fTextStyles.end());
            }

            SkASSERT(!currentStyle->fStyle.isPlaceholder());

            // Process word spacing
            if (currentStyle->fStyle.getWordSpacing() != 0) {
                if (cluster->isWhitespaces() && cluster->isSoftBreak()) {
                    if (!soFarWhitespacesOnly) {
                        shift += run.addSpacesAtTheEnd(currentStyle->fStyle.getWordSpacing(), cluster);
                    }
                }
            }
            // Process letter spacing
            if (currentStyle->fStyle.getLetterSpacing() != 0) {
                shift += run.addSpacesEvenly(currentStyle->fStyle.getLetterSpacing(), cluster);
            }

            if (soFarWhitespacesOnly && !cluster->isWhitespaces()) {
                soFarWhitespacesOnly = false;
            }
        }
    }

    fClusters.emplace_back(this, EMPTY_RUN, 0, 0, SkSpan<const char>(), 0, 0);
}

bool ParagraphImpl::shapeTextIntoEndlessLine() {

    class ShapeHandler final : public SkShaper::RunHandler {
    public:
        explicit ShapeHandler(ParagraphImpl& paragraph, size_t firstChar, FontIterator* fontIterator, SkScalar advanceX)
                : fParagraph(&paragraph)
                , fFirstChar(firstChar)
                , fFontIterator(fontIterator)
                , fAdvance(SkVector::Make(advanceX, 0)) {}

        SkVector advance() const { return fAdvance; }

    private:
        void beginLine() override {}

        void runInfo(const RunInfo&) override {}

        void commitRunInfo() override {}

        Buffer runBuffer(const RunInfo& info) override {
            auto& run = fParagraph->fRuns.emplace_back(fParagraph,
                                                       info,
                                                       fFirstChar,
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
            fAdvance.fY = SkMaxScalar(fAdvance.fY, run.advance().fY);
            run.fPlaceholder = nullptr;
        }

        void commitLine() override {}

        ParagraphImpl* fParagraph;
        size_t fFirstChar;
        FontIterator* fFontIterator;
        SkVector fAdvance;
    };

    // This is a pretty big step - resolving all characters against all given fonts
    fFontResolver.findAllFontsForAllStyledBlocks(this);

    if (fText.size() == 0 || fFontResolver.switches().size() == 0) {
        return false;
    }

    // Check the font-resolved text against the cache
    if (!fFontCollection->getParagraphCache()->findParagraph(this)) {
        // The text can be broken into many shaping sequences
        // (by place holders, possibly, by hard line breaks or tabs, too)
        uint8_t textDirection = fParagraphStyle.getTextDirection() == TextDirection::kLtr  ? 2 : 1;
        auto limitlessWidth = std::numeric_limits<SkScalar>::max();

        auto result = iterateThroughShapingRegions(
                [this, textDirection, limitlessWidth]
                (SkSpan<const char> textSpan, SkSpan<Block> styleSpan, SkScalar& advanceX, size_t start) {

            LangIterator lang(textSpan, styleSpan, paragraphStyle().getTextStyle());
            FontIterator font(textSpan, &fFontResolver);
            auto script = SkShaper::MakeHbIcuScriptRunIterator(textSpan.begin(), textSpan.size());
            auto bidi = SkShaper::MakeIcuBiDiRunIterator(textSpan.begin(), textSpan.size(), textDirection);
            if (bidi == nullptr) {
                return false;
            }

            // Set up the shaper and shape the next
            ShapeHandler handler(*this, start, &font, advanceX);
            auto shaper = SkShaper::MakeShapeDontWrapOrReorder();
            SkASSERT_RELEASE(shaper != nullptr);
            shaper->shape(textSpan.begin(), textSpan.size(), font, *bidi, *script, lang, limitlessWidth, &handler);
            advanceX =  handler.advance().fX;
            return true;
        });
        if (!result) {
            return false;
        }
    }

    // Clean the array for justification
    if (fParagraphStyle.getTextAlign() == TextAlign::kJustify) {
        fRunShifts.reset();
        fRunShifts.push_back_n(fRuns.size(), RunShifts());
        for (size_t i = 0; i < fRuns.size(); ++i) {
            fRunShifts[i].fShifts.push_back_n(fRuns[i].size() + 1, 0.0);
        }
    }

    return true;
}

bool ParagraphImpl::iterateThroughShapingRegions(ShapeVisitor shape) {

    SkScalar advanceX = 0;
    for (auto& placeholder : fPlaceholders) {
        // Shape the text
        if (placeholder.fTextBefore.width() > 0) {
            // Set up the iterators
            SkSpan<const char> textSpan = this->text(placeholder.fTextBefore);
            SkSpan<Block> styleSpan(fTextStyles.begin() + placeholder.fBlocksBefore.start,
                                    placeholder.fBlocksBefore.width());

            if (!shape(textSpan, styleSpan, advanceX, placeholder.fTextBefore.start)) {
                return false;
            }
        }

        if (placeholder.fRange.width() == 0) {
            continue;
        }
        // "Shape" the placeholder
        const SkShaper::RunHandler::RunInfo runInfo = {
            SkFont(),
            (uint8_t)2,
            SkPoint::Make(placeholder.fStyle.fWidth, placeholder.fStyle.fHeight),
            1,
            SkShaper::RunHandler::Range(placeholder.fRange.start, placeholder.fRange.width())
        };
        auto& run = fRuns.emplace_back(this,
                                       runInfo,
                                       0, //placeholder.fRange.start,
                                       1,
                                       fRuns.count(),
                                       advanceX);
        run.fPositions[0] = { advanceX, 0 };
        run.fClusterIndexes[0] = 0;
        run.fPlaceholder = &placeholder.fStyle;
        advanceX += placeholder.fStyle.fWidth;
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
                ClusterRange clustersWithGhosts,
                SkScalar widthWithSpaces,
                size_t startPos,
                size_t endPos,
                SkVector offset,
                SkVector advance,
                LineMetrics metrics,
                bool addEllipsis) {
                // Add the line
                // TODO: Take in account clipped edges
                auto& line = this->addLine(offset, advance, text, textWithSpaces, clusters, clustersWithGhosts, widthWithSpaces, metrics);
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
        if (&line == &fLines.back() && effectiveAlign == TextAlign::kJustify) {
            effectiveAlign = line.assumedTextAlign();
        }
        line.format(effectiveAlign, maxWidth);
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
    if (!strutStyle.getStrutEnabled() || strutStyle.getFontSize() < 0) {
        return;
    }

    sk_sp<SkTypeface> typeface;
    for (auto& fontFamily : strutStyle.getFontFamilies()) {
        typeface = fFontCollection->matchTypeface(fontFamily.c_str(), strutStyle.getFontStyle(), SkString());
        if (typeface.get() != nullptr) {
            break;
        }
    }
    if (typeface.get() == nullptr) {
        return;
    }

    SkFont font(typeface, strutStyle.getFontSize());
    SkFontMetrics metrics;
    font.getMetrics(&metrics);

    if (strutStyle.getHeightOverride()) {
        auto strutHeight = metrics.fDescent - metrics.fAscent + metrics.fLeading;
        auto strutMultiplier = strutStyle.getHeight() * strutStyle.getFontSize();
        fStrutMetrics = LineMetrics(
                metrics.fAscent / strutHeight * strutMultiplier,
                metrics.fDescent / strutHeight * strutMultiplier,
                strutStyle.getLeading() < 0 ? 0 : strutStyle.getLeading() * strutStyle.getFontSize());
    } else {
        fStrutMetrics = LineMetrics(
                metrics.fAscent,
                metrics.fDescent,
                strutStyle.getLeading() < 0 ? 0 : strutStyle.getLeading() * strutStyle.getFontSize());
    }
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
                                 ClusterRange clustersWithGhosts,
                                 SkScalar widthWithSpaces,
                                 LineMetrics sizes) {
    // Define a list of styles that covers the line
    auto blocks = findAllBlocks(text);

    return fLines.emplace_back(this, offset, advance, blocks, text, textWithSpaces, clusters, clustersWithGhosts, widthWithSpaces, sizes);
}

void ParagraphImpl::markGraphemes() {

    if (!fGraphemes.empty()) {
        return;
    }

    // This breaker gets called only once for a paragraph so we don't have to keep it
    TextBreaker breaker;
    if (!breaker.initialize(this->text(), UBRK_CHARACTER)) {
        return;
    }

    auto ptr = fText.c_str();
    auto end = fText.c_str() + fText.size();
    while (ptr < end) {

        size_t index = ptr - fText.c_str();
        SkUnichar u = SkUTF::NextUTF8(&ptr, end);
        uint16_t buffer[2];
        size_t count = SkUTF::ToUTF16(u, buffer);
        fCodePoints.emplace_back(EMPTY_INDEX, index);
        if (count > 1) {
            fCodePoints.emplace_back(EMPTY_INDEX, index);
        }
    }

    CodepointRange codepoints(0ul, 0ul);

    size_t endPos = 0;
    while (!breaker.eof()) {
        auto startPos = endPos;
        endPos = breaker.next();

        // Collect all the codepoints that belong to the grapheme
        while (codepoints.end < fCodePoints.size() && fCodePoints[codepoints.end].fTextIndex < endPos) {
            ++codepoints.end;
        }

        // Update all the codepoints that belong to this grapheme
        for (auto i = codepoints.start; i < codepoints.end; ++i) {
            fCodePoints[i].fGrapeme = fGraphemes.size();
        }

        fGraphemes.emplace_back(codepoints, TextRange(startPos, endPos));
        codepoints.start = codepoints.end;
    }
}

// Returns a vector of bounding boxes that enclose all text between
// start and end glyph indexes, including start and excluding end
std::vector<TextBox> ParagraphImpl::getRectsForRange(unsigned start,
                                                     unsigned end,
                                                     RectHeightStyle rectHeightStyle,
                                                     RectWidthStyle rectWidthStyle) {
    std::vector<TextBox> results;
    if (fText.isEmpty()) {
        results.emplace_back(SkRect::MakeXYWH(0, 0, 0, fHeight), fParagraphStyle.getTextDirection());
        return results;
    }

    markGraphemes();
    if (start >= end || start > fCodePoints.size() || end == 0) {
        return results;
    }

    // Make sure the edges are set on the glyph edges
    TextRange text;
    text.end = end >= fCodePoints.size()
                        ? fText.size()
                        : fGraphemes[fCodePoints[end].fGrapeme].fTextRange.start;
    text.start = start >=  fCodePoints.size()
                        ? fText.size()
                        : fGraphemes[fCodePoints[start].fGrapeme].fTextRange.start;

    for (auto& line : fLines) {
        auto lineText = line.textWithSpaces();
        auto intersect = lineText * text;
        if (intersect.empty() && lineText.start != text.start) {
            continue;
        }

        // Found a line that intersects with the text
        auto firstBoxOnTheLine = results.size();
        auto paragraphTextDirection = paragraphStyle().getTextDirection();
        auto lineTextAlign = line.assumedTextAlign();
        const Run* lastRun = nullptr;
        line.iterateThroughVisualRuns(true,
            [&](const Run* run, SkScalar runOffset, TextRange textRange, SkScalar* width) {

                auto intersect = textRange * text;
                if (intersect.empty() || textRange.empty()) {
                    auto context = line.measureTextInsideOneRun(textRange, run, runOffset, 0, true);
                    *width = context.clip.width();
                    return true;
                }

                TextRange head;
                if (run->leftToRight() && textRange.start != intersect.start) {
                    head = TextRange(textRange.start, intersect.start);
                    *width = line.measureTextInsideOneRun(head, run, runOffset, 0, true).clip.width();
                } else if (!run->leftToRight() && textRange.end != intersect.end) {
                    head = TextRange(intersect.end, textRange.end);
                    *width = line.measureTextInsideOneRun(head, run, runOffset, 0, true).clip.width();
                } else {
                    *width = 0;
                }
                runOffset += *width;

                // Found a run that intersects with the text
                auto context = line.measureTextInsideOneRun(intersect, run, runOffset, 0, true);
                *width += context.clip.width();

                SkRect clip = context.clip;
                SkRect trailingSpaces = SkRect::MakeEmpty();
                SkScalar ghostSpacesRight = context.run->leftToRight() ? clip.right() - line.width() : 0;
                SkScalar ghostSpacesLeft = !context.run->leftToRight() ? clip.right() - line.width() : 0;

                if (ghostSpacesRight + ghostSpacesLeft > 0) {
                    if (lineTextAlign == TextAlign::kLeft && ghostSpacesLeft > 0) {
                        clip.offset(-ghostSpacesLeft, 0);
                    } else if (lineTextAlign == TextAlign::kRight && ghostSpacesLeft > 0) {
                        clip.offset(-ghostSpacesLeft, 0);
                    } else if (lineTextAlign == TextAlign::kCenter) {
                        // TODO: What do we do for centering?
                    }
                }

                clip.offset(line.offset());

                if (rectHeightStyle == RectHeightStyle::kMax) {
                    // TODO: Sort it out with Flutter people
                    // Mimicking TxtLib:
                    //clip.fTop = line.offset().fY + line.roundingDelta();
                    clip.fBottom = line.offset().fY + line.height();
                    clip.fTop = line.offset().fY +
                            line.sizes().baseline() - line.getMaxRunMetrics().baseline() +
                            line.getMaxRunMetrics().delta();

                } else if (rectHeightStyle == RectHeightStyle::kIncludeLineSpacingTop) {
                    if (&line != &fLines.front()) {
                        clip.fTop -= line.sizes().runTop(context.run);
                    }
                    clip.fBottom -= line.sizes().runTop(context.run);
                } else if (rectHeightStyle == RectHeightStyle::kIncludeLineSpacingMiddle) {
                    if (&line != &fLines.front()) {
                        clip.fTop -= line.sizes().runTop(context.run) / 2;
                    }
                    if (&line == &fLines.back()) {
                        clip.fBottom -= line.sizes().runTop(context.run);
                    } else {
                        clip.fBottom -= line.sizes().runTop(context.run) / 2;
                    }
                } else if (rectHeightStyle == RectHeightStyle::kIncludeLineSpacingBottom) {
                    if (&line == &fLines.back()) {
                        clip.fBottom -= line.sizes().runTop(context.run);
                    }
                } else if (rectHeightStyle == RectHeightStyle::kStrut) {
                    auto strutStyle = this->paragraphStyle().getStrutStyle();
                    if (strutStyle.getStrutEnabled() && strutStyle.getFontSize() > 0) {
                        auto top = line.baseline() ; //+ line.sizes().runTop(run);
                        clip.fTop = top + fStrutMetrics.ascent();
                        clip.fBottom = top + fStrutMetrics.descent();
                        clip.offset(line.offset());
                    }
                }

                // Check if we can merge two boxes
                bool mergedBoxes = false;
                if (!results.empty() &&
                    lastRun != nullptr && lastRun->placeholder() == nullptr && context.run->placeholder() == nullptr &&
                    lastRun->lineHeight() == context.run->lineHeight() &&
                    lastRun->font() == context.run->font()) {
                    auto& lastBox = results.back();
                    if (SkScalarNearlyEqual(lastBox.rect.fTop, clip.fTop) &&
                        SkScalarNearlyEqual(lastBox.rect.fBottom, clip.fBottom) &&
                            (SkScalarNearlyEqual(lastBox.rect.fLeft, clip.fRight) ||
                             SkScalarNearlyEqual(lastBox.rect.fRight, clip.fLeft))) {
                        lastBox.rect.fLeft = SkTMin(lastBox.rect.fLeft, clip.fLeft);
                        lastBox.rect.fRight = SkTMax(lastBox.rect.fRight, clip.fRight);
                        mergedBoxes = true;
                    }
                }
                lastRun = context.run;

                if (!mergedBoxes) {
                    results.emplace_back(
                        clip, context.run->leftToRight() ? TextDirection::kLtr : TextDirection::kRtl);
                }

                if (trailingSpaces.width() > 0) {
                    results.emplace_back(trailingSpaces, paragraphTextDirection);
                }

                return true;
            });

        if (rectWidthStyle == RectWidthStyle::kMax) {
            // Align the very left/right box horizontally
            auto lineStart = line.offset().fX;
            auto lineEnd = line.offset().fX + line.width();
            auto left = results.front();
            auto right = results.back();
            if (left.rect.fLeft > lineStart && left.direction == TextDirection::kRtl) {
                left.rect.fRight = left.rect.fLeft;
                left.rect.fLeft = 0;
                results.insert(results.begin() + firstBoxOnTheLine + 1, left);
            }
            if (right.direction == TextDirection::kLtr &&
                right.rect.fRight >= lineEnd &&  right.rect.fRight < this->fMaxWidthWithTrailingSpaces) {
                right.rect.fLeft = right.rect.fRight;
                right.rect.fRight = this->fMaxWidthWithTrailingSpaces;
                results.emplace_back(right);
            }
        }
    }

    return results;
}

std::vector<TextBox> ParagraphImpl::GetRectsForPlaceholders() {
  std::vector<TextBox> boxes;
  if (fText.isEmpty()) {
      boxes.emplace_back(SkRect::MakeXYWH(0, 0, 0, fHeight), fParagraphStyle.getTextDirection());
      return boxes;
  }
  if (fPlaceholders.size() <= 1) {
      boxes.emplace_back(SkRect::MakeXYWH(0, 0, 0, fHeight), fParagraphStyle.getTextDirection());
      return boxes;
  }
  for (auto& line : fLines) {
      line.iterateThroughVisualRuns(true,
      [&boxes, &line]
      (const Run* run, SkScalar runOffset, TextRange textRange, SkScalar* width) {
          auto context = line.measureTextInsideOneRun(textRange, run, runOffset, 0, true);
          *width = context.clip.width();
          if (run->placeholder() == nullptr) {
              return true;
          }
          if (run->textRange().width() == 0) {
              return true;
          }
          SkRect clip = context.clip;
          clip.offset(line.offset());
          boxes.emplace_back(clip, run->leftToRight() ? TextDirection::kLtr : TextDirection::kRtl);
          return true;
      });
  }

  return boxes;
}
// TODO: Deal with RTL here
PositionWithAffinity ParagraphImpl::getGlyphPositionAtCoordinate(SkScalar dx, SkScalar dy) {

    PositionWithAffinity result(0, Affinity::kDownstream);
    if (fText.isEmpty()) {
        return result;
    }

    markGraphemes();
    for (auto& line : fLines) {
        // Let's figure out if we can stop looking
        auto offsetY = line.offset().fY;
        if (dy > offsetY + line.height() && &line != &fLines.back()) {
            // This line is not good enough
            continue;
        }

        // This is so far the the line vertically closest to our coordinates
        // (or the first one, or the only one - all the same)
        line.iterateThroughVisualRuns(true,
            [this, &line, dx, &result]
            (const Run* run, SkScalar runOffset, TextRange textRange, SkScalar* width) {

                  auto context = line.measureTextInsideOneRun(textRange, run, 0, 0, true);
                if (dx < context.clip.fLeft) {
                    // All the other runs are placed right of this one
                    result = { SkToS32(context.run->fClusterIndexes[context.pos]), kDownstream };
                    return false;
                }

                if (dx >= context.clip.fRight) {
                    // We have to keep looking but just in case keep the last one as the closes
                    // so far
                    result = { SkToS32(context.run->fClusterIndexes[context.pos + context.size - 1]) + 1, kUpstream };
                    return true;
                }

                // So we found the run that contains our coordinates
                // Find the glyph position in the run that is the closest left of our point
                // TODO: binary search
                size_t found = context.pos;
                for (size_t i = context.pos; i < context.pos + context.size; ++i) {
                    if (context.run->positionX(i) + context.fTextShift > dx) {
                        break;
                    }
                    found = i;
                }
                auto glyphStart = context.run->positionX(found);
                auto glyphWidth = context.run->positionX(found + 1) - context.run->positionX(found);
                auto clusterIndex8 = context.run->fClusterIndexes[found];

                // Find the grapheme positions in codepoints that contains the point
                auto codepoint = std::lower_bound(
                    fCodePoints.begin(), fCodePoints.end(),
                    clusterIndex8,
                    [](const Codepoint& lhs,size_t rhs) -> bool { return lhs.fTextIndex < rhs; });

                auto codepointIndex = codepoint - fCodePoints.begin();
                auto codepoints = fGraphemes[codepoint->fGrapeme].fCodepointRange;
                auto graphemeSize = codepoints.width();

                // We only need to inspect one glyph (maybe not even the entire glyph)
                SkScalar center;
                if (graphemeSize > 1) {
                    auto averageCodepoint = glyphWidth / graphemeSize;
                    auto codepointStart = glyphStart + averageCodepoint * (codepointIndex - codepoints.start);
                    auto codepointEnd = codepointStart + averageCodepoint;
                    center = (codepointStart + codepointEnd) / 2 + context.fTextShift;
                } else {
                    SkASSERT(graphemeSize == 1);
                    auto codepointStart = glyphStart;
                    auto codepointEnd = codepointStart + glyphWidth;
                    center = (codepointStart + codepointEnd) / 2 + context.fTextShift;
                }

                if ((dx <= center) == context.run->leftToRight()) {
                    result = { SkToS32(codepointIndex), kDownstream };
                } else {
                    result = { SkToS32(codepointIndex + 1), kUpstream };
                }
                // No need to continue
                return false;
            });

        if (dy < offsetY + line.height()) {
            // The closest position on this line; next line is going to be even lower
            break;
        }
    }

    // SkDebugf("getGlyphPositionAtCoordinate(%f,%f) = %d\n", dx, dy, result.position);
    return result;
}

// Finds the first and last glyphs that define a word containing
// the glyph at index offset.
// By "glyph" they mean a character index - indicated by Minikin's code
SkRange<size_t> ParagraphImpl::getWordBoundary(unsigned offset) {

    if (!fWordBreaker.initialized()) {
        if (!fWordBreaker.initialize(this->text(), UBRK_WORD)) {
            return {0, 0};
        }
    }

    auto start = fWordBreaker.preceding(offset + 1);
    auto end = fWordBreaker.following(start);

    return { start, end };
}

SkSpan<const char> ParagraphImpl::text(TextRange textRange) {
    SkASSERT(textRange.start <= fText.size() && textRange.end <= fText.size());
    auto start = fText.c_str() + textRange.start;
    return SkSpan<const char>(start, textRange.width());
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

Run& ParagraphImpl::runByCluster(ClusterIndex clusterIndex) {
    auto start = cluster(clusterIndex);
    return this->run(start.fRunIndex);
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
