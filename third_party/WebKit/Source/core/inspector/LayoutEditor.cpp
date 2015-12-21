// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

PassRefPtr<JSONObject> createAnchor(const String& type, const String& propertyName, PassRefPtr<JSONObject> valueDescription)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setString("type", type);
    object->setString("propertyName", propertyName);
    object->setObject("propertyValue", valueDescription);
    return object.release();
}

PassRefPtr<JSONObject> pointToJSON(FloatPoint point)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("x", point.x());
    object->setNumber("y", point.y());
    return object.release();
}

PassRefPtr<JSONObject> quadToJSON(FloatQuad& quad)
{
    RefPtr<JSONObject> object = JSONObject::create();
    object->setObject("p1", pointToJSON(quad.p1()));
    object->setObject("p2", pointToJSON(quad.p2()));
    object->setObject("p3", pointToJSON(quad.p3()));
    object->setObject("p4", pointToJSON(quad.p4()));
    return object.release();
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

InspectorHighlightConfig affectedNodesHighlightConfig()
{
    // TODO: find a better color
    InspectorHighlightConfig config;
    config.content = Color(95, 127, 162, 100);
    config.padding = Color(95, 127, 162, 100);
    config.margin = Color(95, 127, 162, 100);
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

float roundValue(float value, CSSPrimitiveValue::UnitType unitType)
{
    float roundTo = unitType == CSSPrimitiveValue::UnitType::Pixels ? 1 : 0.05;
    return round(value / roundTo) * roundTo;
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
    , m_matchedStyles(cssAgent->matchingStyles(element))
    , m_currentRuleIndex(0)
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
    visitor->trace(m_matchedStyles);
}

void LayoutEditor::rebuild()
{
    RefPtr<JSONObject> object = JSONObject::create();
    RefPtr<JSONArray> anchors = JSONArray::create();

    appendAnchorFor(anchors.get(), "padding", "padding-top");
    appendAnchorFor(anchors.get(), "padding", "padding-right");
    appendAnchorFor(anchors.get(), "padding", "padding-bottom");
    appendAnchorFor(anchors.get(), "padding", "padding-left");

    appendAnchorFor(anchors.get(), "margin", "margin-top");
    appendAnchorFor(anchors.get(), "margin", "margin-right");
    appendAnchorFor(anchors.get(), "margin", "margin-bottom");
    appendAnchorFor(anchors.get(), "margin", "margin-left");

    object->setArray("anchors", anchors.release());

    FloatQuad content, padding, border, margin;
    InspectorHighlight::buildNodeQuads(m_element.get(), &content, &padding, &border, &margin);
    object->setObject("contentQuad", quadToJSON(content));
    object->setObject("paddingQuad", quadToJSON(padding));
    object->setObject("marginQuad", quadToJSON(margin));
    object->setObject("borderQuad", quadToJSON(border));
    evaluateInOverlay("showLayoutEditor", object.release());
    editableSelectorUpdated(false);
}

RefPtrWillBeRawPtr<CSSPrimitiveValue> LayoutEditor::getPropertyCSSValue(CSSPropertyID property) const
{
    RefPtrWillBeRawPtr<CSSStyleDeclaration> style = m_cssAgent->findEffectiveDeclaration(property, m_matchedStyles);
    if (!style)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValue> cssValue = style->getPropertyCSSValueInternal(property);
    if (!cssValue || !cssValue->isPrimitiveValue())
        return nullptr;

    return toCSSPrimitiveValue(cssValue.get());
}

bool LayoutEditor::growInside(String propertyName, CSSPrimitiveValue* value)
{
    FloatQuad content1, padding1, border1, margin1;
    InspectorHighlight::buildNodeQuads(m_element.get(), &content1, &padding1, &border1, &margin1);

    CSSStyleDeclaration* elementStyle = m_element->style();
    if (!elementStyle)
        return false;

    String initialValue = elementStyle->getPropertyValue(propertyName);
    String initialPriority = elementStyle->getPropertyPriority(propertyName);
    String newValue;
    if (value)
        newValue = String::format("%f", value->getFloatValue() + 1) + CSSPrimitiveValue::unitTypeToString(value->typeWithCalcResolved());
    else
        newValue = "5px";

    TrackExceptionState exceptionState;
    elementStyle->setProperty(propertyName, newValue, "important", exceptionState);
    m_element->ownerDocument()->updateLayout();

    FloatQuad content2, padding2, border2, margin2;
    InspectorHighlight::buildNodeQuads(m_element.get(), &content2, &padding2, &border2, &margin2);

    elementStyle->setProperty(propertyName, initialValue, initialPriority, exceptionState);
    m_element->ownerDocument()->updateLayout();

    float eps = 0.0001;
    FloatRect boundingBox1, boundingBox2;

    if (propertyName.startsWith("padding")) {
        boundingBox1 = padding1.boundingBox();
        boundingBox2 = padding2.boundingBox();
    } else {
        boundingBox1 = margin1.boundingBox();
        boundingBox2 = margin2.boundingBox();
    }

    if (propertyName.endsWith("left"))
        return std::abs(boundingBox1.x() - boundingBox2.x() ) < eps;

    if (propertyName.endsWith("right"))
        return std::abs(boundingBox1.maxX() - boundingBox2.maxX() ) < eps;

    if (propertyName.endsWith("top"))
        return std::abs(boundingBox1.y() - boundingBox2.y()) < eps;

    if (propertyName.endsWith("bottom"))
        return std::abs(boundingBox1.maxY() - boundingBox2.maxY()) < eps;
    return false;
}

PassRefPtr<JSONObject> LayoutEditor::createValueDescription(const String& propertyName)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> cssValue = getPropertyCSSValue(cssPropertyID(propertyName));
    if (cssValue && !(cssValue->isLength() || cssValue->isPercentage()))
        return nullptr;

    RefPtr<JSONObject> object = JSONObject::create();
    object->setNumber("value", cssValue ? cssValue->getFloatValue() : 0);
    CSSPrimitiveValue::UnitType unitType = cssValue ? cssValue->typeWithCalcResolved() : CSSPrimitiveValue::UnitType::Pixels;
    object->setString("unit", CSSPrimitiveValue::unitTypeToString(unitType));
    object->setBoolean("mutable", isMutableUnitType(unitType));

    if (!m_growsInside.contains(propertyName))
        m_growsInside.set(propertyName, growInside(propertyName, cssValue.get()));

    object->setBoolean("growInside", m_growsInside.get(propertyName));
    return object.release();
}

void LayoutEditor::appendAnchorFor(JSONArray* anchors, const String& type, const String& propertyName)
{
    RefPtr<JSONObject> description = createValueDescription(propertyName);
    if (description)
        anchors->pushObject(createAnchor(type, propertyName, description.release()));
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
        float newValue = cssDelta / m_factor + m_propertyInitialValue;
        newValue = newValue >= 0 ? roundValue(newValue, m_valueUnitType) : 0;
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

void LayoutEditor::nextSelector()
{
    if (m_currentRuleIndex == m_matchedStyles.size() - 1)
        return;

    ++m_currentRuleIndex;
    editableSelectorUpdated(true);
}

void LayoutEditor::previousSelector()
{
    if (m_currentRuleIndex == 0)
        return;

    --m_currentRuleIndex;
    editableSelectorUpdated(true);
}

void LayoutEditor::editableSelectorUpdated(bool hasChanged) const
{
    CSSStyleDeclaration* style = m_matchedStyles.at(m_currentRuleIndex).get();
    evaluateInOverlay("setSelectorInLayoutEditor", currentSelectorInfo(style));
    if (hasChanged)
        m_cssAgent->layoutEditorItemSelected(m_element.get(), style);
}

PassRefPtr<JSONObject> LayoutEditor::currentSelectorInfo(CSSStyleDeclaration* style) const
{
    RefPtr<JSONObject> object = JSONObject::create();
    CSSStyleRule* rule = style->parentRule() ? toCSSStyleRule(style->parentRule()) : nullptr;
    String currentSelectorText = rule ? rule->selectorText() : "element.style";
    object->setString("selector", currentSelectorText);

    Document* ownerDocument = m_element->ownerDocument();
    if (!ownerDocument->isActive() || !rule)
        return object.release();

    Vector<String> medias;
    buildMediaListChain(rule, medias);
    RefPtr<JSONArray> mediasJSONArray = JSONArray::create();
    for (size_t i = 0; i < medias.size(); ++i)
        mediasJSONArray->pushString(medias[i]);

    object->setArray("medias", mediasJSONArray.release());

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
    String errorString;
    m_cssAgent->setLayoutEditorValue(&errorString, m_element.get(), m_matchedStyles.at(m_currentRuleIndex), m_changingProperty, value, false);
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
