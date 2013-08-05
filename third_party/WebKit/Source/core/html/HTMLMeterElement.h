/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef HTMLMeterElement_h
#define HTMLMeterElement_h

#include "core/html/LabelableElement.h"

namespace WebCore {

class ExceptionState;
class MeterValueElement;
class RenderMeter;

class HTMLMeterElement FINAL : public LabelableElement {
public:
    static PassRefPtr<HTMLMeterElement> create(const QualifiedName&, Document*);

    enum GaugeRegion {
        GaugeRegionOptimum,
        GaugeRegionSuboptimal,
        GaugeRegionEvenLessGood
    };

    double min() const;
    void setMin(double, ExceptionState&);

    double max() const;
    void setMax(double, ExceptionState&);

    double value() const;
    void setValue(double, ExceptionState&);

    double low() const;
    void setLow(double, ExceptionState&);

    double high() const;
    void setHigh(double, ExceptionState&);

    double optimum() const;
    void setOptimum(double, ExceptionState&);

    double valueRatio() const;
    GaugeRegion gaugeRegion() const;

    bool canContainRangeEndPoint() const { return false; }

private:
    HTMLMeterElement(const QualifiedName&, Document*);
    virtual ~HTMLMeterElement();

    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }
    RenderMeter* renderMeter() const;

    virtual bool supportLabels() const OVERRIDE { return true; }

    virtual bool recalcWillValidate() const { return false; }
    virtual RenderObject* createRenderer(RenderStyle*);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    void didElementStateChange();
    virtual void didAddUserAgentShadowRoot(ShadowRoot*) OVERRIDE;

    RefPtr<MeterValueElement> m_value;
};

inline bool isHTMLMeterElement(Node* node)
{
    return node->hasTagName(HTMLNames::meterTag);
}

inline HTMLMeterElement* toHTMLMeterElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || isHTMLMeterElement(node));
    return static_cast<HTMLMeterElement*>(node);
}

} // namespace

#endif
