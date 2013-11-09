/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef DocumentRuleSets_h
#define DocumentRuleSets_h

#include "core/css/RuleSet.h"
#include "core/css/TreeBoundaryCrossingRules.h"

#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSStyleSheet;
class MediaQueryEvaluator;
class RuleFeatureSet;
class StyleEngine;

// FIXME: This class doesn't really serve a purpose anymore. Merge it into StyleResolver.
class DocumentRuleSets {
public:
    DocumentRuleSets();
    ~DocumentRuleSets();

    // FIXME: watched selectors should be implemented using injected author stylesheets: http://crbug.com/316960
    RuleSet* watchedSelectorRules() const { return m_watchedSelectorsRules.get(); }
    void initWatchedSelectorRules(const Vector<RefPtr<StyleRule> >& watchedSelectors);

    void resetAuthorStyle();
    void collectFeaturesTo(RuleFeatureSet&, bool isViewSource);
    TreeBoundaryCrossingRules& treeBoundaryCrossingRules() { return m_treeBoundaryCrossingRules; }

private:
    OwnPtr<RuleSet> m_watchedSelectorsRules;
    TreeBoundaryCrossingRules m_treeBoundaryCrossingRules;
};

} // namespace WebCore

#endif // DocumentRuleSets_h
