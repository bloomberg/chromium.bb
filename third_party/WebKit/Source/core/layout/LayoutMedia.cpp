/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/layout/LayoutMedia.h"

#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/media/MediaControls.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"

namespace blink {

LayoutMedia::LayoutMedia(HTMLMediaElement* video) : LayoutImage(video) {
  SetImageResource(LayoutImageResource::Create());
}

LayoutMedia::~LayoutMedia() {}

HTMLMediaElement* LayoutMedia::MediaElement() const {
  return ToHTMLMediaElement(GetNode());
}

void LayoutMedia::GetLayout() {
  LayoutSize old_size = ContentBoxRect().size();

  LayoutImage::GetLayout();

  LayoutRect new_rect = ContentBoxRect();

  LayoutState state(*this);

// Iterate the children in reverse order so that the media controls are laid
// out before the text track container. This is to ensure that the text
// track rendering has an up-to-date position of the media controls for
// overlap checking, see LayoutVTTCue.
#if DCHECK_IS_ON()
  bool seen_text_track_container = false;
#endif
  for (LayoutObject* child = children_.LastChild(); child;
       child = child->PreviousSibling()) {
#if DCHECK_IS_ON()
    if (child->GetNode()->IsMediaControls())
      DCHECK(!seen_text_track_container);
    else if (child->GetNode()->IsTextTrackContainer())
      seen_text_track_container = true;
    else
      NOTREACHED();
#endif

    // TODO(mlamouri): we miss some layouts because needsLayout returns false in
    // some cases where we want to change the width of the controls because the
    // visible viewport has changed for example.
    if (new_rect.size() == old_size && !child->NeedsLayout())
      continue;

    LayoutUnit width = new_rect.Width();
    if (child->GetNode()->IsMediaControls()) {
      width = ComputePanelWidth(new_rect);
    }

    LayoutBox* layout_box = ToLayoutBox(child);
    layout_box->SetLocation(new_rect.Location());
    // TODO(foolip): Remove the mutableStyleRef() and depend on CSS
    // width/height: inherit to match the media element size.
    layout_box->MutableStyleRef().SetHeight(Length(new_rect.Height(), kFixed));
    layout_box->MutableStyleRef().SetWidth(Length(width, kFixed));

    layout_box->ForceLayout();
  }

  ClearNeedsLayout();
}

bool LayoutMedia::IsChildAllowed(LayoutObject* child,
                                 const ComputedStyle& style) const {
  // Two types of child layout objects are allowed: media controls
  // and the text track container. Filter children by node type.
  DCHECK(child->GetNode());

  // Out-of-flow positioned or floating child breaks layout hierarchy.
  // This check can be removed if ::-webkit-media-controls is made internal.
  if (style.HasOutOfFlowPosition() || style.IsFloating())
    return false;

  // The user agent stylesheet (mediaControls.css) has
  // ::-webkit-media-controls { display: flex; }. If author style
  // sets display: inline we would get an inline layoutObject as a child
  // of replaced content, which is not supposed to be possible. This
  // check can be removed if ::-webkit-media-controls is made
  // internal.
  if (child->GetNode()->IsMediaControls())
    return child->IsFlexibleBox();

  if (child->GetNode()->IsTextTrackContainer())
    return true;

  return false;
}

void LayoutMedia::PaintReplaced(const PaintInfo&, const LayoutPoint&) const {}

LayoutUnit LayoutMedia::ComputePanelWidth(const LayoutRect& media_rect) const {
  // TODO(mlamouri): we don't know if the main frame has an horizontal scrollbar
  // if it is out of process. See https://crbug.com/662480
  if (GetDocument().GetPage()->MainFrame()->IsRemoteFrame())
    return media_rect.Width();

  // TODO(foolip): when going fullscreen, the animation sometimes does not clear
  // up properly and the last `absoluteXOffset` received is incorrect. This is
  // a shortcut that we could ideally avoid. See https://crbug.com/663680
  if (MediaElement() && MediaElement()->IsFullscreen())
    return media_rect.Width();

  Page* page = GetDocument().GetPage();
  LocalFrame* main_frame = page->DeprecatedLocalMainFrame();
  FrameView* page_view = main_frame ? main_frame->View() : nullptr;
  if (!main_frame || !page_view)
    return media_rect.Width();

  if (page_view->HorizontalScrollbarMode() != kScrollbarAlwaysOff)
    return media_rect.Width();

  // On desktop, this will include scrollbars when they stay visible.
  const LayoutUnit visible_width(page->GetVisualViewport().VisibleWidth());
  const LayoutUnit absolute_x_offset(
      LocalToAbsolute(
          FloatPoint(media_rect.Location()),
          kUseTransforms | kApplyContainerFlip | kTraverseDocumentBoundaries)
          .X());
  const LayoutUnit new_width = visible_width - absolute_x_offset;

  if (new_width < 0)
    return media_rect.Width();

  return std::min(media_rect.Width(), visible_width - absolute_x_offset);
}

}  // namespace blink
