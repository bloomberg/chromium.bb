/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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
#include "core/css/StyleRule.h"

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSCharsetRule.h"
#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSHostRule.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSPageRule.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSSupportsRule.h"
#include "core/css/CSSUnknownRule.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/WebKitCSSFilterRule.h"
#include "core/css/WebKitCSSKeyframeRule.h"
#include "core/css/WebKitCSSKeyframesRule.h"
#include "core/css/WebKitCSSRegionRule.h"
#include "core/css/WebKitCSSViewportRule.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include <wtf/MemoryInstrumentationVector.h>

namespace WebCore {

struct SameSizeAsStyleRuleBase : public WTF::RefCountedBase {
    unsigned bitfields;
};

COMPILE_ASSERT(sizeof(StyleRuleBase) == sizeof(SameSizeAsStyleRuleBase), StyleRuleBase_should_stay_small);

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet) const
{
    return createCSSOMWrapper(parentSheet, 0);
}

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSRule* parentRule) const
{ 
    return createCSSOMWrapper(0, parentRule);
}

void StyleRuleBase::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    switch (type()) {
    case Style:
        static_cast<const StyleRule*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Page:
        static_cast<const StyleRulePage*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case FontFace:
        static_cast<const StyleRuleFontFace*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Media:
        static_cast<const StyleRuleMedia*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Region:
        static_cast<const StyleRuleRegion*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Import:
        static_cast<const StyleRuleImport*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Keyframes:
        static_cast<const StyleRuleKeyframes*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Supports:
    case HostInternal:
        static_cast<const StyleRuleHost*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        static_cast<const StyleRuleViewport*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
#endif
    case Filter:
        static_cast<const StyleRuleFilter*>(this)->reportDescendantMemoryUsage(memoryObjectInfo);
        return;
    case Unknown:
    case Charset:
    case Keyframe:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

void StyleRuleBase::destroy()
{
    switch (type()) {
    case Style:
        delete static_cast<StyleRule*>(this);
        return;
    case Page:
        delete static_cast<StyleRulePage*>(this);
        return;
    case FontFace:
        delete static_cast<StyleRuleFontFace*>(this);
        return;
    case Media:
        delete static_cast<StyleRuleMedia*>(this);
        return;
    case Supports:
        delete static_cast<StyleRuleSupports*>(this);
        return;
    case Region:
        delete static_cast<StyleRuleRegion*>(this);
        return;
    case Import:
        delete static_cast<StyleRuleImport*>(this);
        return;
    case Keyframes:
        delete static_cast<StyleRuleKeyframes*>(this);
        return;
    case HostInternal:
        delete static_cast<StyleRuleHost*>(this);
        return;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        delete static_cast<StyleRuleViewport*>(this);
        return;
#endif
    case Filter:
        delete static_cast<StyleRuleFilter*>(this);
        return;
    case Unknown:
    case Charset:
    case Keyframe:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

PassRefPtr<StyleRuleBase> StyleRuleBase::copy() const
{
    switch (type()) {
    case Style:
        return static_cast<const StyleRule*>(this)->copy();
    case Page:
        return static_cast<const StyleRulePage*>(this)->copy();
    case FontFace:
        return static_cast<const StyleRuleFontFace*>(this)->copy();
    case Media:
        return static_cast<const StyleRuleMedia*>(this)->copy();
    case Supports:
        return static_cast<const StyleRuleSupports*>(this)->copy();
    case Region:
        return static_cast<const StyleRuleRegion*>(this)->copy();
    case Import:
        // FIXME: Copy import rules.
        ASSERT_NOT_REACHED();
        return 0;
    case Keyframes:
        return static_cast<const StyleRuleKeyframes*>(this)->copy();
    case HostInternal:
        return static_cast<const StyleRuleHost*>(this)->copy();
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        return static_cast<const StyleRuleViewport*>(this)->copy();
#endif
    case Filter:
        return static_cast<const StyleRuleFilter*>(this)->copy();
    case Unknown:
    case Charset:
    case Keyframe:
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<CSSRule> StyleRuleBase::createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const
{
    RefPtr<CSSRule> rule;
    StyleRuleBase* self = const_cast<StyleRuleBase*>(this);
    switch (type()) {
    case Style:
        rule = CSSStyleRule::create(static_cast<StyleRule*>(self), parentSheet);
        break;
    case Page:
        rule = CSSPageRule::create(static_cast<StyleRulePage*>(self), parentSheet);
        break;
    case FontFace:
        rule = CSSFontFaceRule::create(static_cast<StyleRuleFontFace*>(self), parentSheet);
        break;
    case Media:
        rule = CSSMediaRule::create(static_cast<StyleRuleMedia*>(self), parentSheet);
        break;
    case Supports:
        rule = CSSSupportsRule::create(static_cast<StyleRuleSupports*>(self), parentSheet);
        break;
    case Region:
        rule = WebKitCSSRegionRule::create(static_cast<StyleRuleRegion*>(self), parentSheet);
        break;
    case Import:
        rule = CSSImportRule::create(static_cast<StyleRuleImport*>(self), parentSheet);
        break;
    case Keyframes:
        rule = WebKitCSSKeyframesRule::create(static_cast<StyleRuleKeyframes*>(self), parentSheet);
        break;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    case Viewport:
        rule = WebKitCSSViewportRule::create(static_cast<StyleRuleViewport*>(self), parentSheet);
        break;
#endif
    case HostInternal:
        rule = CSSHostRule::create(static_cast<StyleRuleHost*>(self), parentSheet);
        break;
    case Filter:
        rule = WebKitCSSFilterRule::create(static_cast<StyleRuleFilter*>(self), parentSheet);
        break;
    case Unknown:
    case Charset:
    case Keyframe:
        ASSERT_NOT_REACHED();
        return 0;
    }
    if (parentRule)
        rule->setParentRule(parentRule);
    return rule.release();
}

unsigned StyleRule::averageSizeInBytes()
{
    return sizeof(StyleRule) + sizeof(CSSSelector) + StylePropertySet::averageSizeInBytes();
}

void StyleRule::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_properties, "properties");
    info.addMember(m_selectorList, "selectorList");
}

StyleRule::StyleRule(int sourceLine)
    : StyleRuleBase(Style, sourceLine)
{
}

StyleRule::StyleRule(const StyleRule& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRule::~StyleRule()
{
}

StylePropertySet* StyleRule::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRule::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

StyleRulePage::StyleRulePage()
    : StyleRuleBase(Page)
{
}

StyleRulePage::StyleRulePage(const StyleRulePage& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
    , m_selectorList(o.m_selectorList)
{
}

StyleRulePage::~StyleRulePage()
{
}

StylePropertySet* StyleRulePage::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRulePage::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

void StyleRulePage::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_properties, "properties");
    info.addMember(m_selectorList, "selectorList");
}

StyleRuleFontFace::StyleRuleFontFace()
    : StyleRuleBase(FontFace, 0)
{
}

StyleRuleFontFace::StyleRuleFontFace(const StyleRuleFontFace& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
{
}

StyleRuleFontFace::~StyleRuleFontFace()
{
}

StylePropertySet* StyleRuleFontFace::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRuleFontFace::setProperties(PassRefPtr<StylePropertySet> properties)
{ 
    m_properties = properties;
}

void StyleRuleFontFace::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_properties, "properties");
}


StyleRuleGroup::StyleRuleGroup(Type type, Vector<RefPtr<StyleRuleBase> >& adoptRule)
    : StyleRuleBase(type, 0)
{
    m_childRules.swap(adoptRule);
}

StyleRuleGroup::StyleRuleGroup(const StyleRuleGroup& o)
    : StyleRuleBase(o)
    , m_childRules(o.m_childRules.size())
{
    for (unsigned i = 0; i < m_childRules.size(); ++i)
        m_childRules[i] = o.m_childRules[i]->copy();
}

void StyleRuleGroup::wrapperInsertRule(unsigned index, PassRefPtr<StyleRuleBase> rule)
{
    m_childRules.insert(index, rule);
}
    
void StyleRuleGroup::wrapperRemoveRule(unsigned index)
{
    m_childRules.remove(index);
}

void StyleRuleGroup::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_childRules, "childRules");
}

StyleRuleMedia::StyleRuleMedia(PassRefPtr<MediaQuerySet> media, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleGroup(Media, adoptRules)
    , m_mediaQueries(media)
{
}

StyleRuleMedia::StyleRuleMedia(const StyleRuleMedia& o)
    : StyleRuleGroup(o)
{
    if (o.m_mediaQueries)
        m_mediaQueries = o.m_mediaQueries->copy();
}

void StyleRuleMedia::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_mediaQueries, "mediaQueries");
}


StyleRuleSupports::StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleGroup(Supports, adoptRules)
    , m_conditionText(conditionText)
    , m_conditionIsSupported(conditionIsSupported)
{
}

StyleRuleSupports::StyleRuleSupports(const StyleRuleSupports& o)
    : StyleRuleGroup(o)
    , m_conditionText(o.m_conditionText)
    , m_conditionIsSupported(o.m_conditionIsSupported)
{
}

StyleRuleRegion::StyleRuleRegion(Vector<OwnPtr<CSSParserSelector> >* selectors, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    : StyleRuleGroup(Region, adoptRules)
{
    ASSERT(RuntimeEnabledFeatures::cssRegionsEnabled());
    m_selectorList.adoptSelectorVector(*selectors);
}

StyleRuleRegion::StyleRuleRegion(const StyleRuleRegion& o)
    : StyleRuleGroup(o)
    , m_selectorList(o.m_selectorList)
{
    ASSERT(RuntimeEnabledFeatures::cssRegionsEnabled());
}

void StyleRuleRegion::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_selectorList, "selectorList");
}

#if ENABLE(CSS_DEVICE_ADAPTATION)
StyleRuleViewport::StyleRuleViewport()
    : StyleRuleBase(Viewport, 0)
{
}

StyleRuleViewport::StyleRuleViewport(const StyleRuleViewport& o)
    : StyleRuleBase(o)
    , m_properties(o.m_properties->copy())
{
}

StyleRuleViewport::~StyleRuleViewport()
{
}

StylePropertySet* StyleRuleViewport::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRuleViewport::setProperties(PassRefPtr<StylePropertySet> properties)
{
    m_properties = properties;
}

void StyleRuleViewport::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_properties, "properties");
}
#endif // ENABLE(CSS_DEVICE_ADAPTATION)

StyleRuleFilter::StyleRuleFilter(const String& filterName)
    : StyleRuleBase(Filter, 0)
    , m_filterName(filterName)
{
}

StyleRuleFilter::StyleRuleFilter(const StyleRuleFilter& o)
    : StyleRuleBase(o)
    , m_filterName(o.m_filterName)
    , m_properties(o.m_properties->copy())
{
}

StyleRuleFilter::~StyleRuleFilter()
{
}

StylePropertySet* StyleRuleFilter::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->copy();
    return m_properties.get();
}

void StyleRuleFilter::setProperties(PassRefPtr<StylePropertySet> properties)
{
    m_properties = properties;
}

void StyleRuleFilter::reportDescendantMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_filterName);
    info.addMember(m_properties);
}

} // namespace WebCore
