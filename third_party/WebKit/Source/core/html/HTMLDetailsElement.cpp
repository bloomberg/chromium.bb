/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/html/HTMLDetailsElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/Text.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSummaryElement.h"
#include "core/html/shadow/DetailsMarkerControl.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutBlockFlow.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

class FirstSummarySelectFilter final : public HTMLContentSelectFilter {
 public:
  virtual ~FirstSummarySelectFilter() {}

  static FirstSummarySelectFilter* Create() {
    return new FirstSummarySelectFilter();
  }

  bool CanSelectNode(const HeapVector<Member<Node>, 32>& siblings,
                     int nth) const override {
    if (!siblings[nth]->HasTagName(HTMLNames::summaryTag))
      return false;
    for (int i = nth - 1; i >= 0; --i) {
      if (siblings[i]->HasTagName(HTMLNames::summaryTag))
        return false;
    }
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { HTMLContentSelectFilter::Trace(visitor); }

 private:
  FirstSummarySelectFilter() {}
};

HTMLDetailsElement* HTMLDetailsElement::Create(Document& document) {
  HTMLDetailsElement* details = new HTMLDetailsElement(document);
  details->EnsureUserAgentShadowRoot();
  return details;
}

HTMLDetailsElement::HTMLDetailsElement(Document& document)
    : HTMLElement(detailsTag, document), is_open_(false) {
  UseCounter::Count(document, WebFeature::kDetailsElement);
}

HTMLDetailsElement::~HTMLDetailsElement() {}

void HTMLDetailsElement::DispatchPendingEvent() {
  DispatchEvent(Event::Create(EventTypeNames::toggle));
}

LayoutObject* HTMLDetailsElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutBlockFlow(this);
}

void HTMLDetailsElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  HTMLSummaryElement* default_summary =
      HTMLSummaryElement::Create(GetDocument());
  default_summary->AppendChild(
      Text::Create(GetDocument(),
                   GetLocale().QueryString(WebLocalizedString::kDetailsLabel)));

  HTMLContentElement* summary = HTMLContentElement::Create(
      GetDocument(), FirstSummarySelectFilter::Create());
  summary->SetIdAttribute(ShadowElementNames::DetailsSummary());
  summary->AppendChild(default_summary);
  root.AppendChild(summary);

  HTMLDivElement* content = HTMLDivElement::Create(GetDocument());
  content->SetIdAttribute(ShadowElementNames::DetailsContent());
  content->AppendChild(HTMLContentElement::Create(GetDocument()));
  content->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  root.AppendChild(content);
}

Element* HTMLDetailsElement::FindMainSummary() const {
  if (HTMLSummaryElement* summary =
          Traversal<HTMLSummaryElement>::FirstChild(*this))
    return summary;

  HTMLContentElement* content =
      toHTMLContentElementOrDie(UserAgentShadowRoot()->firstChild());
  DCHECK(content->firstChild());
  CHECK(isHTMLSummaryElement(*content->firstChild()));
  return ToElement(content->firstChild());
}

void HTMLDetailsElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == openAttr) {
    bool old_value = is_open_;
    is_open_ = !params.new_value.IsNull();
    if (is_open_ == old_value)
      return;

    // Dispatch toggle event asynchronously.
    pending_event_ =
        TaskRunnerHelper::Get(TaskType::kDOMManipulation, &GetDocument())
            ->PostCancellableTask(
                BLINK_FROM_HERE,
                WTF::Bind(&HTMLDetailsElement::DispatchPendingEvent,
                          WrapPersistent(this)));

    Element* content = EnsureUserAgentShadowRoot().getElementById(
        ShadowElementNames::DetailsContent());
    DCHECK(content);
    if (is_open_)
      content->RemoveInlineStyleProperty(CSSPropertyDisplay);
    else
      content->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

    // Invalidate the LayoutDetailsMarker in order to turn the arrow signifying
    // if the details element is open or closed.
    Element* summary = FindMainSummary();
    DCHECK(summary);

    Element* control = toHTMLSummaryElement(summary)->MarkerControl();
    if (control && control->GetLayoutObject())
      control->GetLayoutObject()->SetShouldDoFullPaintInvalidation();

    return;
  }
  HTMLElement::ParseAttribute(params);
}

void HTMLDetailsElement::ToggleOpen() {
  setAttribute(openAttr, is_open_ ? g_null_atom : g_empty_atom);
}

bool HTMLDetailsElement::IsInteractiveContent() const {
  return true;
}

}  // namespace blink
