/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderVTTCue.h"

#include "core/html/track/vtt/VTTCue.h"
#include "core/rendering/LayoutState.h"
#include "core/rendering/RenderInline.h"

namespace blink {

RenderVTTCue::RenderVTTCue(VTTCueBox* element)
    : RenderBlockFlow(element)
    , m_cue(element->getCue())
{
}

void RenderVTTCue::trace(Visitor* visitor)
{
    visitor->trace(m_cue);
    RenderBlockFlow::trace(visitor);
}

class SnapToLinesLayouter {
    STACK_ALLOCATED();
public:
    SnapToLinesLayouter(RenderVTTCue& cueBox, VTTCue::WritingDirection writingDirection, float linePosition)
        : m_cueBox(cueBox)
        , m_cueWritingDirection(writingDirection)
        , m_linePosition(linePosition)
    {
    }

    void layout();

private:
    bool isOutside() const;
    bool isOverlapping() const;
    bool shouldSwitchDirection(InlineFlowBox*, LayoutUnit) const;

    void moveBoxesByStep(LayoutUnit);
    bool switchDirection(bool&, LayoutUnit&);

    bool findFirstLineBox(InlineFlowBox*&);
    bool initializeLayoutParameters(InlineFlowBox*, LayoutUnit&, LayoutUnit&);
    void placeBoxInDefaultPosition(LayoutUnit, bool&);

    LayoutPoint m_specifiedPosition;
    RenderVTTCue& m_cueBox;
    VTTCue::WritingDirection m_cueWritingDirection;
    float m_linePosition;
};

bool SnapToLinesLayouter::findFirstLineBox(InlineFlowBox*& firstLineBox)
{
    if (m_cueBox.firstChild()->isRenderInline())
        firstLineBox = toRenderInline(m_cueBox.firstChild())->firstLineBox();
    else
        return false;

    return true;
}

bool SnapToLinesLayouter::initializeLayoutParameters(InlineFlowBox* firstLineBox, LayoutUnit& step, LayoutUnit& position)
{
    ASSERT(m_cueBox.firstChild());

    // 4. Horizontal: Let step be the height of the first line box in boxes.
    //    Vertical: Let step be the width of the first line box in boxes.
    step = m_cueWritingDirection == VTTCue::Horizontal ? firstLineBox->size().height() : firstLineBox->size().width();

    // 5. If step is zero, then jump to the step labeled done positioning below.
    if (!step)
        return false;

    // 6. Let line position be the text track cue computed line position.
    // 7. Round line position to an integer by adding 0.5 and then flooring it.
    LayoutUnit linePosition = floorf(m_linePosition + 0.5f);

    // 8. Vertical Growing Left: Add one to line position then negate it.
    if (m_cueWritingDirection == VTTCue::VerticalGrowingLeft)
        linePosition = -(linePosition + 1);

    // 9. Let position be the result of multiplying step and line position.
    position = step * linePosition;

    // 10. Vertical Growing Left: Decrease position by the width of the
    // bounding box of the boxes in boxes, then increase position by step.
    if (m_cueWritingDirection == VTTCue::VerticalGrowingLeft) {
        position -= m_cueBox.size().width();
        position += step;
    }

    // 11. If line position is less than zero...
    if (linePosition < 0) {
        RenderBlock* parentBlock = m_cueBox.containingBlock();

        // Horizontal / Vertical: ... then increase position by the
        // height / width of the video's rendering area ...
        position += m_cueWritingDirection == VTTCue::Horizontal ? parentBlock->size().height() : parentBlock->size().width();

        // ... and negate step.
        step = -step;
    }
    return true;
}

void SnapToLinesLayouter::placeBoxInDefaultPosition(LayoutUnit position, bool& switched)
{
    // 12. Move all boxes in boxes ...
    if (m_cueWritingDirection == VTTCue::Horizontal) {
        // Horizontal: ... down by the distance given by position
        m_cueBox.setY(m_cueBox.location().y() + position);
    } else {
        // Vertical: ... right by the distance given by position
        m_cueBox.setX(m_cueBox.location().x() + position);
    }

    // 13. Remember the position of all the boxes in boxes as their specified position.
    m_specifiedPosition = m_cueBox.location();

    // 14. Let best position be null. It will hold a position for boxes, much like specified position in the previous step.
    // 15. Let best position score be null.

    // 16. Let switched be false.
    switched = false;
}

bool SnapToLinesLayouter::isOutside() const
{
    return !m_cueBox.containingBlock()->absoluteBoundingBoxRect().contains(m_cueBox.absoluteContentBox());
}

bool SnapToLinesLayouter::isOverlapping() const
{
    for (RenderObject* box = m_cueBox.previousSibling(); box; box = box->previousSibling()) {
        IntRect boxRect = box->absoluteBoundingBoxRect();

        if (m_cueBox.absoluteBoundingBoxRect().intersects(boxRect))
            return true;
    }

    return false;
}

bool SnapToLinesLayouter::shouldSwitchDirection(InlineFlowBox* firstLineBox, LayoutUnit step) const
{
    LayoutUnit top = m_cueBox.location().y();
    LayoutUnit left = m_cueBox.location().x();
    LayoutUnit bottom = top + firstLineBox->size().height();
    LayoutUnit right = left + firstLineBox->size().width();

    // 21. Horizontal: If step is negative and the top of the first line box in
    // boxes is now above the top of the title area, or if step is positive and
    // the bottom of the first line box in boxes is now below the bottom of the
    // title area, jump to the step labeled switch direction.
    LayoutUnit parentHeight = m_cueBox.containingBlock()->size().height();
    if (m_cueWritingDirection == VTTCue::Horizontal && ((step < 0 && top < 0) || (step > 0 && bottom > parentHeight)))
        return true;

    // 21. Vertical: If step is negative and the left edge of the first line
    // box in boxes is now to the left of the left edge of the title area, or
    // if step is positive and the right edge of the first line box in boxes is
    // now to the right of the right edge of the title area, jump to the step
    // labeled switch direction.
    LayoutUnit parentWidth = m_cueBox.containingBlock()->size().width();
    if (m_cueWritingDirection != VTTCue::Horizontal && ((step < 0 && left < 0) || (step > 0 && right > parentWidth)))
        return true;

    return false;
}

void SnapToLinesLayouter::moveBoxesByStep(LayoutUnit step)
{
    // 22. Horizontal: Move all the boxes in boxes down by the distance
    // given by step. (If step is negative, then this will actually
    // result in an upwards movement of the boxes in absolute terms.)
    if (m_cueWritingDirection == VTTCue::Horizontal)
        m_cueBox.setY(m_cueBox.location().y() + step);

    // 22. Vertical: Move all the boxes in boxes right by the distance
    // given by step. (If step is negative, then this will actually
    // result in a leftwards movement of the boxes in absolute terms.)
    else
        m_cueBox.setX(m_cueBox.location().x() + step);
}

bool SnapToLinesLayouter::switchDirection(bool& switched, LayoutUnit& step)
{
    // 24. Switch direction: If switched is true, then move all the boxes in
    // boxes back to their best position, and jump to the step labeled done
    // positioning below.

    // 25. Otherwise, move all the boxes in boxes back to their specified
    // position as determined in the earlier step.
    m_cueBox.setLocation(m_specifiedPosition);

    // XX. If switched is true, jump to the step labeled done
    // positioning below.
    if (switched)
        return false;

    // 26. Negate step.
    step = -step;

    // 27. Set switched to true.
    switched = true;
    return true;
}

void SnapToLinesLayouter::layout()
{
    // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings
    // Step 13, "If cue's text track cue snap-to-lines flag is set".

    InlineFlowBox* firstLineBox;
    if (!findFirstLineBox(firstLineBox))
        return;

    // Step 1 skipped.

    LayoutUnit step;
    LayoutUnit position;
    if (!initializeLayoutParameters(firstLineBox, step, position))
        return;

    bool switched;
    placeBoxInDefaultPosition(position, switched);

    // Step 17 skipped. (margin == 0; title area == video area)

    // 18. Step loop: If none of the boxes in boxes would overlap any of the
    // boxes in output, and all of the boxes in output are entirely within the
    // title area box, then jump to the step labeled done positioning below.
    while (isOutside() || isOverlapping()) {
        // 19. Let current position score be the percentage of the area of the
        // bounding box of the boxes in boxes that is outside the title area
        // box.
        // 20. If best position is null (i.e. this is the first run through
        // this loop, switched is still false, the boxes in boxes are at their
        // specified position, and best position score is still null), or if
        // current position score is a lower percentage than that in best
        // position score, then remember the position of all the boxes in boxes
        // as their best position, and set best position score to current
        // position score.
        if (!shouldSwitchDirection(firstLineBox, step)) {
            // 22. Move all the boxes in boxes ...
            moveBoxesByStep(step);
            // 23. Jump back to the step labeled step loop.
        } else if (!switchDirection(switched, step)) {
            break;
        }

        // 28. Jump back to the step labeled step loop.
    }
}

void RenderVTTCue::repositionCueSnapToLinesNotSet()
{
    // FIXME: Implement overlapping detection when snap-to-lines is not set. http://wkb.ug/84296

    // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings
    // Step 13, "If cue's text track cue snap-to-lines flag is not set".

    // 1. Let bounding box be the bounding box of the boxes in boxes.

    // 2. Run the appropriate steps from the following list:
    //    If the text track cue writing direction is horizontal
    //       If the text track cue line alignment is middle alignment
    //          Move all the boxes in boxes up by half of the height of
    //          bounding box.
    //       If the text track cue line alignment is end alignment
    //          Move all the boxes in boxes up by the height of bounding box.
    //
    //    If the text track cue writing direction is vertical growing left or
    //    vertical growing right
    //       If the text track cue line alignment is middle alignment
    //          Move all the boxes in boxes left by half of the width of
    //          bounding box.
    //       If the text track cue line alignment is end alignment
    //          Move all the boxes in boxes left by the width of bounding box.

    // 3. If none of the boxes in boxes would overlap any of the boxes in
    // output, and all the boxes in output are within the video's rendering
    // area, then jump to the step labeled done positioning below.

    // 4. If there is a position to which the boxes in boxes can be moved while
    // maintaining the relative positions of the boxes in boxes to each other
    // such that none of the boxes in boxes would overlap any of the boxes in
    // output, and all the boxes in output would be within the video's
    // rendering area, then move the boxes in boxes to the closest such
    // position to their current position, and then jump to the step labeled
    // done positioning below. If there are multiple such positions that are
    // equidistant from their current position, use the highest one amongst
    // them; if there are several at that height, then use the leftmost one
    // amongst them.

    // 5. Otherwise, jump to the step labeled done positioning below. (The
    // boxes will unfortunately overlap.)
}

void RenderVTTCue::adjustForTopAndBottomMarginBorderAndPadding()
{
    // Accommodate extra top and bottom padding, border or margin.
    // Note: this is supported only for internal UA styling, not through the cue selector.
    if (!hasInlineDirectionBordersPaddingOrMargin())
        return;
    IntRect containerRect = containingBlock()->absoluteBoundingBoxRect();
    IntRect cueRect = absoluteBoundingBoxRect();

    int topOverflow = cueRect.y() - containerRect.y();
    int bottomOverflow = containerRect.y() + containerRect.height() - cueRect.y() - cueRect.height();

    int adjustment = 0;
    if (topOverflow < 0)
        adjustment = -topOverflow;
    else if (bottomOverflow < 0)
        adjustment = bottomOverflow;

    if (!adjustment)
        return;

    setY(location().y() + adjustment);
}

void RenderVTTCue::layout()
{
    RenderBlockFlow::layout();

    // If WebVTT Regions are used, the regular WebVTT layout algorithm is no
    // longer necessary, since cues having the region parameter set do not have
    // any positioning parameters. Also, in this case, the regions themselves
    // have positioning information.
    if (!m_cue->regionId().isEmpty())
        return;

    LayoutState state(*this, locationOffset());

    // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings - step 13.
    if (m_cue->snapToLines()) {
        SnapToLinesLayouter(*this, m_cue->getWritingDirection(), m_cue->calculateComputedLinePosition()).layout();

        adjustForTopAndBottomMarginBorderAndPadding();
    } else {
        repositionCueSnapToLinesNotSet();
    }
}

} // namespace blink

