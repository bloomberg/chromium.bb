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

#ifndef WebElement_h
#define WebElement_h

#include "public/platform/WebImage.h"
#include "WebNode.h"

namespace blink {

class Element;
struct WebRect;

// Provides access to some properties of a DOM element node.
class WebElement : public WebNode {
 public:
  WebElement() : WebNode() {}
  WebElement(const WebElement& e) : WebNode(e) {}

  WebElement& operator=(const WebElement& e) {
    WebNode::Assign(e);
    return *this;
  }
  void Assign(const WebElement& e) { WebNode::Assign(e); }

  BLINK_EXPORT bool IsFormControlElement() const;
  // If the element is editable, for example by being contenteditable or being
  // an <input> that isn't readonly or disabled.
  BLINK_EXPORT bool IsEditable() const;
  // Returns the qualified name, which may contain a prefix and a colon.
  BLINK_EXPORT WebString TagName() const;
  // Check if this element has the specified local tag name, and the HTML
  // namespace. Tag name matching is case-insensitive.
  BLINK_EXPORT bool HasHTMLTagName(const WebString&) const;
  BLINK_EXPORT bool HasAttribute(const WebString&) const;
  BLINK_EXPORT WebString GetAttribute(const WebString&) const;
  BLINK_EXPORT void SetAttribute(const WebString& name, const WebString& value);
  BLINK_EXPORT WebString TextContent() const;
  BLINK_EXPORT WebString InnerHTML() const;
  BLINK_EXPORT WebString AttributeLocalName(unsigned index) const;
  BLINK_EXPORT WebString AttributeValue(unsigned index) const;
  BLINK_EXPORT unsigned AttributeCount() const;

  // If this element takes up space in the layout of the page.
  BLINK_EXPORT bool HasNonEmptyLayoutSize() const;

  // Returns the bounds of the element in Visual Viewport. The bounds
  // have been adjusted to include any transformations, including page scale.
  // This function will update the layout if required.
  BLINK_EXPORT WebRect BoundsInViewport() const;

  // Returns the image contents of this element or a null WebImage
  // if there isn't any.
  BLINK_EXPORT WebImage ImageContents();

#if BLINK_IMPLEMENTATION
  BLINK_EXPORT WebElement(Element*);
  BLINK_EXPORT WebElement& operator=(Element*);
  BLINK_EXPORT operator Element*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebElement);

}  // namespace blink

#endif
