/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LineBoxList_h
#define LineBoxList_h

#include "core/layout/api/HitTestAction.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"

namespace blink {

class CullRect;
class HitTestLocation;
class HitTestResult;
class InlineFlowBox;
class LayoutPoint;
class LayoutUnit;
class LineLayoutBoxModel;
class LineLayoutItem;

class LineBoxList {
  DISALLOW_NEW();

 public:
  LineBoxList() : first_line_box_(nullptr), last_line_box_(nullptr) {}

#if DCHECK_IS_ON()
  ~LineBoxList();
#endif

  InlineFlowBox* FirstLineBox() const { return first_line_box_; }
  InlineFlowBox* LastLineBox() const { return last_line_box_; }

  void AppendLineBox(InlineFlowBox*);

  void DeleteLineBoxTree();
  void DeleteLineBoxes();

  void ExtractLineBox(InlineFlowBox*);
  void AttachLineBox(InlineFlowBox*);
  void RemoveLineBox(InlineFlowBox*);

  void DirtyLineBoxes();
  void DirtyLinesFromChangedChild(LineLayoutItem parent,
                                  LineLayoutItem child,
                                  bool can_dirty_ancestors);

  bool HitTest(LineLayoutBoxModel,
               HitTestResult&,
               const HitTestLocation& location_in_container,
               const LayoutPoint& accumulated_offset,
               HitTestAction) const;
  bool AnyLineIntersectsRect(LineLayoutBoxModel,
                             const CullRect&,
                             const LayoutPoint&) const;
  bool LineIntersectsDirtyRect(LineLayoutBoxModel,
                               InlineFlowBox*,
                               const CullRect&,
                               const LayoutPoint&) const;

 private:
  bool RangeIntersectsRect(LineLayoutBoxModel,
                           LayoutUnit logical_top,
                           LayoutUnit logical_bottom,
                           const CullRect&,
                           const LayoutPoint&) const;

  // For block flows, each box represents the root inline box for a line in the
  // paragraph.
  // For inline flows, each box represents a portion of that inline.
  InlineFlowBox* first_line_box_;
  InlineFlowBox* last_line_box_;
};

}  // namespace blink

#endif  // LineBoxList_h
