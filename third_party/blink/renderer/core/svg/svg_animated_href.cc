// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_animated_href.h"

#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"

namespace blink {

SVGAnimatedHref* SVGAnimatedHref::Create(SVGElement* context_element) {
  return new SVGAnimatedHref(context_element);
}

void SVGAnimatedHref::Trace(blink::Visitor* visitor) {
  visitor->Trace(xlink_href_);
  SVGAnimatedString::Trace(visitor);
}

SVGAnimatedHref::SVGAnimatedHref(SVGElement* context_element)
    : SVGAnimatedString(context_element, SVGNames::hrefAttr),
      xlink_href_(
          SVGAnimatedString::Create(context_element, XLinkNames::hrefAttr)) {}

void SVGAnimatedHref::AddToPropertyMap(SVGElement* element) {
  element->AddToPropertyMap(this);
  element->AddToPropertyMap(xlink_href_);
}

bool SVGAnimatedHref::IsKnownAttribute(const QualifiedName& attr_name) {
  return attr_name.Matches(SVGNames::hrefAttr) ||
         attr_name.Matches(XLinkNames::hrefAttr);
}

SVGString* SVGAnimatedHref::CurrentValue() {
  return BackingString()->SVGAnimatedString::CurrentValue();
}

const SVGString* SVGAnimatedHref::CurrentValue() const {
  return BackingString()->SVGAnimatedString::CurrentValue();
}

String SVGAnimatedHref::baseVal() {
  UseCounter::Count(contextElement()->GetDocument(),
                    WebFeature::kSVGHrefBaseVal);
  return BackingString()->SVGAnimatedString::baseVal();
}

void SVGAnimatedHref::setBaseVal(const String& value,
                                 ExceptionState& exception_state) {
  UseCounter::Count(contextElement()->GetDocument(),
                    WebFeature::kSVGHrefBaseVal);
  return BackingString()->SVGAnimatedString::setBaseVal(value, exception_state);
}

String SVGAnimatedHref::animVal() {
  UseCounter::Count(contextElement()->GetDocument(),
                    WebFeature::kSVGHrefAnimVal);
  return BackingString()->SVGAnimatedString::animVal();
}

SVGAnimatedString* SVGAnimatedHref::BackingString() {
  return UseXLink() ? xlink_href_.Get() : this;
}

const SVGAnimatedString* SVGAnimatedHref::BackingString() const {
  return UseXLink() ? xlink_href_.Get() : this;
}

bool SVGAnimatedHref::UseXLink() const {
  return !SVGAnimatedString::IsSpecified() && xlink_href_->IsSpecified();
}

}  // namespace blink
