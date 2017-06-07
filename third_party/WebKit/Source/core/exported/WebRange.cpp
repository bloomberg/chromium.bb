/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebRange.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PlainTextRange.h"
#include "core/frame/LocalFrame.h"

namespace blink {

WebRange::WebRange(int start, int length)
    : start_(start), end_(start + length) {
  DCHECK(start != -1 || length != 0)
      << "These values are reserved to indicate that the range is null";
}

WebRange::WebRange(const EphemeralRange& range) {
  if (range.IsNull())
    return;

  start_ = range.StartPosition().ComputeOffsetInContainerNode();
  end_ = range.EndPosition().ComputeOffsetInContainerNode();
}

WebRange::WebRange(const PlainTextRange& range) {
  if (range.IsNull())
    return;

  start_ = range.Start();
  end_ = range.End();
}

EphemeralRange WebRange::CreateEphemeralRange(LocalFrame* frame) const {
  Element* selection_root = frame->Selection()
                                .ComputeVisibleSelectionInDOMTreeDeprecated()
                                .RootEditableElement();
  ContainerNode* scope =
      selection_root ? selection_root : frame->GetDocument()->documentElement();

  return PlainTextRange(start_, end_).CreateRange(*scope);
}

}  // namespace blink
