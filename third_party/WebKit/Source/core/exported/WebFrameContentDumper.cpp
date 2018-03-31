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
#include "core/html_element_type_helpers.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/LayoutView.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebDocument.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebView.h"

namespace blink {

namespace {

const int text_dumper_max_depth = 512;

bool IsRenderedAndVisible(const Node& node) {
  if (node.GetLayoutObject() &&
      node.GetLayoutObject()->Style()->Visibility() == EVisibility::kVisible)
    return true;
  if (node.IsElementNode() && ToElement(node).HasDisplayContentsStyle())
    return true;
  return false;
}

size_t RequiredLineBreaksAround(const Node& node) {
  if (!IsRenderedAndVisible(node))
    return 0;
  if (node.IsTextNode())
    return 0;
  if (IsHTMLParagraphElement(node))
    return 2;
  if (LayoutObject* layout_object = node.GetLayoutObject()) {
    if (!layout_object->Style()->IsDisplayInlineType())
      return 1;
    if (layout_object->Style()->Display() == EDisplay::kTableCaption)
      return 1;
  }
  return 0;
}

// This class dumps innerText of a node into a StringBuilder, following the spec
// [*] but with a simplified whitespace handling algorithm when processing text
// nodes: only leading and trailing collapsed whitespaces are removed; all other
// whitespace characters are left as-is, without any collapsing or conversion.
// For example, from HTML <p>\na\n\nb\n</p>, we get text dump "a\n\nb".
// [*] https://developer.mozilla.org/en-US/docs/Web/API/Node/innerText
class InnerTextDumper final {
  STACK_ALLOCATED();

 public:
  InnerTextDumper(StringBuilder& builder, size_t max_length)
      : builder_(builder), max_length_(max_length) {}

  void DumpTextFrom(const Node& node) {
    DCHECK(!has_emitted_);
    DCHECK(!required_line_breaks_);
    HandleNode(node, 0);
  }

 private:
  void HandleNode(const Node& node, int depth) {
    const size_t required_line_breaks_around = RequiredLineBreaksAround(node);
    AddRequiredLineBreaks(required_line_breaks_around);

    if (depth < text_dumper_max_depth) {
      for (const Node& child : NodeTraversal::ChildrenOf(node)) {
        HandleNode(child, depth + 1);
        if (builder_.length() >= max_length_)
          return;
      }
    }

    if (!IsRenderedAndVisible(node))
      return;

    if (node.IsTextNode())
      return HandleTextNode(ToText(node));

    if (IsHTMLBRElement(node))
      return DumpText("\n");

    if (LayoutObject* layout_object = node.GetLayoutObject()) {
      if (layout_object->IsTableCell() &&
          ToLayoutTableCell(layout_object)->NextCell())
        return DumpText("\t");
      if (layout_object->IsTableRow() &&
          ToLayoutTableRow(layout_object)->NextRow())
        return DumpText("\n");
    }

    AddRequiredLineBreaks(required_line_breaks_around);
  }

  void HandleTextNode(const Text& node) {
    const LayoutText* layout_text = node.GetLayoutObject();
    if (!layout_text)
      return;
    if (layout_text->IsTextFragment() &&
        ToLayoutTextFragment(layout_text)->IsRemainingTextLayoutObject()) {
      const LayoutText* first_letter =
          ToLayoutText(AssociatedLayoutObjectOf(node, 0));
      if (first_letter && first_letter != layout_text)
        HandleLayoutText(*first_letter);
    }
    HandleLayoutText(*layout_text);
  }

  void HandleLayoutText(const LayoutText& text) {
    if (!text.HasNonCollapsedText())
      return;
    size_t text_start = text.CaretMinOffset();
    size_t text_end = text.CaretMaxOffset();
    String dump = text.GetText().Substring(text_start, text_end - text_start);
    DumpText(dump);
  }

  void AddRequiredLineBreaks(size_t required) {
    required_line_breaks_ = std::max(required, required_line_breaks_);
  }

  void DumpText(String text) {
    if (!text.length())
      return;

    if (has_emitted_ && required_line_breaks_) {
      for (size_t i = 0; i < required_line_breaks_; ++i)
        builder_.Append('\n');
    }
    required_line_breaks_ = 0;
    builder_.Append(text);
    has_emitted_ = true;

    if (builder_.length() > max_length_)
      builder_.Resize(max_length_);
  }

  bool has_emitted_ = false;
  size_t required_line_breaks_ = 0;

  StringBuilder& builder_;
  const size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(InnerTextDumper);
};

bool TextContentDumperIgnoresElement(const Element& element) {
  return IsHTMLStyleElement(element) || IsHTMLScriptElement(element) ||
         IsHTMLNoScriptElement(element);
}

bool IsWhiteSpace(UChar ch) {
  return ch == ' ' || ch == '\n' || ch == '\t';
}

// This class dumps textContent of a node into a StringBuilder, with the minor
// exception that text nodes in certain elements are ignored (See
// TextContentDumperIgnoresElement()), and consucetive whitespace characters are
// collapsed regardless of style.
// Note: This dumper is for TranslateHelper only. Do not use for other purposes!
class TextContentDumper {
  STACK_ALLOCATED();

 public:
  TextContentDumper(StringBuilder& builder, size_t max_length)
      : builder_(builder), max_length_(max_length) {}

  void DumpTextFrom(const Element& element) { HandleElement(element, 0); }

 private:
  void HandleElement(const Element& element, unsigned depth) {
    if (depth == text_dumper_max_depth)
      return;
    if (TextContentDumperIgnoresElement(element))
      return;

    for (const Node& child : NodeTraversal::ChildrenOf(element)) {
      if (child.IsElementNode()) {
        HandleElement(ToElement(child), depth + 1);
        continue;
      }

      if (!child.IsTextNode())
        continue;

      HandleTextNode(ToText(child));
      if (builder_.length() >= max_length_)
        return;
    }
  }

  void HandleTextNode(const Text& node) {
    for (unsigned i = 0;
         i < node.data().length() && builder_.length() < max_length_; ++i) {
      UChar ch = node.data()[i];
      if (ShouldAppendCharacter(ch))
        builder_.Append(ch);
    }
  }

  bool ShouldAppendCharacter(UChar ch) const {
    if (!IsWhiteSpace(ch))
      return true;
    if (builder_.IsEmpty())
      return true;
    if (!IsWhiteSpace(builder_[builder_.length() - 1]))
      return true;
    return false;
  }

  StringBuilder& builder_;
  const size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(TextContentDumper);
};

// Controls which text dumper to use: TextContentDumper or InnerTextDumper.
enum TextDumpOption { kDumpTextContent, kDumpInnerText };

void FrameContentAsPlainText(size_t max_chars,
                             LocalFrame* frame,
                             TextDumpOption option,
                             StringBuilder& output) {
  Document* document = frame->GetDocument();
  if (!document)
    return;

  if (!frame->View() || frame->View()->ShouldThrottleRendering())
    return;

  if (option == TextDumpOption::kDumpInnerText) {
    // Dumping inner text requires clean layout
    DCHECK(!frame->View()->NeedsLayout());
    DCHECK(!document->NeedsLayoutTreeUpdate());
  }

  if (document->documentElement()) {
    if (option == TextDumpOption::kDumpInnerText) {
      InnerTextDumper(output, max_chars)
          .DumpTextFrom(*document->documentElement());
    } else {
      TextContentDumper(output, max_chars)
          .DumpTextFrom(*document->documentElement());
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
    // When dumping inner text, ignore the text of non-visible frames.
    if (option == TextDumpOption::kDumpInnerText) {
      LayoutView* layout_view = cur_local_child->ContentLayoutObject();
      LayoutObject* owner_layout_object = cur_local_child->OwnerLayoutObject();
      if (!layout_view || !layout_view->Size().Width() ||
          !layout_view->Size().Height() ||
          (layout_view->Location().X() + layout_view->Size().Width() <= 0) ||
          (layout_view->Location().Y() + layout_view->Size().Height() <= 0) ||
          (owner_layout_object && owner_layout_object->Style() &&
           owner_layout_object->Style()->Visibility() !=
               EVisibility::kVisible)) {
        continue;
      }
    }

    // Make sure the frame separator won't fill up the buffer, and give up if
    // it will. The danger is if the separator will make the buffer longer than
    // maxChars. This will cause the computation above:
    //   maxChars - output->size()
    // to be a negative number which will crash when the subframe is added.
    if (output.length() >= max_chars - frame_separator_length)
      return;

    output.Append(kFrameSeparator, frame_separator_length);
    FrameContentAsPlainText(max_chars, cur_local_child, option, output);
    if (output.length() >= max_chars)
      return;  // Filled up the buffer.
  }
}

}  // namespace

WebString WebFrameContentDumper::DeprecatedDumpFrameTreeAsText(
    WebLocalFrame* frame,
    size_t max_chars) {
  if (!frame)
    return WebString();
  LocalFrame* local_frame = ToWebLocalFrameImpl(frame)->GetFrame();
  // TODO(crbug.com/586241): We shoulndn't reach here with dirty layout as the
  // function is called in DidMeaningfulLayout(); However, we still see this in
  // the wild. As a workaround, we dump |textContent| instead when layout is
  // dirty. We should always dump |innerText| when the bug is fixed.
  bool needs_layout = local_frame->View()->NeedsLayout() ||
                      local_frame->GetDocument()->NeedsLayoutTreeUpdate();
  StringBuilder text;
  FrameContentAsPlainText(max_chars, local_frame,
                          needs_layout ? TextDumpOption::kDumpTextContent
                                       : TextDumpOption::kDumpInnerText,
                          text);
  return text.ToString();
}

WebString WebFrameContentDumper::DumpWebViewAsText(WebView* web_view,
                                                   size_t max_chars) {
  DCHECK(web_view);
  WebLocalFrame* frame = web_view->MainFrame()->ToWebLocalFrame();
  if (!frame)
    return WebString();

  web_view->UpdateAllLifecyclePhases();

  StringBuilder text;
  FrameContentAsPlainText(max_chars, ToWebLocalFrameImpl(frame)->GetFrame(),
                          TextDumpOption::kDumpInnerText, text);
  return text.ToString();
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
