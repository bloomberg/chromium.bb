// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGAnimatedHref_h
#define SVGAnimatedHref_h

#include "core/svg/SVGAnimatedString.h"

namespace blink {

// This is an "access wrapper" for the 'href' attribute. It holds one object
// for 'href' in the null/default NS and one for 'href' in the XLink NS. The
// internal objects are the ones added to an SVGElement's property map and
// hence any updates/synchronization/etc via the "attribute DOM" (setAttribute
// and friends) will operate on the internal objects, while users of 'href'
// will be using this interface (which essentially just selects one of the
// internal objects and forwards the operation to it.)
class SVGAnimatedHref final : public SVGAnimatedString {
public:
    static PassRefPtrWillBeRawPtr<SVGAnimatedHref> create(SVGElement* contextElement);

    SVGString* baseValue() { return backingHref()->baseValue(); }
    SVGString* currentValue() { return backingHref()->currentValue(); }
    const SVGString* currentValue() const { return backingHref()->currentValue(); }

    // None of the below should be called on this interface.
    SVGPropertyBase* currentValueBase() override;
    const SVGPropertyBase& baseValueBase() const override;
    bool isAnimating() const override;
    PassRefPtrWillBeRawPtr<SVGPropertyBase> createAnimatedValue() override;
    void setAnimatedValue(PassRefPtrWillBeRawPtr<SVGPropertyBase>) override;
    void animationEnded() override;
    SVGParsingError setBaseValueAsString(const String&) override;
    bool needsSynchronizeAttribute() override;
    void synchronizeAttribute() override;

    String baseVal() override;
    void setBaseVal(const String&, ExceptionState&) override;
    String animVal() override;

    bool isSpecified() const { return m_href->isSpecified() || m_xlinkHref->isSpecified(); }

    static bool isKnownAttribute(const QualifiedName&);
    void addToPropertyMap(SVGElement*);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit SVGAnimatedHref(SVGElement* contextElement);

    SVGAnimatedString* backingHref() const;

    RefPtrWillBeMember<SVGAnimatedString> m_href;
    RefPtrWillBeMember<SVGAnimatedString> m_xlinkHref;
};

} // namespace blink

#endif // SVGAnimatedHref_h
