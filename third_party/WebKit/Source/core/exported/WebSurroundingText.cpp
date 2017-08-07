/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "public/web/WebSurroundingText.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SurroundingText.h"
#include "core/editing/VisiblePosition.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/layout/LayoutObject.h"
#include "public/platform/WebPoint.h"
#include "public/web/WebHitTestResult.h"

namespace blink {

WebSurroundingText::WebSurroundingText() {}

WebSurroundingText::~WebSurroundingText() {}

void WebSurroundingText::Initialize(const WebNode& web_node,
                                    const WebPoint& node_point,
                                    size_t max_length) {
  const Node* node = web_node.ConstUnwrap<Node>();
  if (!node)
    return;

  // VisiblePosition and SurroundingText must be created with clean layout.
  node->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      node->GetDocument().Lifecycle());

  if (!node->GetLayoutObject())
    return;

  // TODO(xiaochengh): The followinng SurroundingText can hold a null Range,
  // in which case we should prevent it from being stored in |m_private|.
  private_.reset(new SurroundingText(
      CreateVisiblePosition(node->GetLayoutObject()->PositionForPoint(
                                static_cast<IntPoint>(node_point)))
          .DeepEquivalent()
          .ParentAnchoredEquivalent(),
      max_length));
}

void WebSurroundingText::InitializeFromCurrentSelection(WebLocalFrame* frame,
                                                        size_t max_length) {
  LocalFrame* web_frame = ToWebLocalFrameImpl(frame)->GetFrame();

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  web_frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (Range* range = CreateRange(web_frame->Selection()
                                     .ComputeVisibleSelectionInDOMTree()
                                     .ToNormalizedEphemeralRange())) {
    // TODO(xiaochengh): The followinng SurroundingText can hold a null Range,
    // in which case we should prevent it from being stored in |m_private|.
    private_.reset(new SurroundingText(*range, max_length));
  }
}

WebString WebSurroundingText::TextContent() const {
  return private_->Content();
}

size_t WebSurroundingText::HitOffsetInTextContent() const {
  DCHECK_EQ(private_->StartOffsetInContent(), private_->EndOffsetInContent());
  return private_->StartOffsetInContent();
}

size_t WebSurroundingText::StartOffsetInTextContent() const {
  return private_->StartOffsetInContent();
}

size_t WebSurroundingText::EndOffsetInTextContent() const {
  return private_->EndOffsetInContent();
}

bool WebSurroundingText::IsNull() const {
  return !private_.get();
}

}  // namespace blink
