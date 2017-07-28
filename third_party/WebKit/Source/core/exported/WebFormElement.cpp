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

#include "public/web/WebFormElement.h"

#include "core/HTMLNames.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebFormControlElement.h"
#include "public/web/WebInputElement.h"

namespace blink {

bool WebFormElement::AutoComplete() const {
  return ConstUnwrap<HTMLFormElement>()->ShouldAutocomplete();
}

WebString WebFormElement::Action() const {
  return ConstUnwrap<HTMLFormElement>()->Action();
}

WebString WebFormElement::GetName() const {
  return ConstUnwrap<HTMLFormElement>()->GetName();
}

WebString WebFormElement::Method() const {
  return ConstUnwrap<HTMLFormElement>()->method();
}

void WebFormElement::GetFormControlElements(
    WebVector<WebFormControlElement>& result) const {
  const HTMLFormElement* form = ConstUnwrap<HTMLFormElement>();
  Vector<WebFormControlElement> form_control_elements;

  const ListedElement::List& listed_elements = form->ListedElements();
  for (ListedElement::List::const_iterator it = listed_elements.begin();
       it != listed_elements.end(); ++it) {
    if ((*it)->IsFormControlElement())
      form_control_elements.push_back(ToHTMLFormControlElement(*it));
  }
  result.Assign(form_control_elements);
}

WebFormElement::WebFormElement(HTMLFormElement* e) : WebElement(e) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebFormElement,
                           isHTMLFormElement(ConstUnwrap<Node>()));

WebFormElement& WebFormElement::operator=(HTMLFormElement* e) {
  private_ = e;
  return *this;
}

WebFormElement::operator HTMLFormElement*() const {
  return toHTMLFormElement(private_.Get());
}

}  // namespace blink
