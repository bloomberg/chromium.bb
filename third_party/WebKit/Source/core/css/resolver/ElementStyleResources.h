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

#ifndef ElementStyleResources_h
#define ElementStyleResources_h

#include "CSSPropertyNames.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSValue.h"
#include "wtf/HashMap.h"

namespace WebCore {

class FilterOperation;

typedef HashMap<FilterOperation*, RefPtr<CSSSVGDocumentValue> > PendingSVGDocumentMap;
typedef HashMap<CSSPropertyID, RefPtr<CSSValue> > PendingImagePropertyMap;

// Holds information about resources, requested by stylesheets.
// Lifetime: per-element style resolve.
// FIXME: At least for the moment, the lifetime actually matches that of StyleResolverState,
// but all data is cleared for each element resolve. We must investigate performance
// implications of matching effective and intended lifetime.
class ElementStyleResources {
WTF_MAKE_NONCOPYABLE(ElementStyleResources);
public:
    ElementStyleResources();

    const PendingImagePropertyMap& pendingImageProperties() const { return m_pendingImageProperties; }
    const PendingSVGDocumentMap& pendingSVGDocuments() const { return m_pendingSVGDocuments; }

    void setHasPendingShaders(bool hasPendingShaders) { m_hasPendingShaders = hasPendingShaders; }
    bool hasPendingShaders() const { return m_hasPendingShaders; }

    void addPendingImageProperty(const CSSPropertyID&, CSSValue*);
    void addPendingSVGDocument(FilterOperation*, CSSSVGDocumentValue*);

    void clear();

private:
    PendingImagePropertyMap m_pendingImageProperties;
    PendingSVGDocumentMap m_pendingSVGDocuments;
    bool m_hasPendingShaders;
};

} // namespace WebCore

#endif // ElementStyleResources_h
