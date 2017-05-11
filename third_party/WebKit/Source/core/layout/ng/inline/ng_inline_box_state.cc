// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_box_state.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "core/layout/ng/inline/ng_text_fragment_builder.h"
#include "core/style/ComputedStyle.h"

namespace blink {

void NGInlineBoxState::ComputeTextMetrics(const ComputedStyle& style,
                                          FontBaseline baseline_type) {
  text_metrics = NGLineHeightMetrics(style, baseline_type);
  text_top = -text_metrics.ascent;
  text_metrics.AddLeading(style.ComputedLineHeightAsFixed());
  metrics.Unite(text_metrics);

  include_used_fonts = style.LineHeight().IsNegative();
}

void NGInlineBoxState::AccumulateUsedFonts(const NGInlineItem& item,
                                           unsigned start,
                                           unsigned end,
                                           FontBaseline baseline_type) {
  HashSet<const SimpleFontData*> fallback_fonts;
  item.GetFallbackFonts(&fallback_fonts, start, end);
  for (const auto& fallback_font : fallback_fonts) {
    NGLineHeightMetrics fallback_metrics(fallback_font->GetFontMetrics(),
                                         baseline_type);
    fallback_metrics.AddLeading(
        fallback_font->GetFontMetrics().FixedLineSpacing());
    metrics.Unite(fallback_metrics);
  }
}

NGInlineBoxState* NGInlineLayoutStateStack::OnBeginPlaceItems(
    const ComputedStyle* line_style,
    FontBaseline baseline_type) {
  if (stack_.IsEmpty()) {
    // For the first line, push a box state for the line itself.
    stack_.resize(1);
    NGInlineBoxState* box = &stack_.back();
    box->fragment_start = 0;
  } else {
    // For the following lines, clear states that are not shared across lines.
    for (auto& box : stack_) {
      box.fragment_start = 0;
      box.metrics = NGLineHeightMetrics();
      DCHECK(box.pending_descendants.IsEmpty());
    }
  }

  // Initialize the box state for the line box.
  NGInlineBoxState& line_box = LineBoxState();
  line_box.style = line_style;

  // Use a "strut" (a zero-width inline box with the element's font and
  // line height properties) as the initial metrics for the line box.
  // https://drafts.csswg.org/css2/visudet.html#strut
  line_box.ComputeTextMetrics(*line_style, baseline_type);

  return &stack_.back();
}

NGInlineBoxState* NGInlineLayoutStateStack::OnOpenTag(
    const NGInlineItem& item,
    NGLineBoxFragmentBuilder* line_box,
    NGTextFragmentBuilder* text_builder) {
  stack_.resize(stack_.size() + 1);
  NGInlineBoxState* box = &stack_.back();
  box->fragment_start = line_box->Children().size();
  box->style = item.Style();
  text_builder->SetDirection(box->style->Direction());
  return box;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnCloseTag(
    const NGInlineItem& item,
    NGLineBoxFragmentBuilder* line_box,
    NGInlineBoxState* box,
    FontBaseline baseline_type) {
  EndBoxState(box, line_box, baseline_type);
  // TODO(kojii): When the algorithm restarts from a break token, the stack may
  // underflow. We need either synthesize a missing box state, or push all
  // parents on initialize.
  stack_.pop_back();
  return &stack_.back();
}

void NGInlineLayoutStateStack::OnEndPlaceItems(
    NGLineBoxFragmentBuilder* line_box,
    FontBaseline baseline_type) {
  for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
    NGInlineBoxState* box = &(*it);
    EndBoxState(box, line_box, baseline_type);
  }

  DCHECK(!LineBoxState().metrics.IsEmpty());
  line_box->SetMetrics(LineBoxState().metrics);
}

void NGInlineLayoutStateStack::EndBoxState(NGInlineBoxState* box,
                                           NGLineBoxFragmentBuilder* line_box,
                                           FontBaseline baseline_type) {
  PositionPending position_pending =
      ApplyBaselineShift(box, line_box, baseline_type);

  // Unite the metrics to the parent box.
  if (position_pending == kPositionNotPending && box != stack_.begin()) {
    box[-1].metrics.Unite(box->metrics);
  }
}

NGInlineLayoutStateStack::PositionPending
NGInlineLayoutStateStack::ApplyBaselineShift(NGInlineBoxState* box,
                                             NGLineBoxFragmentBuilder* line_box,
                                             FontBaseline baseline_type) {
  // Compute descendants that depend on the layout size of this box if any.
  LayoutUnit baseline_shift;
  if (!box->pending_descendants.IsEmpty()) {
    for (auto& child : box->pending_descendants) {
      switch (child.vertical_align) {
        case EVerticalAlign::kTextTop:
          DCHECK(!box->text_metrics.IsEmpty());
          baseline_shift = child.metrics.ascent + box->text_top;
          break;
        case EVerticalAlign::kTop:
          baseline_shift = child.metrics.ascent - box->metrics.ascent;
          break;
        case EVerticalAlign::kTextBottom:
          if (const SimpleFontData* font_data =
                  box->style->GetFont().PrimaryFont()) {
            LayoutUnit text_bottom =
                font_data->GetFontMetrics().FixedDescent(baseline_type);
            baseline_shift = text_bottom - child.metrics.descent;
            break;
          }
          NOTREACHED();
        // Fall through.
        case EVerticalAlign::kBottom:
          baseline_shift = box->metrics.descent - child.metrics.descent;
          break;
        default:
          NOTREACHED();
          continue;
      }
      child.metrics.Move(baseline_shift);
      box->metrics.Unite(child.metrics);
      line_box->MoveChildrenInBlockDirection(
          baseline_shift, child.fragment_start, child.fragment_end);
    }
    box->pending_descendants.clear();
  }

  const ComputedStyle& style = *box->style;
  EVerticalAlign vertical_align = style.VerticalAlign();
  if (vertical_align == EVerticalAlign::kBaseline)
    return kPositionNotPending;

  // 'vertical-align' aplies only to inline-level elements.
  if (box == stack_.begin())
    return kPositionNotPending;

  // Check if there are any fragments to move.
  unsigned fragment_end = line_box->Children().size();
  if (box->fragment_start == fragment_end)
    return kPositionNotPending;

  switch (vertical_align) {
    case EVerticalAlign::kSub:
      baseline_shift = style.ComputedFontSizeAsFixed() / 5 + 1;
      break;
    case EVerticalAlign::kSuper:
      baseline_shift = -(style.ComputedFontSizeAsFixed() / 3 + 1);
      break;
    case EVerticalAlign::kLength: {
      // 'Percentages: refer to the 'line-height' of the element itself'.
      // https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
      const Length& length = style.GetVerticalAlignLength();
      LayoutUnit line_height = length.IsPercentOrCalc()
                                   ? style.ComputedLineHeightAsFixed()
                                   : box->text_metrics.LineHeight();
      baseline_shift = -ValueForLength(length, line_height);
      break;
    }
    case EVerticalAlign::kMiddle:
      baseline_shift = (box->metrics.ascent - box->metrics.descent) / 2;
      if (const SimpleFontData* font_data = style.GetFont().PrimaryFont()) {
        baseline_shift -= LayoutUnit::FromFloatRound(
            font_data->GetFontMetrics().XHeight() / 2);
      }
      break;
    case EVerticalAlign::kBaselineMiddle:
      baseline_shift = (box->metrics.ascent - box->metrics.descent) / 2;
      break;
    case EVerticalAlign::kTop:
    case EVerticalAlign::kBottom:
      // 'top' and 'bottom' require the layout size of the line box.
      stack_.front().pending_descendants.push_back(NGPendingPositions{
          box->fragment_start, fragment_end, box->metrics, vertical_align});
      return kPositionPending;
    default:
      // Other values require the layout size of the parent box.
      SECURITY_CHECK(box != stack_.begin());
      box[-1].pending_descendants.push_back(NGPendingPositions{
          box->fragment_start, fragment_end, box->metrics, vertical_align});
      return kPositionPending;
  }
  box->metrics.Move(baseline_shift);
  line_box->MoveChildrenInBlockDirection(baseline_shift, box->fragment_start,
                                         fragment_end);
  return kPositionNotPending;
}

}  // namespace blink
