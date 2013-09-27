/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "core/css/resolver/FilterOperationResolver.h"

#include "core/css/CSSFilterValue.h"
#include "core/css/CSSMixFunctionValue.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSShaderValue.h"
#include "core/css/ShadowValue.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/platform/graphics/filters/custom/CustomFilterArrayParameter.h"
#include "core/platform/graphics/filters/custom/CustomFilterConstants.h"
#include "core/platform/graphics/filters/custom/CustomFilterNumberParameter.h"
#include "core/platform/graphics/filters/custom/CustomFilterOperation.h"
#include "core/platform/graphics/filters/custom/CustomFilterParameter.h"
#include "core/platform/graphics/filters/custom/CustomFilterProgramInfo.h"
#include "core/platform/graphics/filters/custom/CustomFilterTransformParameter.h"
#include "core/rendering/style/StyleCustomFilterProgram.h"
#include "core/rendering/style/StyleShader.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

static Length convertToFloatLength(CSSPrimitiveValue* primitiveValue, const RenderStyle* style, const RenderStyle* rootStyle, double multiplier)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | FractionConversion>(style, rootStyle, multiplier) : Length(Undefined);
}

static FilterOperation::OperationType filterOperationForType(CSSFilterValue::FilterOperationType type)
{
    switch (type) {
    case CSSFilterValue::ReferenceFilterOperation:
        return FilterOperation::REFERENCE;
    case CSSFilterValue::GrayscaleFilterOperation:
        return FilterOperation::GRAYSCALE;
    case CSSFilterValue::SepiaFilterOperation:
        return FilterOperation::SEPIA;
    case CSSFilterValue::SaturateFilterOperation:
        return FilterOperation::SATURATE;
    case CSSFilterValue::HueRotateFilterOperation:
        return FilterOperation::HUE_ROTATE;
    case CSSFilterValue::InvertFilterOperation:
        return FilterOperation::INVERT;
    case CSSFilterValue::OpacityFilterOperation:
        return FilterOperation::OPACITY;
    case CSSFilterValue::BrightnessFilterOperation:
        return FilterOperation::BRIGHTNESS;
    case CSSFilterValue::ContrastFilterOperation:
        return FilterOperation::CONTRAST;
    case CSSFilterValue::BlurFilterOperation:
        return FilterOperation::BLUR;
    case CSSFilterValue::DropShadowFilterOperation:
        return FilterOperation::DROP_SHADOW;
    case CSSFilterValue::CustomFilterOperation:
        return FilterOperation::CUSTOM;
    case CSSFilterValue::UnknownFilterOperation:
        return FilterOperation::NONE;
    }
    return FilterOperation::NONE;
}

static bool sortParametersByNameComparator(const RefPtr<CustomFilterParameter>& a, const RefPtr<CustomFilterParameter>& b)
{
    return codePointCompareLessThan(a->name(), b->name());
}

static StyleShader* cachedOrPendingStyleShaderFromValue(CSSShaderValue* value, StyleResolverState& state)
{
    StyleShader* shader = value->cachedOrPendingShader();
    if (shader && shader->isPendingShader())
        state.elementStyleResources().setHasPendingShaders(true);
    return shader;
}

static StyleShader* styleShader(CSSValue* value, StyleResolverState& state)
{
    if (value->isShaderValue())
        return cachedOrPendingStyleShaderFromValue(toCSSShaderValue(value), state);
    return 0;
}

static PassRefPtr<CustomFilterParameter> parseCustomFilterArrayParameter(const String& name, CSSValueList* values)
{
    RefPtr<CustomFilterArrayParameter> arrayParameter = CustomFilterArrayParameter::create(name);
    for (unsigned i = 0, length = values->length(); i < length; ++i) {
        CSSValue* value = values->itemWithoutBoundsCheck(i);
        if (!value->isPrimitiveValue())
            return 0;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return 0;
        arrayParameter->addValue(primitiveValue->getDoubleValue());
    }
    return arrayParameter.release();
}

static PassRefPtr<CustomFilterParameter> parseCustomFilterNumberParameter(const String& name, CSSValueList* values)
{
    RefPtr<CustomFilterNumberParameter> numberParameter = CustomFilterNumberParameter::create(name);
    for (unsigned i = 0; i < values->length(); ++i) {
        CSSValue* value = values->itemWithoutBoundsCheck(i);
        if (!value->isPrimitiveValue())
            return 0;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_NUMBER)
            return 0;
        numberParameter->addValue(primitiveValue->getDoubleValue());
    }
    return numberParameter.release();
}

static PassRefPtr<CustomFilterParameter> parseCustomFilterTransformParameter(const String& name, CSSValueList* values, StyleResolverState& state)
{
    RefPtr<CustomFilterTransformParameter> transformParameter = CustomFilterTransformParameter::create(name);
    TransformOperations operations;
    TransformBuilder::createTransformOperations(values, state.style(), state.rootElementStyle(), operations);
    transformParameter->setOperations(operations);
    return transformParameter.release();
}

static PassRefPtr<CustomFilterParameter> parseCustomFilterParameter(const String& name, CSSValue* parameterValue, StyleResolverState& state)
{
    // FIXME: Implement other parameters types parsing.
    // booleans: https://bugs.webkit.org/show_bug.cgi?id=76438
    // textures: https://bugs.webkit.org/show_bug.cgi?id=71442
    // mat2, mat3, mat4: https://bugs.webkit.org/show_bug.cgi?id=71444
    // Number parameters are wrapped inside a CSSValueList and all
    // the other functions values inherit from CSSValueList.
    if (!parameterValue->isValueList())
        return 0;

    CSSValueList* values = toCSSValueList(parameterValue);
    if (!values->length())
        return 0;

    if (parameterValue->isCSSArrayFunctionValue())
        return parseCustomFilterArrayParameter(name, values);

    // If the first value of the list is a transform function,
    // then we could safely assume that all the remaining items
    // are transforms. parseCustomFilterTransformParameter will
    // return 0 if that assumption is incorrect.
    if (values->itemWithoutBoundsCheck(0)->isCSSTransformValue())
        return parseCustomFilterTransformParameter(name, values, state);

    // We can have only arrays of booleans or numbers, so use the first value to choose between those two.
    // We need up to 4 values (all booleans or all numbers).
    if (!values->itemWithoutBoundsCheck(0)->isPrimitiveValue() || values->length() > 4)
        return 0;

    CSSPrimitiveValue* firstPrimitiveValue = toCSSPrimitiveValue(values->itemWithoutBoundsCheck(0));
    if (firstPrimitiveValue->primitiveType() == CSSPrimitiveValue::CSS_NUMBER)
        return parseCustomFilterNumberParameter(name, values);

    // FIXME: Implement the boolean array parameter here.
    // https://bugs.webkit.org/show_bug.cgi?id=76438

    return 0;
}

static bool parseCustomFilterParameterList(CSSValue* parametersValue, CustomFilterParameterList& parameterList, StyleResolverState& state)
{
    HashSet<String> knownParameterNames;
    CSSValueListIterator parameterIterator(parametersValue);
    for (; parameterIterator.hasMore(); parameterIterator.advance()) {
        if (!parameterIterator.value()->isValueList())
            return false;
        CSSValueListIterator iterator(parameterIterator.value());
        if (!iterator.isPrimitiveValue())
            return false;
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(iterator.value());
        if (primitiveValue->primitiveType() != CSSPrimitiveValue::CSS_STRING)
            return false;

        String name = primitiveValue->getStringValue();
        // Do not allow duplicate parameter names.
        if (!knownParameterNames.add(name).isNewEntry)
            return false;

        iterator.advance();

        if (!iterator.hasMore())
            return false;

        RefPtr<CustomFilterParameter> parameter = parseCustomFilterParameter(name, iterator.value(), state);
        if (!parameter)
            return false;
        parameterList.append(parameter.release());
    }

    // Make sure we sort the parameters before passing them down to the CustomFilterOperation.
    std::sort(parameterList.begin(), parameterList.end(), sortParametersByNameComparator);

    return true;
}

static PassRefPtr<CustomFilterOperation> createCustomFilterOperationWithAtRuleReferenceSyntax(CSSFilterValue* filterValue)
{
    // FIXME: Implement style resolution for the custom filter at-rule reference syntax.
    UNUSED_PARAM(filterValue);
    return 0;
}

static PassRefPtr<CustomFilterProgram> createCustomFilterProgram(CSSShaderValue* vertexShader, CSSShaderValue* fragmentShader,
    CustomFilterProgramType programType, const CustomFilterProgramMixSettings& mixSettings, CustomFilterMeshType meshType,
    StyleResolverState& state)
{
    ResourceFetcher* fetcher = state.document().fetcher();
    KURL vertexShaderURL = vertexShader ? vertexShader->completeURL(fetcher) : KURL();
    KURL fragmentShaderURL = fragmentShader ? fragmentShader->completeURL(fetcher) : KURL();
    RefPtr<StyleCustomFilterProgram> program = StyleCustomFilterProgram::create(vertexShaderURL, vertexShader ? styleShader(vertexShader, state) : 0,
            fragmentShaderURL, fragmentShader ? styleShader(fragmentShader, state) : 0, programType, mixSettings, meshType);
        // FIXME
        // FIXME: Find out what the fixme above means.
    return program.release();
}

static PassRefPtr<CustomFilterOperation> createCustomFilterOperationWithInlineSyntax(CSSFilterValue* filterValue, StyleResolverState& state)
{
    CSSValue* shadersValue = filterValue->itemWithoutBoundsCheck(0);
    ASSERT_WITH_SECURITY_IMPLICATION(shadersValue->isValueList());
    CSSValueList* shadersList = toCSSValueList(shadersValue);

    unsigned shadersListLength = shadersList->length();
    ASSERT(shadersListLength);

    CSSShaderValue* vertexShader = 0;
    CSSShaderValue* fragmentShader = 0;

    if (shadersList->itemWithoutBoundsCheck(0)->isShaderValue())
        vertexShader = toCSSShaderValue(shadersList->itemWithoutBoundsCheck(0));

    CustomFilterProgramType programType = PROGRAM_TYPE_BLENDS_ELEMENT_TEXTURE;
    CustomFilterProgramMixSettings mixSettings;

    if (shadersListLength > 1) {
        CSSValue* fragmentShaderOrMixFunction = shadersList->itemWithoutBoundsCheck(1);
        if (fragmentShaderOrMixFunction->isCSSMixFunctionValue()) {
            CSSMixFunctionValue* mixFunction = static_cast<CSSMixFunctionValue*>(fragmentShaderOrMixFunction);
            CSSValueListIterator iterator(mixFunction);

            ASSERT(mixFunction->length());
            if (iterator.value()->isShaderValue())
                fragmentShader = toCSSShaderValue(iterator.value());

            iterator.advance();

            ASSERT(mixFunction->length() <= 3);
            while (iterator.hasMore()) {
                CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(iterator.value());
                if (CSSParser::isBlendMode(primitiveValue->getValueID()))
                    mixSettings.blendMode = *primitiveValue;
                else if (CSSParser::isCompositeOperator(primitiveValue->getValueID()))
                    mixSettings.compositeOperator = *primitiveValue;
                else
                    ASSERT_NOT_REACHED();
                iterator.advance();
            }
        } else {
            programType = PROGRAM_TYPE_NO_ELEMENT_TEXTURE;
            if (fragmentShaderOrMixFunction->isShaderValue())
                fragmentShader = toCSSShaderValue(fragmentShaderOrMixFunction);
        }
    }

    if (!vertexShader && !fragmentShader)
        return 0;

    unsigned meshRows = 1;
    unsigned meshColumns = 1;
    CustomFilterMeshType meshType = MeshTypeAttached;

    CSSValue* parametersValue = 0;

    if (filterValue->length() > 1) {
        CSSValueListIterator iterator(filterValue->itemWithoutBoundsCheck(1));

        // The second value might be the mesh box or the list of parameters:
        // If it starts with a number or any of the mesh-box identifiers it is
        // the mesh-box list, if not it means it is the parameters list.

        if (iterator.hasMore() && iterator.isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(iterator.value());
            if (primitiveValue->isNumber()) {
                // If only one integer value is specified, it will set both
                // the rows and the columns.
                meshColumns = meshRows = primitiveValue->getIntValue();
                iterator.advance();

                // Try to match another number for the rows.
                if (iterator.hasMore() && iterator.isPrimitiveValue()) {
                    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(iterator.value());
                    if (primitiveValue->isNumber()) {
                        meshRows = primitiveValue->getIntValue();
                        iterator.advance();
                    }
                }
            }
        }

        if (iterator.hasMore() && iterator.isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(iterator.value());
            if (primitiveValue->getValueID() == CSSValueDetached) {
                meshType = MeshTypeDetached;
                iterator.advance();
            }
        }

        if (!iterator.index()) {
            // If no value was consumed from the mesh value, then it is just a parameter list, meaning that we end up
            // having just two CSSListValues: list of shaders and list of parameters.
            ASSERT(filterValue->length() == 2);
            parametersValue = filterValue->itemWithoutBoundsCheck(1);
        }
    }

    if (filterValue->length() > 2 && !parametersValue)
        parametersValue = filterValue->itemWithoutBoundsCheck(2);

    CustomFilterParameterList parameterList;
    if (parametersValue && !parseCustomFilterParameterList(parametersValue, parameterList, state))
        return 0;

    RefPtr<CustomFilterProgram> program = createCustomFilterProgram(vertexShader, fragmentShader, programType, mixSettings, meshType, state);
    return CustomFilterOperation::create(program.release(), parameterList, meshRows, meshColumns, meshType);
}

static PassRefPtr<CustomFilterOperation> createCustomFilterOperation(CSSFilterValue* filterValue, StyleResolverState& state)
{
    ASSERT(filterValue->length());
    bool isAtRuleReferenceSyntax = filterValue->itemWithoutBoundsCheck(0)->isPrimitiveValue();
    return isAtRuleReferenceSyntax ? createCustomFilterOperationWithAtRuleReferenceSyntax(filterValue) : createCustomFilterOperationWithInlineSyntax(filterValue, state);
}


bool FilterOperationResolver::createFilterOperations(CSSValue* inValue, const RenderStyle* style, const RenderStyle* rootStyle, FilterOperations& outOperations, StyleResolverState& state)
{
    ASSERT(outOperations.isEmpty());

    if (!inValue)
        return false;

    if (inValue->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(inValue);
        if (primitiveValue->getValueID() == CSSValueNone)
            return true;
    }

    if (!inValue->isValueList())
        return false;

    float zoomFactor = (style ? style->effectiveZoom() : 1) * state.elementStyleResources().deviceScaleFactor();
    FilterOperations operations;
    for (CSSValueListIterator i = inValue; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (!currValue->isCSSFilterValue())
            continue;

        CSSFilterValue* filterValue = static_cast<CSSFilterValue*>(i.value());
        FilterOperation::OperationType operationType = filterOperationForType(filterValue->operationType());

        if (operationType == FilterOperation::VALIDATED_CUSTOM) {
            // ValidatedCustomFilterOperation is not supposed to end up in the RenderStyle.
            ASSERT_NOT_REACHED();
            continue;
        }
        if (operationType == FilterOperation::CUSTOM) {
            RefPtr<CustomFilterOperation> operation = createCustomFilterOperation(filterValue, state);
            if (!operation)
                return false;

            operations.operations().append(operation);
            continue;
        }
        if (operationType == FilterOperation::REFERENCE) {
            if (filterValue->length() != 1)
                continue;
            CSSValue* argument = filterValue->itemWithoutBoundsCheck(0);

            if (!argument->isSVGDocumentValue())
                continue;

            CSSSVGDocumentValue* svgDocumentValue = toCSSSVGDocumentValue(argument);
            KURL url = state.document().completeURL(svgDocumentValue->url());

            RefPtr<ReferenceFilterOperation> operation = ReferenceFilterOperation::create(svgDocumentValue->url(), url.fragmentIdentifier(), operationType);
            if (SVGURIReference::isExternalURIReference(svgDocumentValue->url(), state.document())) {
                if (!svgDocumentValue->loadRequested())
                    state.elementStyleResources().addPendingSVGDocument(operation.get(), svgDocumentValue);
                else if (svgDocumentValue->cachedSVGDocument())
                    operation->setDocumentResourceReference(adoptPtr(new DocumentResourceReference(svgDocumentValue->cachedSVGDocument())));
            }
            operations.operations().append(operation);
            continue;
        }

        // Check that all parameters are primitive values, with the
        // exception of drop shadow which has a ShadowValue parameter.
        if (operationType != FilterOperation::DROP_SHADOW) {
            bool haveNonPrimitiveValue = false;
            for (unsigned j = 0; j < filterValue->length(); ++j) {
                if (!filterValue->itemWithoutBoundsCheck(j)->isPrimitiveValue()) {
                    haveNonPrimitiveValue = true;
                    break;
                }
            }
            if (haveNonPrimitiveValue)
                continue;
        }

        CSSPrimitiveValue* firstValue = filterValue->length() && filterValue->itemWithoutBoundsCheck(0)->isPrimitiveValue() ? toCSSPrimitiveValue(filterValue->itemWithoutBoundsCheck(0)) : 0;
        switch (filterValue->operationType()) {
        case CSSFilterValue::GrayscaleFilterOperation:
        case CSSFilterValue::SepiaFilterOperation:
        case CSSFilterValue::SaturateFilterOperation: {
            double amount = 1;
            if (filterValue->length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicColorMatrixFilterOperation::create(amount, operationType));
            break;
        }
        case CSSFilterValue::HueRotateFilterOperation: {
            double angle = 0;
            if (filterValue->length() == 1)
                angle = firstValue->computeDegrees();

            operations.operations().append(BasicColorMatrixFilterOperation::create(angle, operationType));
            break;
        }
        case CSSFilterValue::InvertFilterOperation:
        case CSSFilterValue::BrightnessFilterOperation:
        case CSSFilterValue::ContrastFilterOperation:
        case CSSFilterValue::OpacityFilterOperation: {
            double amount = (filterValue->operationType() == CSSFilterValue::BrightnessFilterOperation) ? 0 : 1;
            if (filterValue->length() == 1) {
                amount = firstValue->getDoubleValue();
                if (firstValue->isPercentage())
                    amount /= 100;
            }

            operations.operations().append(BasicComponentTransferFilterOperation::create(amount, operationType));
            break;
        }
        case CSSFilterValue::BlurFilterOperation: {
            Length stdDeviation = Length(0, Fixed);
            if (filterValue->length() >= 1)
                stdDeviation = convertToFloatLength(firstValue, style, rootStyle, zoomFactor);
            if (stdDeviation.isUndefined())
                return false;

            operations.operations().append(BlurFilterOperation::create(stdDeviation, operationType));
            break;
        }
        case CSSFilterValue::DropShadowFilterOperation: {
            if (filterValue->length() != 1)
                return false;

            CSSValue* cssValue = filterValue->itemWithoutBoundsCheck(0);
            if (!cssValue->isShadowValue())
                continue;

            ShadowValue* item = static_cast<ShadowValue*>(cssValue);
            IntPoint location(item->x->computeLength<int>(style, rootStyle, zoomFactor), item->y->computeLength<int>(style, rootStyle, zoomFactor));
            int blur = item->blur ? item->blur->computeLength<int>(style, rootStyle, zoomFactor) : 0;
            Color shadowColor;
            if (item->color)
                shadowColor = state.document().textLinkColors().colorFromPrimitiveValue(item->color.get(), state.style()->visitedDependentColor(CSSPropertyColor));

            operations.operations().append(DropShadowFilterOperation::create(location, blur, shadowColor.isValid() ? shadowColor : Color::transparent, operationType));
            break;
        }
        case CSSFilterValue::UnknownFilterOperation:
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    outOperations = operations;
    return true;
}

} // namespace WebCore
