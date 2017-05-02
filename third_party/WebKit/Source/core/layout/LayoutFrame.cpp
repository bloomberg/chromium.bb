/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutFrame.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameElement.h"
#include "core/input/EventHandler.h"
#include "core/style/CursorData.h"

namespace blink {

LayoutFrame::LayoutFrame(HTMLFrameElement* frame) : LayoutPart(frame) {
  SetInline(false);
}

FrameEdgeInfo LayoutFrame::EdgeInfo() const {
  HTMLFrameElement* element = toHTMLFrameElement(GetNode());
  return FrameEdgeInfo(element->NoResize(), element->HasFrameBorder());
}

void LayoutFrame::ImageChanged(WrappedImagePtr image, const IntRect*) {
  if (const CursorList* cursors = Style()->Cursors()) {
    for (const CursorData& cursor : *cursors) {
      if (cursor.GetImage() && cursor.GetImage()->CachedImage() == image) {
        if (LocalFrame* frame = this->GetFrame()) {
          // Cursor update scheduling is done by the local root, which is the
          // main frame if there are no RemoteFrame ancestors in the frame tree.
          // Use of localFrameRoot() is discouraged but will change when cursor
          // update scheduling is moved from EventHandler to PageEventHandler.
          frame->LocalFrameRoot().GetEventHandler().ScheduleCursorUpdate();
        }
      }
    }
  }
}

void LayoutFrame::UpdateFromElement() {
  if (Parent() && Parent()->IsFrameSet())
    ToLayoutFrameSet(Parent())->NotifyFrameEdgeInfoChanged();
}

}  // namespace blink
