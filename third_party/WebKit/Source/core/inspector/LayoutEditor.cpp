// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/LayoutEditor.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/MediaList.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StaticNodeList.h"
#include "core/frame/FrameView.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorHighlight.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "platform/JSONValues.h"
#include "platform/ScriptForbiddenScope.h"

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

bool isMutableUnitType(CSSPrimitiveValue::UnitType unitType)
{
    return unitType == CSSPrimitiveValue::UnitType::Pixels || unitType == CSSPrimitiveValue::UnitType::Ems || unitType == CSSPrimitiveValue::UnitType::Percentage || unitType == CSSPrimitiveValue::UnitType::Rems;
}

String truncateZeroes(const String& number)
{
    if (!number.contains('.'))
        return number;

    int removeCount = 0;
    while (number[number.length() - removeCount - 1] == '0')
        removeCount++;

    if (number[number.length() - removeCount - 1] == '.')
        removeCount++;

    return number.left(number.length() - removeCount);
}

float toValidValue(CSSPropertyID propertyId, float newValue)
{
    if (CSSPropertyPaddingBottom <= propertyId && propertyId <= CSSPropertyPaddingTop)
        return newValue >= 0 ? newValue : 0;

    return newValue;
}

InspectorHighlightConfig affectedNodesHighlightConfig()
{
    // TODO: find a better color
    InspectorHighlightConfig config;
    config.content = Color(95, 127, 162, 100);
    config.padding = Color(95, 127, 162, 100);
    config.margin = Color(95, 127, 162, 100);
    return config;
}

InspectorHighlightConfig chosenNodeHighlightConfig()
{
    InspectorHighlightConfig config;
    config.padding = Color(147, 196, 125, 140);
    config.margin = Color(246, 178, 107, 168);
    return config;
}

void collectMediaQueriesFromRule(CSSRule* rule, Vector<String>& mediaArray)
{
    MediaList* mediaList;
    if (rule->type() == CSSRule::MEDIA_RULE) {
        CSSMediaRule* mediaRule = toCSSMediaRule(rule);
        mediaList = mediaRule->media();
    } else if (rule->type() == CSSRule::IMPORT_RULE) {
        CSSImportRule* importRule = toCSSImportRule(rule);
        mediaList = importRule->media();
    } else {
        mediaList = nullptr;
    }

    if (mediaList && mediaList->length())
        mediaArray.append(mediaList->mediaText());
}

void buildMediaListChain(CSSRule* rule, Vector<String>& mediaArray)
{
    while (rule) {
        collectMediaQueriesFromRule(rule, mediaArray);
        if (rule->parentRule()) {
            rule = rule->parentRule();
        } else if (rule->parentStyleSheet()) {
            CSSStyleSheet* styleSheet = rule->parentStyleSheet();
            MediaList* mediaList = styleSheet->media();
            if (mediaList && mediaList->length())
                mediaArray.append(mediaList->mediaText());

            rule = styleSheet->ownerRule();
        } else {
            break;
        }
    }
}

} // namespace

LayoutEditor::LayoutEditor(Element* element, InspectorCSSAgent* cssAgent, InspectorDOMAgent* domAgent, ScriptController* scriptController)
    : m_element(element)
    , m_cssAgent(cssAgent)
    , m_domAgent(domAgent)
    , m_scriptController(scriptController)
    , m_changingProperty(CSSPropertyInvalid)
    , m_propertyInitialValue(0)
    , m_isDirty(false)
    , m_matchedRules(m_cssAgent->matchedRulesList(element))
    , m_currentRuleIndex(m_matchedRules->length())
{
}

LayoutEditor::~LayoutEditor()
{
}

void LayoutEditor::dispose()
{
    if (!m_isDirty)
        return;

    ErrorString errorString;
    m_domAgent->undo(&errorString);
}

DEFINE_TRACE(LayoutEditor)
{
    visitor->trace(m_element);
    visitor->trace(m_cssAgent);
    visitor->trace(m_domAgent);
    visitor->trace(m_scriptController);
    visitor->trace(m_matchedRules);
}

void LayoutEditor::rebuild() const
{
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
    InspectorHighlight highlight(m_element.get(), chosenNodeHighlightConfig(), false);
    object->setObject("nodeHighlight", highlight.asJSONObject());
    evaluateInOverlay("showLayoutEditor", object.release());
    pushSelectorInfoInOverlay();
}

RefPtrWillBeRawPtr<CSSPrimitiveValue> LayoutEditor::getPropertyCSSValue(CSSPropertyID property) const
{
    RefPtrWillBeRawPtr<CSSStyleDeclaration> style = m_cssAgent->findEffectiveDeclaration(property, m_matchedRules.get(), m_element->style());
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
    CSSPrimitiveValue::UnitType unitType = cssValue ? cssValue->typeWithCalcResolved() : CSSPrimitiveValue::UnitType::Pixels;
    object->setString("unit", CSSPrimitiveValue::unitTypeToString(unitType));
    object->setBoolean("mutable", isMutableUnitType(unitType));
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
    if (!m_changingProperty)
        return;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> cssValue = getPropertyCSSValue(m_changingProperty);
    m_valueUnitType = cssValue ? cssValue->typeWithCalcResolved() : CSSPrimitiveValue::UnitType::Pixels;
    if (!isMutableUnitType(m_valueUnitType))
        return;

    switch (m_valueUnitType) {
    case CSSPrimitiveValue::UnitType::Pixels:
        m_factor = 1;
        break;
    case CSSPrimitiveValue::UnitType::Ems:
        m_factor = m_element->computedStyle()->computedFontSize();
        break;
    case CSSPrimitiveValue::UnitType::Percentage:
        // It is hard to correctly support percentages, so we decided hack it this way: 100% = 1000px
        m_factor = 10;
        break;
    case CSSPrimitiveValue::UnitType::Rems:
        m_factor = m_element->document().computedStyle()->computedFontSize();
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    m_propertyInitialValue = cssValue ? cssValue->getFloatValue() : 0;
}

void LayoutEditor::overlayPropertyChanged(float cssDelta)
{
    if (m_changingProperty && m_factor) {
        float newValue = toValidValue(m_changingProperty, cssDelta / m_factor + m_propertyInitialValue);
        m_isDirty |= setCSSPropertyValueInCurrentRule(truncateZeroes(String::format("%.2f", newValue)) + CSSPrimitiveValue::unitTypeToString(m_valueUnitType));
    }
}

void LayoutEditor::overlayEndedPropertyChange()
{
    m_changingProperty = CSSPropertyInvalid;
    m_propertyInitialValue = 0;
    m_factor = 0;
    m_valueUnitType = CSSPrimitiveValue::UnitType::Unknown;
}

void LayoutEditor::commitChanges()
{
    if (!m_isDirty)
        return;

    m_isDirty = false;
    ErrorString errorString;
    m_domAgent->markUndoableState(&errorString);
}

bool LayoutEditor::currentStyleIsInline() const
{
    return m_currentRuleIndex == m_matchedRules->length();
}

void LayoutEditor::nextSelector()
{
    if (m_currentRuleIndex == 0)
        return;

    m_currentRuleIndex--;
    pushSelectorInfoInOverlay();
}

void LayoutEditor::previousSelector()
{
    if (currentStyleIsInline())
        return;

    m_currentRuleIndex++;
    pushSelectorInfoInOverlay();
}
void LayoutEditor::pushSelectorInfoInOverlay() const
{
    evaluateInOverlay("setSelectorInLayoutEditor", currentSelectorInfo());
}

PassRefPtr<JSONObject> LayoutEditor::currentSelectorInfo() const
{
    RefPtr<JSONObject> object = JSONObject::create();
    String currentSelectorText = currentStyleIsInline() ? "inline style" : toCSSStyleRule(m_matchedRules->item(m_currentRuleIndex))->selectorText();
    object->setString("selector", currentSelectorText);

    Document* ownerDocument = m_element->ownerDocument();
    if (!ownerDocument->isActive() || currentStyleIsInline())
        return object.release();

    bool hasSameSelectors = false;
    for (unsigned i = 0; i < m_matchedRules->length(); i++) {
        if (i != m_currentRuleIndex && toCSSStyleRule(m_matchedRules->item(i))->selectorText() == currentSelectorText) {
            hasSameSelectors = true;
            break;
        }
    }

    if (hasSameSelectors) {
        Vector<String> medias;
        buildMediaListChain(m_matchedRules->item(m_currentRuleIndex), medias);
        RefPtr<JSONArray> mediasJSONArray = JSONArray::create();
        for (size_t i = 0; i < medias.size(); ++i)
            mediasJSONArray->pushString(medias[i]);

        object->setArray("medias", mediasJSONArray.release());
    }

    TrackExceptionState exceptionState;
    RefPtrWillBeRawPtr<StaticElementList> elements = ownerDocument->querySelectorAll(AtomicString(currentSelectorText), exceptionState);

    if (!elements || exceptionState.hadException())
        return object.release();

    RefPtr<JSONArray> highlights = JSONArray::create();
    InspectorHighlightConfig config = affectedNodesHighlightConfig();
    for (unsigned i = 0; i < elements->length(); ++i) {
        Element* element = elements->item(i);
        if (element == m_element)
            continue;

        InspectorHighlight highlight(element, config, false);
        highlights->pushObject(highlight.asJSONObject());
    }

    object->setArray("nodes", highlights.release());
    return object.release();
}

bool LayoutEditor::setCSSPropertyValueInCurrentRule(const String& value)
{
    RefPtrWillBeRawPtr<CSSStyleDeclaration> effectiveDeclaration = m_cssAgent->findEffectiveDeclaration(m_changingProperty, m_matchedRules.get(), m_element->style());
    bool forceImportant = false;

    if (effectiveDeclaration) {
        CSSStyleRule* effectiveRule = nullptr;
        if (effectiveDeclaration->parentRule() && effectiveDeclaration->parentRule()->type() == CSSRule::STYLE_RULE)
            effectiveRule = toCSSStyleRule(effectiveDeclaration->parentRule());

        unsigned effectiveRuleIndex = m_matchedRules->length();
        for (unsigned i = 0; i < m_matchedRules->length(); ++i) {
            if (m_matchedRules->item(i) == effectiveRule) {
                effectiveRuleIndex = i;
                break;
            }
        }
        forceImportant = effectiveDeclaration->getPropertyPriority(getPropertyNameString(m_changingProperty)) == "important";
        forceImportant |= effectiveRuleIndex > m_currentRuleIndex;
    }

    CSSStyleDeclaration* style = currentStyleIsInline() ? m_element->style() : toCSSStyleRule(m_matchedRules->item(m_currentRuleIndex))->style();

    String errorString;
    m_cssAgent->setCSSPropertyValue(&errorString, m_element.get(), style, m_changingProperty, value, forceImportant);
    return errorString.isEmpty();
}

void LayoutEditor::evaluateInOverlay(const String& method, PassRefPtr<JSONValue> argument) const
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    RefPtr<JSONArray> command = JSONArray::create();
    command->pushString(method);
    command->pushValue(argument);
    m_scriptController->executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

} // namespace blink
