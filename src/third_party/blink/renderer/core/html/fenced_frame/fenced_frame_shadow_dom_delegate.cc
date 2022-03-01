// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_shadow_dom_delegate.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

FencedFrameShadowDOMDelegate::FencedFrameShadowDOMDelegate(
    HTMLFencedFrameElement* outer_element)
    : HTMLFencedFrameElement::FencedFrameDelegate(outer_element) {
  DCHECK_EQ(features::kFencedFramesImplementationTypeParam.Get(),
            features::FencedFramesImplementationType::kShadowDOM);
  GetElement().CreateUserAgentShadowRoot();
}

void FencedFrameShadowDOMDelegate::DidGetInserted() {
  // Only create and append a new internal <iframe> element if it doesn't
  // already exist.
  ShadowRoot* root = GetElement().UserAgentShadowRoot();
  DCHECK(root);
  if (!root->HasChildren())
    AddUserAgentShadowContent(*root);
}

void FencedFrameShadowDOMDelegate::AddUserAgentShadowContent(ShadowRoot& root) {
  DCHECK(!root.HasChildren());

  // Make all children block, to avoid making this box an inline formatting
  // context, and possible baseline alignment kicking in.
  Document& doc = GetElement().GetDocument();
  auto* style =
      MakeGarbageCollected<HTMLStyleElement>(doc, CreateElementFlags());
  style->setTextContent(R"CSS(
iframe {
  display: block;
  box-sizing: border-box;
  width: 100%;
  height: 100%;
  transform-origin: left top;
}
)CSS");
  root.AppendChild(style);

  auto* iframe = MakeGarbageCollected<HTMLIFrameElement>(doc);
  root.AppendChild(iframe);
}

void FencedFrameShadowDOMDelegate::Navigate(const KURL& url) {
  DCHECK(GetElement().UserAgentShadowRoot());

  HTMLIFrameElement* internal_iframe = GetElement().InnerIFrameElement();
  DCHECK(internal_iframe);
  AtomicString url_string(url.GetString());
  internal_iframe->setAttribute(html_names::kSrcAttr, url_string);
}

}  // namespace blink
