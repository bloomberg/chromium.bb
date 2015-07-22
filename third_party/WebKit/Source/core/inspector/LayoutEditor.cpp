// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/LayoutEditor.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/frame/FrameView.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorHighlight.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "platform/JSONValues.h"

namespace blink {

namespace {

PassRefPtr<JSONObject> createAnchor(const FloatPoint& point, const String& propertyName, FloatPoint deltaVector, PassRefPtr<JSONObject> valueDescription)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("x", point.x());
    object->setNumber("y", point.y());
    object->setString("propertyName", propertyName);

    RefPtr<JSONObject> deltaVectorJSON = JSONObject::create();
    deltaVectorJSON->setNumber("x", deltaVector.x());
    deltaVectorJSON->setNumber("y", deltaVector.y());
    object->setObject("deltaVector", deltaVectorJSON.release());
    object->setObject("propertyValue", valueDescription);
    return object.release();
}

FloatPoint orthogonalVector(FloatPoint from, FloatPoint to, FloatPoint defaultVector)
{
    if (from == to)
        return defaultVector;

    return FloatPoint(to.y() - from.y(), from.x() - to.x());
}

FloatPoint mean(const FloatPoint& p1, const FloatPoint& p2)
{
    float x = (p1.x() + p2.x()) / 2;
    float y = (p1.y() + p2.y()) / 2;
    return FloatPoint(x, y);
}

FloatQuad means(const FloatQuad& quad)
{
    FloatQuad result;
    result.setP1(mean(quad.p1(), quad.p2()));
    result.setP2(mean(quad.p2(), quad.p3()));
    result.setP3(mean(quad.p3(), quad.p4()));
    result.setP4(mean(quad.p4(), quad.p1()));
    return result;
}

FloatQuad orthogonalVectors(const FloatQuad& quad)
{
    FloatQuad result;
    result.setP1(orthogonalVector(quad.p1(), quad.p2(), FloatPoint(0, -1)));
    result.setP2(orthogonalVector(quad.p2(), quad.p3(), FloatPoint(1, 0)));
    result.setP3(orthogonalVector(quad.p3(), quad.p4(), FloatPoint(0, 1)));
    result.setP4(orthogonalVector(quad.p4(), quad.p1(), FloatPoint(-1, 0)));
    return result;
}

} // namespace

LayoutEditor::LayoutEditor(InspectorCSSAgent* cssAgent)
    : m_element(nullptr)
    , m_cssAgent(cssAgent)
    , m_changingProperty(CSSPropertyInvalid)
    , m_propertyInitialValue(0)
{
}

DEFINE_TRACE(LayoutEditor)
{
    visitor->trace(m_element);
    visitor->trace(m_cssAgent);
}

void LayoutEditor::setNode(Node* node)
{
    m_element = node && node->isElementNode() ? toElement(node) : nullptr;
    m_changingProperty = CSSPropertyInvalid;
    m_propertyInitialValue = 0;
}

PassRefPtr<JSONObject> LayoutEditor::buildJSONInfo() const
{
    if (!m_element)
        return nullptr;

    FloatQuad padding, unused;
    InspectorHighlight::buildNodeQuads(m_element.get(), &unused, &padding, &unused, &unused);

    FloatQuad paddingMeans = means(padding);
    FloatQuad paddingOrthogonalVectors = orthogonalVectors(padding);

    RefPtr<JSONObject> object = JSONObject::create();
    RefPtr<JSONArray> anchors = JSONArray::create();

    appendAnchorFor(anchors.get(), "padding-top", paddingMeans.p1(), paddingOrthogonalVectors.p1());
    appendAnchorFor(anchors.get(), "padding-right", paddingMeans.p2(), paddingOrthogonalVectors.p2());
    appendAnchorFor(anchors.get(), "padding-bottom", paddingMeans.p3(), paddingOrthogonalVectors.p3());
    appendAnchorFor(anchors.get(), "padding-left", paddingMeans.p4(), paddingOrthogonalVectors.p4());

    object->setArray("anchors", anchors.release());
    return object.release();
}

RefPtrWillBeRawPtr<CSSPrimitiveValue> LayoutEditor::getPropertyCSSValue(CSSPropertyID property) const
{
    CSSStyleDeclaration* style = m_cssAgent->findEffectiveDeclaration(m_element.get(), property);
    if (!style)
        return nullptr;
    RefPtrWillBeRawPtr<CSSValue> cssValue = style->getPropertyCSSValueInternal(property);
    if (!cssValue->isPrimitiveValue())
        return nullptr;

    return toCSSPrimitiveValue(cssValue.get());
}

PassRefPtr<JSONObject> LayoutEditor::createValueDescription(const String& propertyName) const
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> cssValue = getPropertyCSSValue(cssPropertyID(propertyName));
    if (cssValue && !cssValue->isPx())
        return nullptr;

    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("value", cssValue ? cssValue->getFloatValue() : 0);
    object->setString("unit", "px");
    return object.release();
}

void LayoutEditor::appendAnchorFor(JSONArray* anchors, const String& propertyName, const FloatPoint& position, const FloatPoint& orthogonalVector) const
{
    RefPtr<JSONObject> description = createValueDescription(propertyName);
    if (description)
        anchors->pushObject(createAnchor(position, propertyName, orthogonalVector, description.release()));
}

void LayoutEditor::overlayStartedPropertyChange(const String& anchorName)
{
    m_changingProperty = cssPropertyID(anchorName);
    if (!m_element || !m_changingProperty)
        return;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> cssValue = getPropertyCSSValue(m_changingProperty);
    if (cssValue && !cssValue->isPx())
        return;

    m_propertyInitialValue = cssValue ? cssValue->getFloatValue() : 0;
}

void LayoutEditor::overlayPropertyChanged(float cssDelta)
{
    if (m_changingProperty) {
        String errorString;
        m_cssAgent->setCSSPropertyValue(&errorString, m_element.get(), m_changingProperty, String::number(cssDelta + m_propertyInitialValue) + "px");
    }
}

void LayoutEditor::overlayEndedPropertyChange()
{
    m_changingProperty = CSSPropertyInvalid;
    m_propertyInitialValue = 0;
}

} // namespace blink
