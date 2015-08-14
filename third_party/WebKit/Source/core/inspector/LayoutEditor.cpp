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

PassRefPtr<JSONObject> createAnchor(const FloatPoint& point, const String& type, const String& propertyName, FloatPoint deltaVector, PassRefPtr<JSONObject> valueDescription)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("x", point.x());
    object->setNumber("y", point.y());
    object->setString("type", type);
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

float det(float a11, float a12, float a21, float a22)
{
    return a11 * a22 - a12 * a21;
}

FloatPoint projection(const FloatPoint& anchor, const FloatPoint& orthogonalVector, const FloatPoint& point)
{
    float a1 = orthogonalVector.x();
    float b1 = orthogonalVector.y();
    float c1 = - anchor.x() * a1 - anchor.y() * b1;

    float a2 = orthogonalVector.y();
    float b2 = - orthogonalVector.x();
    float c2 = - point.x() * a2 - point.y() * b2;
    float d = det(a1, b1, a2, b2);
    ASSERT(d < 0);
    float x = - det(c1, b1, c2, b2) / d;
    float y = - det(a1, c1, a2, c2) / d;
    return FloatPoint(x, y);
}

FloatPoint translatePoint(const FloatPoint& point, const FloatPoint& orthogonalVector, float distance)
{
    float d = sqrt(orthogonalVector.x() * orthogonalVector.x() + orthogonalVector.y() * orthogonalVector.y());
    float dx = - orthogonalVector.y() * distance / d;
    float dy = orthogonalVector.x() * distance / d;
    return FloatPoint(point.x() + dx, point.y() + dy);
}

FloatQuad translateAndProject(const FloatQuad& origin, const FloatQuad& orthogonals, const FloatQuad& projectOn, float distance)
{
    FloatQuad result;
    result.setP1(projection(projectOn.p1(), orthogonals.p1(), translatePoint(origin.p1(), orthogonals.p1(), distance)));
    result.setP2(projection(projectOn.p2(), orthogonals.p2(), translatePoint(origin.p2(), orthogonals.p2(), distance)));
    // We want to translate at top and bottom point in the same direction, so we use p1 here.
    result.setP3(projection(projectOn.p3(), orthogonals.p3(), translatePoint(origin.p3(), orthogonals.p1(), distance)));
    result.setP4(projection(projectOn.p4(), orthogonals.p4(), translatePoint(origin.p4(), orthogonals.p2(), distance)));
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

    FloatQuad content, padding, border, margin;
    InspectorHighlight::buildNodeQuads(m_element.get(), &content, &padding, &border, &margin);
    FloatQuad orthogonals = orthogonalVectors(padding);

    RefPtr<JSONObject> object = JSONObject::create();
    RefPtr<JSONArray> anchors = JSONArray::create();
    FloatQuad contentMeans = means(content);

    FloatQuad paddingHandles = translateAndProject(contentMeans, orthogonals, padding, 0);
    appendAnchorFor(anchors.get(), "padding", "padding-top", paddingHandles.p1(), orthogonals.p1());
    appendAnchorFor(anchors.get(), "padding", "padding-right", paddingHandles.p2(), orthogonals.p2());
    appendAnchorFor(anchors.get(), "padding", "padding-bottom", paddingHandles.p3(), orthogonals.p3());
    appendAnchorFor(anchors.get(), "padding", "padding-left", paddingHandles.p4(), orthogonals.p4());

    FloatQuad marginHandles = translateAndProject(contentMeans, orthogonals, margin, 12);
    appendAnchorFor(anchors.get(), "margin", "margin-top", marginHandles.p1(), orthogonals.p1());
    appendAnchorFor(anchors.get(), "margin", "margin-right", marginHandles.p2(), orthogonals.p2());
    appendAnchorFor(anchors.get(), "margin", "margin-bottom", marginHandles.p3(), orthogonals.p3());
    appendAnchorFor(anchors.get(), "margin", "margin-left", marginHandles.p4(), orthogonals.p4());

    object->setArray("anchors", anchors.release());
    return object.release();
}

RefPtrWillBeRawPtr<CSSPrimitiveValue> LayoutEditor::getPropertyCSSValue(CSSPropertyID property) const
{
    RefPtrWillBeRawPtr<CSSStyleDeclaration> style = m_cssAgent->findEffectiveDeclaration(m_element.get(), property);
    if (!style)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValue> cssValue = style->getPropertyCSSValueInternal(property);
    if (!cssValue || !cssValue->isPrimitiveValue())
        return nullptr;

    return toCSSPrimitiveValue(cssValue.get());
}

PassRefPtr<JSONObject> LayoutEditor::createValueDescription(const String& propertyName) const
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> cssValue = getPropertyCSSValue(cssPropertyID(propertyName));
    if (cssValue && !(cssValue->isLength() || cssValue->isPercentage()))
        return nullptr;

    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("value", cssValue ? cssValue->getFloatValue() : 0);
    object->setString("unit", CSSPrimitiveValue::unitTypeToString(cssValue ? cssValue->typeWithCalcResolved() : CSSPrimitiveValue::UnitType::Pixels));
    // TODO: Support an editing of other popular units like: em, rem
    object->setBoolean("mutable", !cssValue || cssValue->isPx());
    return object.release();
}

void LayoutEditor::appendAnchorFor(JSONArray* anchors, const String& type, const String& propertyName, const FloatPoint& position, const FloatPoint& orthogonalVector) const
{
    RefPtr<JSONObject> description = createValueDescription(propertyName);
    if (description)
        anchors->pushObject(createAnchor(position, type, propertyName, orthogonalVector, description.release()));
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
