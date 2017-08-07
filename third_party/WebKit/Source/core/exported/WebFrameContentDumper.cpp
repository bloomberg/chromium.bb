// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebFrameContentDumper.h"

#include "core/editing/EphemeralRange.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutEmbeddedContentItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebDocument.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebView.h"

namespace blink {

static void FrameContentAsPlainText(size_t max_chars,
                                    LocalFrame* frame,
                                    StringBuilder& output) {
  Document* document = frame->GetDocument();
  if (!document)
    return;

  if (!frame->View() || frame->View()->ShouldThrottleRendering())
    return;

  DCHECK(!frame->View()->NeedsLayout());
  DCHECK(!document->NeedsLayoutTreeUpdate());

  // Select the document body.
  if (document->body()) {
    const EphemeralRange range =
        EphemeralRange::RangeOfContents(*document->body());

    // The text iterator will walk nodes giving us text. This is similar to
    // the plainText() function in core/editing/TextIterator.h, but we
    // implement the maximum size and also copy the results directly into a
    // wstring, avoiding the string conversion.
    for (TextIterator it(range.StartPosition(), range.EndPosition());
         !it.AtEnd(); it.Advance()) {
      it.GetText().AppendTextToStringBuilder(output, 0,
                                             max_chars - output.length());
      if (output.length() >= max_chars)
        return;  // Filled up the buffer.
    }
  }

  // The separator between frames when the frames are converted to plain text.
  const LChar kFrameSeparator[] = {'\n', '\n'};
  const size_t frame_separator_length = WTF_ARRAY_LENGTH(kFrameSeparator);

  // Recursively walk the children.
  const FrameTree& frame_tree = frame->Tree();
  for (Frame* cur_child = frame_tree.FirstChild(); cur_child;
       cur_child = cur_child->Tree().NextSibling()) {
    if (!cur_child->IsLocalFrame())
      continue;
    LocalFrame* cur_local_child = ToLocalFrame(cur_child);
    // Ignore the text of non-visible frames.
    LayoutViewItem content_layout_item = cur_local_child->ContentLayoutItem();
    LayoutEmbeddedContentItem owner_layout_item =
        cur_local_child->OwnerLayoutItem();
    if (content_layout_item.IsNull() || !content_layout_item.Size().Width() ||
        !content_layout_item.Size().Height() ||
        (content_layout_item.Location().X() +
             content_layout_item.Size().Width() <=
         0) ||
        (content_layout_item.Location().Y() +
             content_layout_item.Size().Height() <=
         0) ||
        (!owner_layout_item.IsNull() && owner_layout_item.Style() &&
         owner_layout_item.Style()->Visibility() != EVisibility::kVisible)) {
      continue;
    }

    // Make sure the frame separator won't fill up the buffer, and give up if
    // it will. The danger is if the separator will make the buffer longer than
    // maxChars. This will cause the computation above:
    //   maxChars - output->size()
    // to be a negative number which will crash when the subframe is added.
    if (output.length() >= max_chars - frame_separator_length)
      return;

    output.Append(kFrameSeparator, frame_separator_length);
    FrameContentAsPlainText(max_chars, cur_local_child, output);
    if (output.length() >= max_chars)
      return;  // Filled up the buffer.
  }
}

WebString WebFrameContentDumper::DeprecatedDumpFrameTreeAsText(
    WebLocalFrame* frame,
    size_t max_chars) {
  if (!frame)
    return WebString();
  StringBuilder text;
  FrameContentAsPlainText(max_chars, ToWebLocalFrameImpl(frame)->GetFrame(),
                          text);
  return text.ToString();
}

WebString WebFrameContentDumper::DumpWebViewAsText(WebView* web_view,
                                                   size_t max_chars) {
  DCHECK(web_view);
  web_view->UpdateAllLifecyclePhases();
  return WebFrameContentDumper::DeprecatedDumpFrameTreeAsText(
      web_view->MainFrame()->ToWebLocalFrame(), max_chars);
}

WebString WebFrameContentDumper::DumpAsMarkup(WebLocalFrame* frame) {
  if (!frame)
    return WebString();
  return CreateMarkup(ToWebLocalFrameImpl(frame)->GetFrame()->GetDocument());
}

WebString WebFrameContentDumper::DumpLayoutTreeAsText(
    WebLocalFrame* frame,
    LayoutAsTextControls to_show) {
  if (!frame)
    return WebString();
  LayoutAsTextBehavior behavior = kLayoutAsTextShowAllLayers;

  if (to_show & kLayoutAsTextWithLineTrees)
    behavior |= kLayoutAsTextShowLineTrees;

  if (to_show & kLayoutAsTextDebug) {
    behavior |= kLayoutAsTextShowCompositedLayers | kLayoutAsTextShowAddresses |
                kLayoutAsTextShowIDAndClass | kLayoutAsTextShowLayerNesting;
  }

  if (to_show & kLayoutAsTextPrinting)
    behavior |= kLayoutAsTextPrintingMode;

  return ExternalRepresentation(ToWebLocalFrameImpl(frame)->GetFrame(),
                                behavior);
}
}
