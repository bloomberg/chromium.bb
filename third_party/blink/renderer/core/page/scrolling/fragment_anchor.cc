// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/scroll_into_view_options.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

constexpr char kCssFragmentIdentifierPrefix[] = "targetElement=";
constexpr size_t kCssFragmentIdentifierPrefixLength =
    base::size(kCssFragmentIdentifierPrefix);

bool ParseCSSFragmentIdentifier(const String& fragment, String* selector) {
  size_t pos = fragment.Find(kCssFragmentIdentifierPrefix);
  if (pos == 0) {
    *selector = fragment.Substring(kCssFragmentIdentifierPrefixLength - 1);
    return true;
  }

  return false;
}

Element* FindCSSFragmentAnchor(const AtomicString& selector,
                               Document& document) {
  DummyExceptionStateForTesting exception_state;
  return document.QuerySelector(selector, exception_state);
}

Node* FindAnchorFromFragment(const String& fragment, Document& doc) {
  Element* anchor_node;
  String selector;
  if (RuntimeEnabledFeatures::CSSFragmentIdentifiersEnabled() &&
      ParseCSSFragmentIdentifier(fragment, &selector)) {
    anchor_node = FindCSSFragmentAnchor(AtomicString(selector), doc);
  } else {
    anchor_node = doc.FindAnchor(fragment);
  }

  // Implement the rule that "" and "top" both mean top of page as in other
  // browsers.
  if (!anchor_node &&
      (fragment.IsEmpty() || DeprecatedEqualIgnoringCase(fragment, "top")))
    return &doc;

  return anchor_node;
}

}  // namespace

FragmentAnchor* FragmentAnchor::TryCreate(const KURL& url,
                                          Behavior behavior,
                                          LocalFrame& frame) {
  DCHECK(frame.GetDocument());
  Document& doc = *frame.GetDocument();

  // If our URL has no ref, then we have no place we need to jump to.
  // OTOH If CSS target was set previously, we want to set it to 0, recalc
  // and possibly paint invalidation because :target pseudo class may have been
  // set (see bug 11321).
  // Similarly for svg, if we had a previous svgView() then we need to reset
  // the initial view if we don't have a fragment.
  if (!url.HasFragmentIdentifier() && !doc.CssTarget() && !doc.IsSVGDocument())
    return nullptr;

  String fragment = url.FragmentIdentifier();

  Node* anchor_node = nullptr;

  // Try the raw fragment for HTML documents, but skip it for `svgView()`:
  if (!doc.IsSVGDocument())
    anchor_node = FindAnchorFromFragment(fragment, doc);

  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#the-indicated-part-of-the-document
  // 5. Let decodedFragment be the result of running UTF-8 decode without BOM
  // on fragmentBytes.
  if (!anchor_node) {
    fragment = DecodeURLEscapeSequences(fragment, DecodeURLMode::kUTF8);
    anchor_node = FindAnchorFromFragment(fragment, doc);
  }

  // Setting to null will clear the current target.
  Element* target = anchor_node && anchor_node->IsElementNode()
                        ? ToElement(anchor_node)
                        : nullptr;
  doc.SetCSSTarget(target);

  if (doc.IsSVGDocument()) {
    if (SVGSVGElement* svg = ToSVGSVGElementOrNull(doc.documentElement()))
      svg->SetupInitialView(fragment, target);
  }

  if (target)
    target->DispatchActivateInvisibleEventIfNeeded();

  if (frame.GetDocument()->IsSVGDocument() && !frame.IsMainFrame())
    return nullptr;

  if (!anchor_node || behavior == kBehaviorDontScroll)
    return nullptr;

  return MakeGarbageCollected<FragmentAnchor>(*anchor_node, frame);
}

FragmentAnchor::FragmentAnchor(Node& anchor_node, LocalFrame& frame)
    : anchor_node_(&anchor_node),
      frame_(&frame),
      needs_focus_(!anchor_node.IsDocumentNode()) {}

bool FragmentAnchor::Invoke() {
  if (!frame_ || !anchor_node_ || !needs_invoke_)
    return false;

  if (!frame_->GetDocument()->IsRenderingReady())
    return true;

  LayoutRect rect;
  if (anchor_node_ != frame_->GetDocument()) {
    rect = anchor_node_->BoundingBoxForScrollIntoView();
  } else {
    if (Element* document_element = frame_->GetDocument()->documentElement())
      rect = document_element->BoundingBoxForScrollIntoView();
  }

  Frame* boundary_frame = frame_->FindUnsafeParentScrollPropagationBoundary();

  // FIXME: Handle RemoteFrames
  if (boundary_frame && boundary_frame->IsLocalFrame()) {
    ToLocalFrame(boundary_frame)
        ->View()
        ->SetSafeToPropagateScrollToParent(false);
  }

  Element* element_to_scroll = anchor_node_->IsElementNode()
                                   ? ToElement(anchor_node_)
                                   : frame_->GetDocument()->documentElement();
  if (element_to_scroll) {
    ScrollIntoViewOptions* options = ScrollIntoViewOptions::Create();
    options->setBlock("start");
    options->setInlinePosition("nearest");
    element_to_scroll->ScrollIntoViewNoVisualUpdate(options);
  }

  if (boundary_frame && boundary_frame->IsLocalFrame()) {
    ToLocalFrame(boundary_frame)
        ->View()
        ->SetSafeToPropagateScrollToParent(true);
  }

  if (AXObjectCache* cache = frame_->GetDocument()->ExistingAXObjectCache())
    cache->HandleScrolledToAnchor(anchor_node_);

  // If the anchor accepts keyboard focus and fragment scrolling is allowed,
  // move focus there to aid users relying on keyboard navigation.
  // If anchorNode is not focusable or fragment scrolling is not allowed,
  // clear focus, which matches the behavior of other browsers.
  if (needs_focus_) {
    if (anchor_node_->IsElementNode() &&
        ToElement(anchor_node_)->IsFocusable()) {
      ToElement(anchor_node_)->focus();
    } else {
      frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
          anchor_node_);
      frame_->GetDocument()->ClearFocusedElement();
    }
    needs_focus_ = false;
  }

  needs_invoke_ = !frame_->GetDocument()->IsLoadCompleted();
  return needs_invoke_;
}

void FragmentAnchor::DidScroll(ScrollType type) {
  if (!IsExplicitScrollType(type))
    return;

  // If the user/page scrolled, avoid clobbering the scroll offset by removing
  // the anchor on the next invocation. Note: we may get here as a result of
  // calling Invoke() because of the ScrollIntoView but that's ok because
  // needs_invoke_ is recomputed at the end of that method.
  needs_invoke_ = false;
}

void FragmentAnchor::DidCompleteLoad() {
  DCHECK(frame_);
  DCHECK(frame_->View());

  // If there is a pending layout, the fragment anchor will be cleared when it
  // finishes.
  if (!frame_->View()->NeedsLayout())
    needs_invoke_ = false;
}

void FragmentAnchor::Trace(blink::Visitor* visitor) {
  visitor->Trace(anchor_node_);
  visitor->Trace(frame_);
}

}  // namespace blink
