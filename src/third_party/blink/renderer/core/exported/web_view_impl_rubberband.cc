/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "third_party/blink/renderer/core/exported/web_view_impl.h"

#include <vector>

#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#include "third_party/blink/public/web/web_view_client.h"

#include "base/logging.h"

namespace blink {

class RubberbandCandidate {
  public:
    WTF::String m_text;
    std::vector<LayoutUnit> m_charPositions;
    LayoutRect m_absRect;
    LayoutRect m_clipRect;
    LayoutUnit m_spaceWidth;
    int m_start;
    int m_len;
    bool m_isLTR;
    bool m_useLeadingTab;
    int m_groupId;
    WTF::String m_groupDelimiter;

    bool isAllWhitespaces() const
    {
        if (m_text.Impl()) {
            int end = m_start + m_len;
            const StringImpl& impl = *m_text.Impl();
            for (int i = m_start; i < end; ++i) {
                if (!IsSpaceOrNewline(impl[i])) {
                    return false;
                }
            }
        }
        return true;
    }
};

void appendCandidatesByLayoutNGText(
    const RubberbandContext& localContext,
    const LayoutText* layoutText,
    std::vector<RubberbandCandidate>& candidates);

struct RubberbandCandidate_XSorter {
    bool operator()(const RubberbandCandidate& lhs,
                    const RubberbandCandidate& rhs)
    {
        return lhs.m_clipRect.X() < rhs.m_clipRect.X();
    }
};

struct RubberbandCandidate_YSorter {
    bool operator()(const RubberbandCandidate& lhs,
                    const RubberbandCandidate& rhs)
    {
        return lhs.m_absRect.Y() < rhs.m_absRect.Y();
    }
};

class RubberbandStateImpl {
  public:
    std::vector<RubberbandCandidate> m_candidates;
    IntPoint m_startPoint;
};

RubberbandState::RubberbandState()
: impl_(std::make_unique<RubberbandStateImpl>())
{
}

RubberbandState::~RubberbandState()
{
}

class RubberbandLayerContext {
  public:
    LayoutRect m_clipRect;
    // frame scroll offset is not needed because LocalFrameView is not a ScrollableArea,
    // https://chromium.googlesource.com/chromium/src/+/88d8498ab4cf6c6be67e3d1dd1e4611c14fbace2
    FloatSize m_layerScrollOffset;
    double m_translateX;
    double m_translateY;
    double m_scaleX;
    double m_scaleY;
    LayoutBlock* m_colBlock;
    LayoutPoint m_colBlockAbsTopLeft;

    RubberbandLayerContext()
    : m_translateX(0.0)
    , m_translateY(0.0)
    , m_scaleX(1.0)
    , m_scaleY(1.0)
    , m_colBlock(0)
    {
    }

    RubberbandLayerContext(const RubberbandLayerContext* parent)
    : m_clipRect(parent->m_clipRect)
    , m_translateX(parent->m_translateX)
    , m_translateY(parent->m_translateY)
    , m_scaleX(parent->m_scaleX)
    , m_scaleY(parent->m_scaleY)
    , m_colBlock(parent->m_colBlock)
    , m_colBlockAbsTopLeft(parent->m_colBlockAbsTopLeft)
    {
        // m_layerScrollOffset is not inherited from the parent PaintLayer's
        // rubberband context because the coordinate space of the current
        // PaintLayer is already with respect to its parent PaintLayer.
        //
        // m_layerScrollOffset is only useful for LayoutObjects in the current
        // PaintLayer to compute their relative position due to scrolling.
    }
};

class RubberbandContext {
  public:
    const RubberbandContext* m_parent;
    RubberbandLayerContext* m_layerContext;
    const LayoutObject* m_layoutObject;
    const LayoutBlock* m_containingBlock;
    LayoutPoint m_layoutTopLeft;  // relative to the layer's top-left
    int m_groupId;
    WTF::String m_groupDelimiter;

    RubberbandContext()
    : m_parent(0)
    , m_layerContext(0)
    , m_layoutObject(0)
    , m_containingBlock(0)
    , m_groupId(-1)
    {
    }

    explicit RubberbandContext(const RubberbandContext* parent, const LayoutObject* layoutObject)
    : m_parent(parent)
    , m_layoutObject(layoutObject)
    , m_containingBlock(layoutObject ? layoutObject->ContainingBlock() : 0)
    , m_groupId(parent->m_groupId)
    , m_groupDelimiter(parent->m_groupDelimiter)
    {
        if (m_layoutObject && m_layoutObject->HasLayer()) {
            m_layerContext = new RubberbandLayerContext(parent->m_layerContext);
        }
        else {
            m_layerContext = parent->m_layerContext;
            m_layoutTopLeft = parent->m_layoutTopLeft;
        }
    }

    ~RubberbandContext()
    {
        if (m_layoutObject && m_layoutObject->HasLayer())
            delete m_layerContext;
    }

    LayoutPoint calcAbsPoint(const LayoutPoint& pt) const
    {
        LayoutUnit x = LayoutUnit(
                          m_layerContext->m_translateX
                        + ((pt.X() + m_layoutTopLeft.X()) * m_layerContext->m_scaleX));
        LayoutUnit y = LayoutUnit(
                          m_layerContext->m_translateY
                        + ((pt.Y() + m_layoutTopLeft.Y()) * m_layerContext->m_scaleY));
        LayoutPoint result(x, y);
        if (m_layerContext->m_colBlock) {
            LayoutPoint tmp = result;
            tmp.MoveBy(-m_layerContext->m_colBlockAbsTopLeft);
            LayoutSize offset = m_layerContext->m_colBlock->ColumnOffset(tmp);
            result.Move(offset);
        }
        return result;
    }

    LayoutRect calcAbsRect(const LayoutRect& rc) const
    {
        LayoutPoint topLeft = calcAbsPoint(rc.MinXMinYCorner());
        LayoutSize size = rc.Size();
        size.Scale(m_layerContext->m_scaleX, m_layerContext->m_scaleY);
        return LayoutRect(topLeft, size);
    }
};

static int s_groupId = 0;

static bool isClipped(EOverflow overflow)
{
    return overflow == EOverflow::kAuto
        || overflow == EOverflow::kHidden
        || overflow == EOverflow::kScroll;
}

static bool isSupportedTransform(const TransformationMatrix& matrix)
{
    // We support transforms that have scale and translate only.
    return 0.0 == matrix.M21() && 0.0 == matrix.M31()
        && 0.0 == matrix.M12() && 0.0 == matrix.M32()
        && 0.0 == matrix.M13() && 0.0 == matrix.M23() && 1.0 == matrix.M33() && 0.0 == matrix.M43()
        && 0.0 == matrix.M14() && 0.0 == matrix.M24() && 0.0 == matrix.M34() && 1.0 == matrix.M44()
        // Additionally, we don't support flips or zero-scales.
        && 0.0 < matrix.M11() && 0.0 < matrix.M22();
}

static bool isTextRubberbandable(const LayoutObject* layoutObject)
{
    return !layoutObject->Style()
        || ERubberbandable::kText == layoutObject->Style()->Rubberbandable()
        || ERubberbandable::kTextWithTab == layoutObject->Style()->Rubberbandable();
}

static bool isTextWithTab(const LayoutObject* layoutObject)
{
    return !layoutObject->Style()
        || ERubberbandable::kTextWithTab == layoutObject->Style()->Rubberbandable();
}

static WTF::String getRubberbandGroupDelimiter(const LayoutObject* layoutObject)
{
    if (!layoutObject->Style()) {
        return "";
    }
    return layoutObject->Style()->BbRubberbandGroupDelimiter();
}

static WTF::String getRubberbandEmptyText(const LayoutObject* layoutObject)
{
    if (!layoutObject->Style()) {
        return "";
    }
    return layoutObject->Style()->BbRubberbandEmptyText();
}

template <typename CHAR_TYPE>
static void appendWithoutNewlines(WTF::StringBuilder *builder, const CHAR_TYPE* chars, unsigned length)
{
    builder->ReserveCapacity(length);
    const CHAR_TYPE* end = chars + length;
    while (chars != end) {
        if ('\n' != *chars) {
            builder->Append(*chars);
        }
        ++chars;
    }
}

static IntRect getRubberbandRect(const IntPoint& start, const IntPoint& extent)
{
    IntRect rc;
    rc.ShiftXEdgeTo(std::min(start.X(), extent.X()));
    rc.ShiftMaxXEdgeTo(std::max(start.X(), extent.X()));
    rc.ShiftYEdgeTo(std::min(start.Y(), extent.Y()));
    rc.ShiftMaxYEdgeTo(std::max(start.Y(), extent.Y()));
    return rc;
}

void WebViewImpl::RubberbandWalkFrame(const RubberbandContext& context, const LocalFrame* frame, const LayoutPoint& clientTopLeft)
{
    frame->GetDocument()->UpdateStyleAndLayout();

    LocalFrameView* view = frame->View();

    LayoutObject* layoutObject = frame->ContentLayoutObject();
    if (!layoutObject || !layoutObject->HasLayer())
        return;

    RubberbandContext localContext(&context, 0);
    localContext.m_containingBlock = 0;
    localContext.m_parent = 0;
    localContext.m_layoutTopLeft = LayoutPoint::Zero();

    RubberbandLayerContext layerContext;
    localContext.m_layerContext = &layerContext;

    if (context.m_layerContext) {
        layerContext.m_layerScrollOffset = context.m_layerContext->m_layerScrollOffset;
        layerContext.m_translateX = context.m_layerContext->m_translateX + clientTopLeft.X();
        layerContext.m_translateY = context.m_layerContext->m_translateY + clientTopLeft.Y();
        layerContext.m_scaleX = context.m_layerContext->m_scaleX;
        layerContext.m_scaleY = context.m_layerContext->m_scaleY;

        layerContext.m_clipRect = localContext.calcAbsRect(LayoutRect(LayoutPoint(), LayoutSize(view->Size())));
        layerContext.m_clipRect.Intersect(context.m_layerContext->m_clipRect);
    }
    else {
        layerContext.m_translateX = clientTopLeft.X();
        layerContext.m_translateY = clientTopLeft.Y();
        layerContext.m_clipRect = localContext.calcAbsRect(LayoutRect(LayoutPoint(), LayoutSize(view->Size())));
    }

    if (layerContext.m_clipRect.IsEmpty()) {
        return;
    }

    RubberbandWalkLayoutObject(localContext, layoutObject);
}

void WebViewImpl::RubberbandWalkLayoutObject(const RubberbandContext& context, const LayoutObject* layoutObject)
{
    RubberbandContext localContext(&context, layoutObject);

    if (layoutObject->HasLayer()) {
        DCHECK(layoutObject->IsBoxModelObject());
        PaintLayer* layer = ToLayoutBoxModelObject(layoutObject)->Layer();
        RubberbandLayerContext& layerContext = *localContext.m_layerContext;

        auto location = layer->Location().ToLayoutPoint();

        if (layer->HasTransformRelatedProperty()) {
            TransformationMatrix matrix = layer->CurrentTransform();
            if (!isSupportedTransform(matrix)) {
                return;
            }

            layerContext.m_translateX += layerContext.m_scaleX * (matrix.M41() + location.X());
            layerContext.m_translateY += layerContext.m_scaleY * (matrix.M42() + location.Y());
            layerContext.m_scaleX *= matrix.M11();
            layerContext.m_scaleY *= matrix.M22();
        }
        else if (location != LayoutPoint::Zero()) {
            layerContext.m_translateX += layerContext.m_scaleX * location.X();
            layerContext.m_translateY += layerContext.m_scaleY * location.Y();
        }

        // TODO: how should we clip layers that are in columns?
        if (layer->GetLayoutObject().Style() && !layerContext.m_colBlock) {
            bool isClippedX = isClipped(layer->GetLayoutObject().Style()->OverflowX());
            bool isClippedY = isClipped(layer->GetLayoutObject().Style()->OverflowY());
            if (isClippedX || isClippedY) {
                LayoutPoint minXminY = localContext.calcAbsPoint(LayoutPoint::Zero());
                if (isClippedX) {
                    LayoutUnit minX = LayoutUnit(minXminY.X());
                    LayoutUnit maxX = LayoutUnit(minX + (layer->Size().Width() * layerContext.m_scaleX));
                    layerContext.m_clipRect.ShiftXEdgeTo(std::max(minX, layerContext.m_clipRect.X()));
                    layerContext.m_clipRect.ShiftMaxXEdgeTo(std::min(maxX, layerContext.m_clipRect.MaxX()));
                }
                if (isClippedY) {
                    LayoutUnit minY = LayoutUnit(minXminY.Y());
                    LayoutUnit maxY = LayoutUnit(minY + (layer->Size().Height() * layerContext.m_scaleY));
                    layerContext.m_clipRect.ShiftYEdgeTo(std::max(minY, layerContext.m_clipRect.Y()));
                    layerContext.m_clipRect.ShiftMaxYEdgeTo(std::min(maxY, layerContext.m_clipRect.MaxY()));
                }

                if (layerContext.m_clipRect.IsEmpty()) {
                    return;
                }
            }
        }

        if (layer->GetScrollableArea()) {
            layerContext.m_layerScrollOffset.SetWidth(layer->GetScrollableArea()->GetScrollOffset().Width() * layerContext.m_scaleX);
            layerContext.m_layerScrollOffset.SetHeight(layer->GetScrollableArea()->GetScrollOffset().Height() * layerContext.m_scaleY);
        }
    }
    else if (localContext.m_containingBlock != context.m_containingBlock) {
        DCHECK(localContext.m_containingBlock);
        const LayoutBlock* cb = localContext.m_containingBlock;
        const RubberbandContext* cbContext = &context;
        while (cbContext->m_layoutObject != cb) {
            DCHECK(cbContext->m_parent);
            cbContext = cbContext->m_parent;
        }

        DCHECK(cbContext->m_parent);
        DCHECK(cbContext->m_parent->m_layerContext);

        LayoutPoint offset;
        if (!cb->HasLayer() && cbContext->m_layerContext == localContext.m_layerContext)
            offset.Move(cb->FrameRect().X(), cb->FrameRect().Y());

        // undo previous containing block offsets (TODO)


        localContext.m_layoutTopLeft.MoveBy(offset);
    }

    bool isVisible = !layoutObject->Style() || layoutObject->Style()->Visibility() == EVisibility::kVisible;

    // Keep the current candidate count
    const unsigned int candidateCnt = rubberbandState_->impl_->m_candidates.size();

    if (layoutObject->IsBox()) {
        const LayoutBox* layoutBox = ToLayoutBox(layoutObject);

        // HACK: LayoutTableSection is not a containing block, but the cell
        //       positions seem to be relative to LayoutTableSection instead of
        //       LayoutTable.  This hack moves the top-left corner by the x,y
        //       position of the LayoutTableSection.
        if (layoutObject->IsTableSection() && !layoutObject->HasLayer()) {
            localContext.m_layoutTopLeft.Move(layoutBox->FrameRect().X(), layoutBox->FrameRect().Y());
        }

        if (layoutObject->IsLayoutIFrame() && isVisible) {
            const LayoutIFrame* layoutIFrame = ToLayoutIFrame(layoutObject);
            if (layoutIFrame->ChildFrameView()) {
                LocalFrameView* frameView = DynamicTo<LocalFrameView>(layoutIFrame->ChildFrameView());
                LayoutPoint topLeft;
                if (!layoutObject->HasLayer()) {
                    topLeft.Move((localContext.m_layoutTopLeft.X() + layoutBox->FrameRect().X()) * context.m_layerContext->m_scaleX,
                                 (localContext.m_layoutTopLeft.Y() + layoutBox->FrameRect().Y()) * context.m_layerContext->m_scaleY);
                }
                topLeft.Move((layoutBox->BorderLeft() + layoutBox->PaddingLeft()) * localContext.m_layerContext->m_scaleX,
                             (layoutBox->BorderTop() + layoutBox->PaddingTop()) * localContext.m_layerContext->m_scaleY);
                RubberbandWalkFrame(localContext, &frameView->GetFrame(), topLeft);
            }
        }
    }
    else if (layoutObject->IsText() && isVisible && isTextRubberbandable(layoutObject)) {
        const LayoutText* layoutText = ToLayoutText(layoutObject);

        WTF::String text(layoutText->GetText());
        if (layoutText->IsLayoutNGText()) {
          appendCandidatesByLayoutNGText(localContext, layoutText,
                                         rubberbandState_->impl_->m_candidates);
        } else {
          for (InlineTextBox* textBox = layoutText->FirstTextBox(); textBox; textBox = textBox->NextForSameLayoutObject()) {
            LayoutUnit textBoxTop = textBox->Root().LineTop();
            LayoutUnit textBoxHeight = textBox->Root().LineBottom() - textBoxTop;
            LayoutUnit textBoxLeft = textBox->X();
            LayoutUnit textBoxWidth = textBox->Width();

            LayoutRect localRect(textBoxLeft, textBoxTop, textBoxWidth, textBoxHeight);
            LayoutRect absRect = localContext.calcAbsRect(localRect);
            if (absRect.Intersects(localContext.m_layerContext->m_clipRect)) {
                rubberbandState_->impl_->m_candidates.push_back(RubberbandCandidate());

                RubberbandCandidate& candidate = rubberbandState_->impl_->m_candidates.back();
                candidate.m_absRect = absRect;
                candidate.m_clipRect = candidate.m_absRect;
                candidate.m_clipRect.Intersect(localContext.m_layerContext->m_clipRect);
                candidate.m_text = text;
                candidate.m_isLTR = textBox->IsLeftToRightDirection();
                candidate.m_useLeadingTab = isTextWithTab(layoutText);
                candidate.m_start = textBox->Start();
                candidate.m_len = textBox->Len();
                candidate.m_groupId = localContext.m_groupId;
                candidate.m_groupDelimiter = localContext.m_groupDelimiter;

                int end = candidate.m_start + candidate.m_len;
                for (int offset = candidate.m_start; offset <= end; ++offset) {
                    LayoutUnit pos = textBox->PositionForOffset(offset) - textBox->LogicalLeft();
                    pos *= localContext.m_layerContext->m_scaleX;
                    if (candidate.m_isLTR)
                        pos += candidate.m_absRect.X();
                    else
                        pos = candidate.m_absRect.MaxX() - pos;
                    candidate.m_charPositions.push_back(pos);
                }

                {
                    const Font& font = layoutObject->Style()->GetFont();
                    UChar space = ' ';
                    candidate.m_spaceWidth = LayoutUnit(font.Width(
                          ConstructTextRun(font, &space, 1, layoutObject->StyleRef(), candidate.m_isLTR ? TextDirection::kLtr : TextDirection::kRtl))
                        * localContext.m_layerContext->m_scaleX);
                }
            }
          }
        }
    }

    // If the current node has '-bb-rubberband-group-delimiter' set,
    // keep the group delimiter and the group id to localContext so the children use them
    const WTF::String& groupDelimiter = getRubberbandGroupDelimiter(layoutObject);
    if (groupDelimiter && groupDelimiter.length() > 0) {
        localContext.m_groupDelimiter = groupDelimiter;
        localContext.m_groupId = s_groupId++;
    }

    // Walk down the children
    for (LayoutObject* child = layoutObject->SlowFirstChild(); child; child = child->NextSibling()) {
        RubberbandWalkLayoutObject(localContext, child);
    }

    const bool candidateAdded = (candidateCnt < rubberbandState_->impl_->m_candidates.size());

    // 1. Check if no candidate was created (either empty or has no text child)
    if (!candidateAdded && isVisible && isTextWithTab(layoutObject)) {
        // Check if the layoutObject has '-bb-rubberband-empty-text' set.
        // If the empty text was specified, create a dummy candidate with the value
        const WTF::String& emptyText = getRubberbandEmptyText(layoutObject);
        if (emptyText && emptyText.length() > 0) {
            rubberbandState_->impl_->m_candidates.push_back(RubberbandCandidate());
            RubberbandCandidate& candidate = rubberbandState_->impl_->m_candidates.back();
            candidate.m_absRect = localContext.m_layerContext->m_clipRect;
            candidate.m_clipRect = localContext.m_layerContext->m_clipRect;
            candidate.m_text = emptyText;
            candidate.m_isLTR = true;
            candidate.m_useLeadingTab = true;
            candidate.m_start = 0;
            candidate.m_len = emptyText.length();
            candidate.m_charPositions.push_back(candidate.m_absRect.X());
            candidate.m_groupId = -1;

            {
                const Font& font = layoutObject->Style()->GetFont();
                UChar space = ' ';
                candidate.m_spaceWidth = LayoutUnit(font.Width(
                    ConstructTextRun(font, &space, 1, layoutObject->StyleRef(), candidate.m_isLTR ? TextDirection::kLtr : TextDirection::kRtl))
                    * localContext.m_layerContext->m_scaleX);
            }
        }
    }
}

WTF::String WebViewImpl::GetTextInRubberbandImpl(const LayoutRect& rcOrig)
{
    DCHECK(IsRubberbanding());

    // Log the given rect for future reference
    LOG(INFO) << "Rubberband requested with rect: (" << rcOrig << ")";

    LayoutRect rc(rcOrig);

    const auto& stateImpl = rubberbandState_->impl_;

    std::vector<RubberbandCandidate> hits;
    for (std::size_t i = 0; i < stateImpl->m_candidates.size(); ++i) {
        const RubberbandCandidate& candidate = stateImpl->m_candidates[i];
        if (candidate.m_clipRect.Intersects(rc)) {
            RubberbandCandidate hit = candidate;

            // Don't include any items that are over the bottom edge of the rcOrig.
            if (candidate.m_absRect.MaxY().Floor() > rc.MaxY().Ceil()) {
                continue;
            }

            hit.m_clipRect.Intersect(rc);
            hits.push_back(hit);
        }
    }

    if (hits.empty()) {
        return WTF::g_empty_string;
    }

    std::sort(hits.begin(), hits.end(), RubberbandCandidate_YSorter());
    LayoutUnit firstHitTop = hits[0].m_absRect.Y();
    LayoutUnit firstHitHeight = hits[0].m_absRect.Height();

    std::vector<std::size_t> lineBreaks;
    {
        LayoutUnit currLineTop = hits[0].m_absRect.Y();
        LayoutUnit currLineBottom = hits[0].m_absRect.MaxY();
        std::size_t currLineStart = 0;
        for (std::size_t i = 1; i < hits.size(); ++i) {
            const RubberbandCandidate& hit = hits[i];

            DCHECK(currLineTop <= hit.m_absRect.Y());

            LayoutUnit maxTop = std::max(currLineTop, hit.m_absRect.Y());
            LayoutUnit minBottom = std::min(currLineBottom, hit.m_absRect.MaxY());
            LayoutUnit potentialLineHeight = std::max(currLineBottom, hit.m_absRect.MaxY()) - currLineTop;
            if (minBottom - maxTop > std::min(hit.m_absRect.Height(), potentialLineHeight) / 2) {
                // It overlaps sufficiently in the Y direction.  Now, if it
                // doesn't overlap with anything on the current line in the X
                // direction, we can keep this on the same row.
                bool overlapsInXDirection = false;
                if (!hit.isAllWhitespaces()) {
                    for (std::size_t j = currLineStart; j < i; ++j) {
                        const RubberbandCandidate& xtest = hits[j];
                        if (xtest.isAllWhitespaces()) {
                            continue;
                        }
                        LayoutUnit maxLeft = std::max(xtest.m_clipRect.X(), hit.m_clipRect.X());
                        LayoutUnit minRight = std::min(xtest.m_clipRect.MaxX(), hit.m_clipRect.MaxX());
                        if (minRight - maxLeft > 0.5) { // threshold of half pixel
                            overlapsInXDirection = true;
                            break;
                        }
                    }
                }
                if (!overlapsInXDirection) {
                    currLineBottom = std::max(currLineBottom, hit.m_absRect.MaxY());
                    continue;
                }
            }

            lineBreaks.push_back(i);
            currLineTop = hit.m_absRect.Y();
            currLineBottom = hit.m_absRect.MaxY();
            currLineStart = i;
        }
    }

    {
        std::size_t lastLineBreak = 0;
        for (std::size_t i = 0; i < lineBreaks.size(); ++i) {
            std::sort(hits.begin() + lastLineBreak,
                      hits.begin() + lineBreaks[i],
                      RubberbandCandidate_XSorter());
            lastLineBreak = lineBreaks[i];
        }
        std::sort(hits.begin() + lastLineBreak, hits.end(), RubberbandCandidate_XSorter());
    }

    WTF::StringBuilder builder;
    LayoutUnit lastX = rc.X();
    bool lastHitWasAllWhitespaces = false;

    // Add sufficient newlines to get from the top of the rubberband to the first hit.
    {
        LayoutUnit y = rc.Y() + firstHitHeight;
        while (y <= firstHitTop) {
            builder.Append('\n');
            y += firstHitHeight;
        }
    }

    LayoutUnit lineTop = hits[0].m_absRect.Y();
    LayoutUnit lineBottom = hits[0].m_absRect.MaxY();

    std::size_t nextLineBreak = 0;
    bool newLine = true;
    for (std::size_t i = 0; i < hits.size(); ++i) {
        const RubberbandCandidate& hit = hits[i];

        if (nextLineBreak < lineBreaks.size() && i == lineBreaks[nextLineBreak]) {
            ++nextLineBreak;
            builder.Append('\n');
            lastX = rc.X();

            LayoutUnit lastLineHeight = lineBottom - lineTop;
            LayoutUnit y = lineBottom + lastLineHeight;
            while (y < hit.m_absRect.Y()) {
                builder.Append('\n');
                y += lastLineHeight;
            }

            lineTop = hit.m_absRect.Y();
            lineBottom = hit.m_absRect.MaxY();
            newLine = true;
        }
        else {
            lineTop = std::min(lineTop, hit.m_absRect.Y());
            lineBottom = std::max(lineBottom, hit.m_absRect.MaxY());
            newLine = false;
        }

        DCHECK(lastX <= hit.m_clipRect.X() || hit.isAllWhitespaces() || lastHitWasAllWhitespaces);

        if (hit.m_useLeadingTab) {
            // Don't add a tab in front of a new line
            if (i > 0 && !newLine) {
                bool shouldPrependTab = true;

                // If the current candidate is in the same group as the previous candidate, don't add tab.
                if (hit.m_groupId >= 0 && (hit.m_groupId == hits[i-1].m_groupId)) {
                    shouldPrependTab = false;
                }

                if (shouldPrependTab)
                {
                    builder.Append('\t');
                }
                else {
                    // The current candidate is in the same group. Check the group delimiter.
                    if (hit.m_groupDelimiter == " ") {
                        // If the group delimiter is a space, fill the gap with spaces
                        if (hit.m_clipRect.X() > lastX) {
                            LayoutUnit x = lastX + hit.m_spaceWidth;
                            while (x <= hit.m_clipRect.X()) {
                                builder.Append(' ');
                                x += hit.m_spaceWidth;
                            }
                        }
                    }
                    else {
                        // If the group delimiter is not a space, just insert one delimiter
                        builder.Append(hit.m_groupDelimiter);
                    }
                }
            }
        }
        else if (hit.m_clipRect.X() > lastX) {
            LayoutUnit x = lastX + hit.m_spaceWidth;
            while (x <= hit.m_clipRect.X()) {
                builder.Append(' ');
                x += hit.m_spaceWidth;
            }
        }

        DCHECK(!hit.m_clipRect.IsEmpty());
        DCHECK(hit.m_clipRect.X() >= hit.m_absRect.X());
        DCHECK(hit.m_clipRect.MaxX() <= hit.m_absRect.MaxX());

        if (hit.m_clipRect.X() == hit.m_absRect.X() && hit.m_clipRect.MaxX() == hit.m_absRect.MaxX()) {
            if (hit.m_text.Is8Bit()) {
                appendWithoutNewlines(&builder, hit.m_text.Characters8() + hit.m_start, hit.m_len);
            }
            else {
                appendWithoutNewlines(&builder, hit.m_text.Characters16() + hit.m_start, hit.m_len);
            }
        }
        else {
            int startOffset = 0, endOffset = hit.m_len;
            if (hit.m_isLTR) {
                for (std::size_t j = 0; j < hit.m_charPositions.size() - 1; ++j) {
                    LayoutUnit minX = hit.m_charPositions[j];
                    LayoutUnit maxX = hit.m_charPositions[j+1];
                    if (hit.m_clipRect.X() >= minX && hit.m_clipRect.X() < maxX) {
                        startOffset = j;
                    }
                    if (hit.m_clipRect.MaxX() > minX && hit.m_clipRect.MaxX() <= maxX) {
                        endOffset = j + 1;
                        break;
                    }
                }
            }
            else {
                for (std::size_t j = 0; j < hit.m_charPositions.size() - 1; ++j) {
                    LayoutUnit maxX = hit.m_charPositions[j];
                    LayoutUnit minX = hit.m_charPositions[j+1];
                    if (hit.m_clipRect.MaxX() > minX && hit.m_clipRect.MaxX() <= maxX) {
                        startOffset = j + 1;
                    }
                    if (hit.m_clipRect.X() >= minX && hit.m_clipRect.X() < maxX) {
                        endOffset = j;
                        break;
                    }
                }
            }

            DCHECK(startOffset <= endOffset);
            DCHECK(0 <= startOffset);
            DCHECK(startOffset <= hit.m_len);
            DCHECK(0 <= endOffset);
            DCHECK(endOffset <= hit.m_len);

            if (hit.m_text.Is8Bit()) {
                appendWithoutNewlines(&builder,
                                      hit.m_text.Characters8() + hit.m_start + startOffset,
                                      endOffset - startOffset);
            }
            else {
                appendWithoutNewlines(&builder,
                                      hit.m_text.Characters16() + hit.m_start + startOffset,
                                      endOffset - startOffset);
            }
        }

        lastX = hit.m_clipRect.MaxX();
        lastHitWasAllWhitespaces = hit.isAllWhitespaces();
    }

    return builder.ToString();
}

bool WebViewImpl::ForceStartRubberbanding(int x, int y)
{
    rubberbandingForcedOn_ = true;

    if (IsRubberbanding()) {
        return true;
    }

    if (!PreStartRubberbanding()) {
        return false;
    }

    StartRubberbanding();
    rubberbandState_->impl_->m_startPoint = IntPoint(x, y);
    return true;
}

bool WebViewImpl::HandleAltDragRubberbandEvent(const WebInputEvent& inputEvent)
{
    if (!rubberbandingForcedOn_ && !(inputEvent.GetModifiers() & WebInputEvent::kAltKey)) {
        if (IsRubberbanding()) {
            AbortRubberbanding();
        }
        return false;
    }

    if (!WebInputEvent::IsMouseEventType(inputEvent.GetType()))
        return false;

    const WebMouseEvent& mouseEvent = *static_cast<const WebMouseEvent*>(&inputEvent);
    auto positionInWidget = mouseEvent.PositionInWidget();

    if (!IsRubberbanding()) {
        if (inputEvent.GetType() == WebInputEvent::kMouseDown && PreStartRubberbanding()) {
            // set the rubberbandingForcedOn_ to true not to abort the rubberband
            // when 'alt' is lifted before the mouse button.
            rubberbandingForcedOn_ = true;
            StartRubberbanding();
            rubberbandState_->impl_->m_startPoint = IntPoint(positionInWidget.x, positionInWidget.y);
            return true;
        }

        return false;
    }
    else if (inputEvent.GetType() == WebInputEvent::kMouseUp) {
        IntPoint start = rubberbandState_->impl_->m_startPoint;
        IntPoint extent = IntPoint(positionInWidget.x, positionInWidget.y);
        LayoutRect rc = ExpandRubberbandRectImpl(getRubberbandRect(start, extent));
        if (rc.IsEmpty()) {
            AbortRubberbanding();
        }
        else {
            WebString copied = FinishRubberbandingImpl(rc);
            SystemClipboard::GetInstance().WritePlainText(copied, SystemClipboard::kCannotSmartReplace);
            SystemClipboard::GetInstance().CommitWrite();
        }

        return true;
    }
    else {
        WebViewClient* client = AsView().client;
        if (client) {
            IntPoint start = rubberbandState_->impl_->m_startPoint;
            IntPoint extent = IntPoint(positionInWidget.x, positionInWidget.y);
            WebRect rc = ExpandRubberbandRect(getRubberbandRect(start, extent));
            client->setRubberbandRect(rc);
        }
        return true;
    }
}

bool WebViewImpl::IsAltDragRubberbandingEnabled() const
{
    return isAltDragRubberbandingEnabled_;
}

void WebViewImpl::EnableAltDragRubberbanding(bool value)
{
    isAltDragRubberbandingEnabled_ = value;
    if (!isAltDragRubberbandingEnabled_ && IsRubberbanding()) {
        AbortRubberbanding();
    }
}

bool WebViewImpl::IsRubberbanding() const
{
    return rubberbandState_.get();
}

bool WebViewImpl::PreStartRubberbanding()
{
    DCHECK(!IsRubberbanding());
    Page* page = AsView().page;
    if (!page || !page->MainFrame() || !page->MainFrame()->IsLocalFrame() || !page->DeprecatedLocalMainFrame()->GetDocument())
        return false;

    Event* event = Event::CreateCancelable("rubberbandstarting");
    if (DispatchEventResult::kCanceledByEventHandler == page->DeprecatedLocalMainFrame()->GetDocument()->DispatchEvent(*event))
        return false;

    return !event->defaultPrevented();
}

void WebViewImpl::StartRubberbanding()
{
    DCHECK(!IsRubberbanding());

    rubberbandState_ = std::unique_ptr<RubberbandState>(new RubberbandState());

    s_groupId = 0; // reset group id

    RubberbandContext context;
    RubberbandWalkFrame(context, AsView().page->DeprecatedLocalMainFrame(), LayoutPoint());
}

WebRect WebViewImpl::ExpandRubberbandRect(const WebRect& rcOrig)
{
    LayoutRect rc = ExpandRubberbandRectImpl(rcOrig);
    return PixelSnappedIntRect(rc);
}

LayoutRect WebViewImpl::ExpandRubberbandRectImpl(const WebRect& rcOrig)
{
    DCHECK(IsRubberbanding());

    LayoutRect rc(rcOrig);
    LayoutRect origRect(rcOrig);


    const auto& stateImpl = rubberbandState_->impl_;

    std::size_t i = 0;
    while (i < stateImpl->m_candidates.size()) {
        const RubberbandCandidate& candidate = stateImpl->m_candidates[i];
        if (candidate.m_clipRect.Intersects(rc)) {
            bool expanded = false;
            LayoutUnit y = candidate.m_absRect.Y();
            LayoutUnit maxY = candidate.m_absRect.MaxY();
            if (rc.Y() > y) {
                rc.ShiftYEdgeTo(y);
                expanded = true;
            }

            if (rc.MaxY() < maxY && y < origRect.MaxY()) {
                rc.ShiftMaxYEdgeTo(maxY);
                expanded = true;
            }
            if (expanded) {
                // since we expanded the rect, there might be more candidates that get included
                i = 0;
                continue;
            }
        }
        ++i;
    }

    LayoutUnit minX = rc.X();
    LayoutUnit maxX = rc.MaxX();

    for (i = 0; i < stateImpl->m_candidates.size(); ++i) {
        RubberbandCandidate& candidate = stateImpl->m_candidates[i];
        if (candidate.m_clipRect.Intersects(rc)) {
            for (std::size_t j = 0; j < candidate.m_charPositions.size() - 1; ++j) {
                LayoutUnit startOfChar = candidate.m_charPositions[j];
                LayoutUnit endOfChar = candidate.m_charPositions[j+1];
                if (candidate.m_isLTR) {
                    if (rc.X() > startOfChar && rc.X() < endOfChar) {
                        minX = std::min(minX, startOfChar + 1);
                    }
                    if (rc.MaxX() > startOfChar && rc.MaxX() < endOfChar) {
                        maxX = std::max(maxX, endOfChar);
                    }
                }
                else {
                    if (rc.X() > endOfChar && rc.X() < startOfChar) {
                        minX = std::min(minX, endOfChar);
                    }
                    if (rc.MaxX() > endOfChar && rc.MaxX() < startOfChar) {
                        maxX = std::max(maxX, startOfChar - 1);
                    }
                }
            }
        }
    }

    rc.ShiftXEdgeTo(minX);
    rc.ShiftMaxXEdgeTo(maxX);

    return rc;
}

WebString WebViewImpl::FinishRubberbanding(const WebRect& rc)
{
    LayoutRect layoutRc(rc);
    return FinishRubberbandingImpl(layoutRc);
}

WebString WebViewImpl::FinishRubberbandingImpl(const LayoutRect& rc)
{
    DCHECK(IsRubberbanding());

    WebViewClient* client = AsView().client;
    if (client)
        client->hideRubberbandRect();

    WTF::String copied = GetTextInRubberbandImpl(rc);

    rubberbandState_.reset(nullptr);
    rubberbandingForcedOn_ = false;
    Page* page = AsView().page;
    if (page && page->MainFrame() && page->MainFrame()->IsLocalFrame() && page->DeprecatedLocalMainFrame()->GetDocument()) {
        Event* event = Event::Create("rubberbandfinished");
        page->DeprecatedLocalMainFrame()->GetDocument()->DispatchEvent(*event);
    }

    return copied;
}

void WebViewImpl::AbortRubberbanding()
{
    DCHECK(IsRubberbanding());


    WebViewClient* client = AsView().client;
    if (client)
        client->hideRubberbandRect();

    rubberbandState_.reset(nullptr);
    rubberbandingForcedOn_ = false;

    Page* page = AsView().page;
    if (page && page->MainFrame() && page->MainFrame()->IsLocalFrame() && page->DeprecatedLocalMainFrame()->GetDocument()) {
        Event* event = Event::Create("rubberbandaborted");
        page->DeprecatedLocalMainFrame()->GetDocument()->DispatchEvent(*event);
    }
}

WebString WebViewImpl::GetTextInRubberband(const WebRect& rc)
{
    DCHECK(!IsRubberbanding());

    Page* page = AsView().page;
    if (!page || !page->MainFrame() || rc.IsEmpty() || !page->MainFrame()->IsLocalFrame())
        return WTF::g_empty_string;

    rubberbandState_ = std::unique_ptr<RubberbandState>(new RubberbandState());

    RubberbandContext context;
    RubberbandLayerContext layerContext;
    context.m_layerContext = &layerContext;
    layerContext.m_clipRect = LayoutRect(rc);
    RubberbandWalkFrame(context, page->DeprecatedLocalMainFrame(), LayoutPoint());
    LayoutRect layoutRc(rc);
    WTF::String result = GetTextInRubberbandImpl(layoutRc);
    rubberbandState_.reset(nullptr);
    return result;
}

void appendCandidatesByLayoutNGText(
    const RubberbandContext& localContext,
    const LayoutText* layoutText,
    std::vector<RubberbandCandidate>& candidates)
{
    static const UChar space = ' ';
    const Font& font = layoutText->Style()->GetFont();
    for (const NGPaintFragment* paintFragment :
        NGPaintFragment::InlineFragmentsFor(layoutText)) {
        const auto& physicalFragment = paintFragment->PhysicalFragment();
        if (!physicalFragment.IsText()) {
            continue;
        }
        const blink::NGPhysicalTextFragment& physicalTextFragment =
            blink::To<blink::NGPhysicalTextFragment>(physicalFragment);
        const blink::PhysicalOffset& offset = paintFragment->InlineOffsetToContainerBox();
        const blink::LayoutSize fragSize{physicalTextFragment.Size().width,
                                         physicalTextFragment.Size().height};
        LayoutRect localRect{offset.ToLayoutPoint(), fragSize};
        // NG needs to take m_layerScrollOffset into account
        localRect.Move(-localContext.m_layerContext->m_layerScrollOffset.Width(),
                       -localContext.m_layerContext->m_layerScrollOffset.Height());
        LayoutRect absRect = localContext.calcAbsRect(localRect);
        if (!absRect.Intersects(localContext.m_layerContext->m_clipRect)) {
            continue;
        }

        candidates.push_back(RubberbandCandidate());
        RubberbandCandidate& candidate = candidates.back();
        candidate.m_absRect = absRect;
        candidate.m_clipRect = candidate.m_absRect;
        candidate.m_clipRect.Intersect(localContext.m_layerContext->m_clipRect);
        candidate.m_text = physicalTextFragment.Text().ToString();
        candidate.m_isLTR = blink::IsLtr(physicalFragment.ResolvedDirection());
        candidate.m_useLeadingTab = isTextWithTab(layoutText);
        // startOffset is indexed to layoutText->GetText()
        int startOffset = physicalTextFragment.StartOffset();
        // m_start is indexed to candidate.m_text, which is also physicalTextFragment.Text()
        candidate.m_start = 0;
        candidate.m_len = physicalTextFragment.Length();
        candidate.m_groupId = localContext.m_groupId;
        candidate.m_groupDelimiter = localContext.m_groupDelimiter;

        candidate.m_spaceWidth = LayoutUnit(
            font.Width(ConstructTextRun(
                font, &space, 1, layoutText->StyleRef(),
                candidate.m_isLTR ? TextDirection::kLtr : TextDirection::kRtl)) *
            localContext.m_layerContext->m_scaleX);

        candidate.m_charPositions.reserve(candidate.m_len);
        for (int id = startOffset; id < startOffset + candidate.m_len; ++id) {
            auto localRect = physicalTextFragment.LocalRect(id, id + 1);
            LayoutUnit pos = localRect.X();
            pos *= localContext.m_layerContext->m_scaleX;
            if (candidate.m_isLTR)
                pos += candidate.m_absRect.X();
            else
                pos = candidate.m_absRect.MaxX() - pos;
            candidate.m_charPositions.push_back(pos);
        }
    }
}

}  // namespace blink
