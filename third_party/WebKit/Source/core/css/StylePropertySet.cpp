/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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
#include "core/css/StylePropertySet.h"

#include "CSSValueKeywords.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePool.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySerializer.h"
#include "core/css/StylePropertyShorthand.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/page/RuntimeCSSEnabled.h"
#include <wtf/BitArray.h>
#include <wtf/MemoryInstrumentationVector.h>
#include <wtf/text/StringBuilder.h>

#ifndef NDEBUG
#include <stdio.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/CString.h>
#endif

using namespace std;

namespace WebCore {

typedef HashMap<const StylePropertySet*, OwnPtr<PropertySetCSSStyleDeclaration> > PropertySetCSSOMWrapperMap;
static PropertySetCSSOMWrapperMap& propertySetCSSOMWrapperMap()
{
    DEFINE_STATIC_LOCAL(PropertySetCSSOMWrapperMap, propertySetCSSOMWrapperMapInstance, ());
    return propertySetCSSOMWrapperMapInstance;
}

static size_t sizeForImmutableStylePropertySetWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStylePropertySet) - sizeof(void*) + sizeof(CSSValue*) * count + sizeof(StylePropertyMetadata) * count;
}

PassRefPtr<StylePropertySet> StylePropertySet::createImmutable(const CSSProperty* properties, unsigned count, CSSParserMode cssParserMode)
{
    void* slot = WTF::fastMalloc(sizeForImmutableStylePropertySetWithPropertyCount(count));
    return adoptRef(new (slot) ImmutableStylePropertySet(properties, count, cssParserMode));
}

PassRefPtr<StylePropertySet> StylePropertySet::immutableCopyIfNeeded() const
{
    if (!isMutable())
        return const_cast<StylePropertySet*>(this);
    return createImmutable(mutablePropertyVector().data(), mutablePropertyVector().size(), cssParserMode());
}

MutableStylePropertySet::MutableStylePropertySet(const CSSProperty* properties, unsigned length)
    : StylePropertySet(CSSStrictMode)
{
    m_propertyVector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_propertyVector.uncheckedAppend(properties[i]);
}

ImmutableStylePropertySet::ImmutableStylePropertySet(const CSSProperty* properties, unsigned length, CSSParserMode cssParserMode)
    : StylePropertySet(cssParserMode, length)
{
    StylePropertyMetadata* metadataArray = const_cast<StylePropertyMetadata*>(immutableMetadataArray());
    CSSValue** valueArray = const_cast<CSSValue**>(immutableValueArray());
    for (unsigned i = 0; i < length; ++i) {
        metadataArray[i] = properties[i].metadata();
        valueArray[i] = properties[i].value();
        valueArray[i]->ref();
    }
}

ImmutableStylePropertySet::~ImmutableStylePropertySet()
{
    CSSValue** valueArray = const_cast<CSSValue**>(immutableValueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

MutableStylePropertySet::MutableStylePropertySet(const StylePropertySet& other)
    : StylePropertySet(other.cssParserMode())
{
    if (other.isMutable())
        m_propertyVector = static_cast<const MutableStylePropertySet&>(other).mutablePropertyVector();
    else {
        m_propertyVector.reserveInitialCapacity(other.propertyCount());
        for (unsigned i = 0; i < other.propertyCount(); ++i)
            m_propertyVector.uncheckedAppend(other.propertyAt(i).toCSSProperty());
    }
}

StylePropertySet::~StylePropertySet()
{
    ASSERT(!m_ownsCSSOMWrapper || propertySetCSSOMWrapperMap().contains(this));
    if (m_ownsCSSOMWrapper)
        propertySetCSSOMWrapperMap().remove(this);
}

String StylePropertySet::getPropertyValue(CSSPropertyID propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    return StylePropertySerializer(*this).getPropertyValue(propertyID);
}

PassRefPtr<CSSValue> StylePropertySet::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return 0;
    return propertyAt(foundPropertyIndex).value();
}

bool StylePropertySet::removeShorthandProperty(CSSPropertyID propertyID)
{
    ASSERT(isMutable());
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    bool ret = removePropertiesInSet(shorthand.properties(), shorthand.length());

    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(propertyID);
    if (prefixingVariant == propertyID)
        return ret;

    StylePropertyShorthand shorthandPrefixingVariant = shorthandForProperty(prefixingVariant);
    return removePropertiesInSet(shorthandPrefixingVariant.properties(), shorthandPrefixingVariant.length());
}

bool StylePropertySet::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    ASSERT(isMutable());
    if (removeShorthandProperty(propertyID)) {
        // FIXME: Return an equivalent shorthand when possible.
        if (returnText)
            *returnText = "";
        return true;
    }

    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1) {
        if (returnText)
            *returnText = "";
        return false;
    }

    if (returnText)
        *returnText = propertyAt(foundPropertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    mutablePropertyVector().remove(foundPropertyIndex);

    removePrefixedOrUnprefixedProperty(propertyID);

    return true;
}

void StylePropertySet::removePrefixedOrUnprefixedProperty(CSSPropertyID propertyID)
{
    int foundPropertyIndex = findPropertyIndex(prefixingVariantForPropertyId(propertyID));
    if (foundPropertyIndex == -1)
        return;
    mutablePropertyVector().remove(foundPropertyIndex);
}

bool StylePropertySet::propertyIsImportant(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();

    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!propertyIsImportant(shorthand.properties()[i]))
            return false;
    }
    return true;
}

CSSPropertyID StylePropertySet::getPropertyShorthand(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return CSSPropertyInvalid;
    return propertyAt(foundPropertyIndex).shorthandID();
}

bool StylePropertySet::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

bool StylePropertySet::setProperty(CSSPropertyID propertyID, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    ASSERT(isMutable());
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeProperty(propertyID);

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseValue(this, propertyID, value, important, cssParserMode(), contextStyleSheet);
}

void StylePropertySet::setProperty(CSSPropertyID propertyID, PassRefPtr<CSSValue> prpValue, bool important)
{
    ASSERT(isMutable());
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        setProperty(CSSProperty(propertyID, prpValue, important));
        return;
    }

    removePropertiesInSet(shorthand.properties(), shorthand.length());

    RefPtr<CSSValue> value = prpValue;
    for (unsigned i = 0; i < shorthand.length(); ++i)
        mutablePropertyVector().append(CSSProperty(shorthand.properties()[i], value, important));
}

void StylePropertySet::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    ASSERT(isMutable());
    if (!removeShorthandProperty(property.id())) {
        CSSProperty* toReplace = slot ? slot : findMutableCSSPropertyWithID(property.id());
        if (toReplace) {
            *toReplace = property;
            setPrefixingVariantProperty(property);
            return;
        }
    }
    appendPrefixingVariantProperty(property);
}

void StylePropertySet::appendPrefixingVariantProperty(const CSSProperty& property)
{
    mutablePropertyVector().append(property);
    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(property.id());
    if (prefixingVariant == property.id())
        return;
    mutablePropertyVector().append(CSSProperty(prefixingVariant, property.value(), property.isImportant(), property.shorthandID(), property.metadata().m_implicit));
}

void StylePropertySet::setPrefixingVariantProperty(const CSSProperty& property)
{
    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(property.id());
    CSSProperty* toReplace = findMutableCSSPropertyWithID(prefixingVariant);
    if (toReplace)
        *toReplace = CSSProperty(prefixingVariant, property.value(), property.isImportant(), property.shorthandID(), property.metadata().m_implicit);
}

bool StylePropertySet::setProperty(CSSPropertyID propertyID, int identifier, bool important)
{
    ASSERT(isMutable());
    setProperty(CSSProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important));
    return true;
}

void StylePropertySet::parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet)
{
    ASSERT(isMutable());

    mutablePropertyVector().clear();

    CSSParserContext context(cssParserMode());
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.mode = cssParserMode();
    }
    CSSParser parser(context);
    parser.parseDeclaration(this, styleDeclaration, 0, contextStyleSheet);
}

void StylePropertySet::addParsedProperties(const Vector<CSSProperty>& properties)
{
    ASSERT(isMutable());
    mutablePropertyVector().reserveCapacity(mutablePropertyVector().size() + properties.size());
    for (unsigned i = 0; i < properties.size(); ++i)
        addParsedProperty(properties[i]);
}

void StylePropertySet::addParsedProperty(const CSSProperty& property)
{
    ASSERT(isMutable());
    // Only add properties that have no !important counterpart present
    if (!propertyIsImportant(property.id()) || property.isImportant())
        setProperty(property);
}

String StylePropertySet::asText() const
{
    return StylePropertySerializer(*this).asText();
}

void StylePropertySet::mergeAndOverrideOnConflict(const StylePropertySet* other)
{
    ASSERT(isMutable());
    unsigned size = other->propertyCount();
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference toMerge = other->propertyAt(n);
        CSSProperty* old = findMutableCSSPropertyWithID(toMerge.id());
        if (old)
            setProperty(toMerge.toCSSProperty(), old);
        else
            appendPrefixingVariantProperty(toMerge.toCSSProperty());
    }
}

void StylePropertySet::addSubresourceStyleURLs(ListHashSet<KURL>& urls, StyleSheetContents* contextStyleSheet) const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i)
        propertyAt(i).value()->addSubresourceStyleURLs(urls, contextStyleSheet);
}

bool StylePropertySet::hasFailedOrCanceledSubresources() const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i) {
        if (propertyAt(i).value()->hasFailedOrCanceledSubresources())
            return true;
    }
    return false;
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const CSSPropertyID staticBlockProperties[] = {
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
    CSSPropertyWebkitAspectRatio,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnWidth,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyWebkitRegionBreakAfter,
    CSSPropertyWebkitRegionBreakBefore,
    CSSPropertyWebkitRegionBreakInside,
    CSSPropertyTextAlign,
#if ENABLE(CSS3_TEXT)
    CSSPropertyWebkitTextAlignLast,
#endif // CSS3_TEXT
    CSSPropertyTextIndent,
    CSSPropertyWidows
};

static const Vector<CSSPropertyID>& blockProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    if (properties.isEmpty())
        RuntimeCSSEnabled::filterEnabledCSSPropertiesIntoVector(staticBlockProperties, WTF_ARRAY_LENGTH(staticBlockProperties), properties);
    return properties;
}

void StylePropertySet::clear()
{
    ASSERT(isMutable());
    mutablePropertyVector().clear();
}

PassRefPtr<StylePropertySet> StylePropertySet::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties());
}

void StylePropertySet::removeBlockProperties()
{
    removePropertiesInSet(blockProperties().data(), blockProperties().size());
}

bool StylePropertySet::removePropertiesInSet(const CSSPropertyID* set, unsigned length)
{
    ASSERT(isMutable());
    if (mutablePropertyVector().isEmpty())
        return false;

    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<CSSPropertyID> toRemove;
    for (unsigned i = 0; i < length; ++i)
        toRemove.add(set[i]);

    Vector<CSSProperty> newProperties;
    newProperties.reserveInitialCapacity(mutablePropertyVector().size());

    unsigned size = mutablePropertyVector().size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& property = mutablePropertyVector().at(n);
        // Not quite sure if the isImportant test is needed but it matches the existing behavior.
        if (!property.isImportant()) {
            if (toRemove.contains(property.id()))
                continue;
        }
        newProperties.append(property);
    }

    bool changed = newProperties.size() != mutablePropertyVector().size();
    mutablePropertyVector() = newProperties;
    return changed;
}

int StylePropertySet::findPropertyIndex(CSSPropertyID propertyID) const
{
    for (int n = propertyCount() - 1 ; n >= 0; --n) {
        if (propertyID == propertyAt(n).id()) {
            // Only enabled properties should be part of the style.
            ASSERT(RuntimeCSSEnabled::isCSSPropertyEnabled(propertyID));
            return n;
        }
    }
    return -1;
}

CSSProperty* StylePropertySet::findMutableCSSPropertyWithID(CSSPropertyID propertyID)
{
    ASSERT(isMutable());
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return 0;
    return &mutablePropertyVector().at(foundPropertyIndex);
}
    
bool StylePropertySet::propertyMatches(CSSPropertyID propertyID, const CSSValue* propertyValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).value()->equals(*propertyValue);
}
    
void StylePropertySet::removeEquivalentProperties(const StylePropertySet* style)
{
    ASSERT(isMutable());
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = mutablePropertyVector().size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->propertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }    
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

void StylePropertySet::removeEquivalentProperties(const CSSStyleDeclaration* style)
{
    ASSERT(isMutable());
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = mutablePropertyVector().size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->cssPropertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }    
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

PassRefPtr<StylePropertySet> StylePropertySet::copy() const
{
    return adoptRef(new MutableStylePropertySet(*this));
}

PassRefPtr<StylePropertySet> StylePropertySet::copyPropertiesInSet(const Vector<CSSPropertyID>& properties) const
{
    Vector<CSSProperty, 256> list;
    list.reserveInitialCapacity(properties.size());
    for (unsigned i = 0; i < properties.size(); ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
        if (value)
            list.append(CSSProperty(properties[i], value.release(), false));
    }
    return StylePropertySet::create(list.data(), list.size());
}

PropertySetCSSStyleDeclaration* StylePropertySet::cssStyleDeclaration()
{
    if (!m_ownsCSSOMWrapper)
        return 0;
    ASSERT(isMutable());
    return propertySetCSSOMWrapperMap().get(this);
}

CSSStyleDeclaration* StylePropertySet::ensureCSSStyleDeclaration()
{
    ASSERT(isMutable());

    if (m_ownsCSSOMWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(propertySetCSSOMWrapperMap().get(this))->parentRule());
        ASSERT(!propertySetCSSOMWrapperMap().get(this)->parentElement());
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_ownsCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new PropertySetCSSStyleDeclaration(const_cast<StylePropertySet*>(this));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

CSSStyleDeclaration* StylePropertySet::ensureInlineCSSStyleDeclaration(const StyledElement* parentElement)
{
    ASSERT(isMutable());

    if (m_ownsCSSOMWrapper) {
        ASSERT(propertySetCSSOMWrapperMap().get(this)->parentElement() == parentElement);
        return propertySetCSSOMWrapperMap().get(this);
    }
    m_ownsCSSOMWrapper = true;
    PropertySetCSSStyleDeclaration* cssomWrapper = new InlineCSSStyleDeclaration(const_cast<StylePropertySet*>(this), const_cast<StyledElement*>(parentElement));
    propertySetCSSOMWrapperMap().add(this, adoptPtr(cssomWrapper));
    return cssomWrapper;
}

unsigned StylePropertySet::averageSizeInBytes()
{
    // Please update this if the storage scheme changes so that this longer reflects the actual size.
    return sizeForImmutableStylePropertySetWithPropertyCount(4);
}

void StylePropertySet::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    size_t actualSize = m_isMutable ? sizeof(StylePropertySet) : sizeForImmutableStylePropertySetWithPropertyCount(m_arraySize);
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS, actualSize);
    if (m_isMutable)
        info.addMember(mutablePropertyVector(), "mutablePropertyVector()");
    else {
        for (unsigned i = 0; i < propertyCount(); ++i)
            info.addMember(propertyAt(i).value(), "value");
    }
}

// See the function above if you need to update this.
struct SameSizeAsStylePropertySet : public RefCounted<SameSizeAsStylePropertySet> {
    unsigned bitfield;
};
COMPILE_ASSERT(sizeof(StylePropertySet) == sizeof(SameSizeAsStylePropertySet), style_property_set_should_stay_small);

#ifndef NDEBUG
void StylePropertySet::showStyle()
{
    fprintf(stderr, "%s\n", asText().ascii().data());
}
#endif

PassRefPtr<StylePropertySet> StylePropertySet::create(CSSParserMode cssParserMode)
{
    return adoptRef(new MutableStylePropertySet(cssParserMode));
}

PassRefPtr<StylePropertySet> StylePropertySet::create(const CSSProperty* properties, unsigned count)
{
    return adoptRef(new MutableStylePropertySet(properties, count));
}

String StylePropertySet::PropertyReference::cssName() const
{
    if (id() == CSSPropertyVariable) {
        ASSERT(propertyValue()->isVariableValue());
        if (!propertyValue()->isVariableValue())
            return emptyString(); // Should not happen, but if it does, avoid a bad cast.
        return "-webkit-var-" + static_cast<const CSSVariableValue*>(propertyValue())->name();
    }
    return getPropertyNameString(id());
}

String StylePropertySet::PropertyReference::cssText() const
{
    StringBuilder result;
    result.append(cssName());
    result.appendLiteral(": ");
    result.append(propertyValue()->cssText());
    if (isImportant())
        result.appendLiteral(" !important");
    result.append(';');
    return result.toString();
}


} // namespace WebCore
