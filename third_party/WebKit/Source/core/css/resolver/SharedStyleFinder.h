/*
 * Copyright (C) 2013 Google, Inc.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef SharedStyleFinder_h
#define SharedStyleFinder_h

namespace WebCore {

class Element;
class ElementResolveContext;
class Node;
class RenderStyle;
class RuleFeatureSet;
class RuleSet;
class SpaceSplitString;
class StyleResolver;

class SharedStyleFinder {
public:
    // FIXME: StyleResolver* only used for calling styleSharingCandidateMatchesRuleSet.
    // RuleSets are passed non-const as the act of matching against them can cause them
    // to be compacted. :(
    SharedStyleFinder(const RuleFeatureSet& features, RuleSet* siblingRuleSet,
        RuleSet* uncommonAttributeRuleSet, StyleResolver* styleResolver)
        : m_elementAffectedByClassRules(false)
        , m_features(features)
        , m_siblingRuleSet(siblingRuleSet)
        , m_uncommonAttributeRuleSet(uncommonAttributeRuleSet)
        , m_styleResolver(styleResolver)
    { }

    // FIXME: It is not necessarily safe to call this method more than once.
    RenderStyle* locateSharedStyle(const ElementResolveContext&, RenderStyle* newStyle);

private:
    Element* findElementForStyleSharing(const ElementResolveContext&) const;

    // Only used when we're collecting stats on styles
    Element* searchDocumentForSharedStyle(const ElementResolveContext&) const;

    bool classNamesAffectedByRules(const SpaceSplitString&) const;

    bool canShareStyleWithElement(const ElementResolveContext&, Element*) const;
    bool canShareStyleWithControl(const ElementResolveContext&, Element*) const;
    bool sharingCandidateHasIdenticalStyleAffectingAttributes(const ElementResolveContext&, Element*) const;

    bool m_elementAffectedByClassRules;
    const RuleFeatureSet& m_features;
    RuleSet* m_siblingRuleSet;
    RuleSet* m_uncommonAttributeRuleSet;
    StyleResolver* m_styleResolver;
};

} // namespace WebCore

#endif // SharedStyleFinder_h
