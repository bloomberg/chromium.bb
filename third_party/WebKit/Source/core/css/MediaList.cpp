/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2010, 2012 Apple Inc. All rights reserved.
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
#include "core/css/MediaList.h"

#include "bindings/v8/ExceptionState.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaFeatureNames.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaQueryExp.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/DOMWindow.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

/* MediaList is used to store 3 types of media related entities which mean the same:
 *
 * Media Queries, Media Types and Media Descriptors.
 *
 * Media queries, as described in the Media Queries Level 3 specification, build on
 * the mechanism outlined in HTML4. The syntax of media queries fit into the media
 * type syntax reserved in HTML4. The media attribute of HTML4 also exists in XHTML
 * and generic XML. The same syntax can also be used inside the @media and @import
 * rules of CSS.
 *
 * However, the parsing rules for media queries are incompatible with those of HTML4
 * and are consistent with those of media queries used in CSS.
 *
 * HTML5 (at the moment of writing still work in progress) references the Media Queries
 * specification directly and thus updates the rules for HTML.
 *
 * CSS 2.1 Spec (http://www.w3.org/TR/CSS21/media.html)
 * CSS 3 Media Queries Spec (http://www.w3.org/TR/css3-mediaqueries/)
 */

MediaQuerySet::MediaQuerySet()
{
}

MediaQuerySet::MediaQuerySet(const MediaQuerySet& o)
    : RefCounted<MediaQuerySet>()
    , m_queries(o.m_queries.size())
{
    for (unsigned i = 0; i < m_queries.size(); ++i)
        m_queries[i] = o.m_queries[i]->copy();
}

MediaQuerySet::~MediaQuerySet()
{
}

PassRefPtr<MediaQuerySet> MediaQuerySet::create(const String& mediaString)
{
    if (mediaString.isEmpty())
        return MediaQuerySet::create();

    CSSParser parser(CSSStrictMode);
    return parser.parseMediaQueryList(mediaString);
}

bool MediaQuerySet::set(const String& mediaString)
{
    RefPtr<MediaQuerySet> result = create(mediaString);
    m_queries.swap(result->m_queries);
    return true;
}

bool MediaQuerySet::add(const String& queryString)
{
    // To "parse a media query" for a given string means to follow "the parse
    // a media query list" steps and return "null" if more than one media query
    // is returned, or else the returned media query.
    RefPtr<MediaQuerySet> result = create(queryString);

    // Only continue if exactly one media query is found, as described above.
    if (result->m_queries.size() != 1)
        return true;

    OwnPtr<MediaQuery> newQuery = result->m_queries[0].release();
    ASSERT(newQuery);

    // If comparing with any of the media queries in the collection of media
    // queries returns true terminate these steps.
    for (size_t i = 0; i < m_queries.size(); ++i) {
        MediaQuery* query = m_queries[i].get();
        if (*query == *newQuery)
            return true;
    }

    m_queries.append(newQuery.release());
    return true;
}

bool MediaQuerySet::remove(const String& queryStringToRemove)
{
    // To "parse a media query" for a given string means to follow "the parse
    // a media query list" steps and return "null" if more than one media query
    // is returned, or else the returned media query.
    RefPtr<MediaQuerySet> result = create(queryStringToRemove);

    // Only continue if exactly one media query is found, as described above.
    if (result->m_queries.size() != 1)
        return true;

    OwnPtr<MediaQuery> newQuery = result->m_queries[0].release();
    ASSERT(newQuery);

    // Remove any media query from the collection of media queries for which
    // comparing with the media query returns true.
    bool found = false;
    for (size_t i = 0; i < m_queries.size(); ++i) {
        MediaQuery* query = m_queries[i].get();
        if (*query == *newQuery) {
            m_queries.remove(i);
            --i;
            found = true;
        }
    }

    return found;
}

void MediaQuerySet::addMediaQuery(PassOwnPtr<MediaQuery> mediaQuery)
{
    m_queries.append(mediaQuery);
}

String MediaQuerySet::mediaText() const
{
    StringBuilder text;

    bool first = true;
    for (size_t i = 0; i < m_queries.size(); ++i) {
        if (!first)
            text.appendLiteral(", ");
        else
            first = false;
        text.append(m_queries[i]->cssText());
    }
    return text.toString();
}

MediaList::MediaList(MediaQuerySet* mediaQueries, CSSStyleSheet* parentSheet)
    : m_mediaQueries(mediaQueries)
    , m_parentStyleSheet(parentSheet)
    , m_parentRule(0)
{
}

MediaList::MediaList(MediaQuerySet* mediaQueries, CSSRule* parentRule)
    : m_mediaQueries(mediaQueries)
    , m_parentStyleSheet(0)
    , m_parentRule(parentRule)
{
}

MediaList::~MediaList()
{
}

void MediaList::setMediaText(const String& value)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    m_mediaQueries->set(value);

    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

String MediaList::item(unsigned index) const
{
    const Vector<OwnPtr<MediaQuery> >& queries = m_mediaQueries->queryVector();
    if (index < queries.size())
        return queries[index]->cssText();
    return String();
}

void MediaList::deleteMedium(const String& medium, ExceptionState& es)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->remove(medium);
    if (!success) {
        es.throwUninformativeAndGenericDOMException(NotFoundError);
        return;
    }
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

void MediaList::appendMedium(const String& medium, ExceptionState& es)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->add(medium);
    if (!success) {
        es.throwUninformativeAndGenericDOMException(InvalidCharacterError);
        return;
    }

    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

void MediaList::reattach(MediaQuerySet* mediaQueries)
{
    ASSERT(mediaQueries);
    m_mediaQueries = mediaQueries;
}

static void addResolutionWarningMessageToConsole(Document* document, const String& serializedExpression, const CSSPrimitiveValue* value)
{
    ASSERT(document);
    ASSERT(value);

    DEFINE_STATIC_LOCAL(String, mediaQueryMessage, ("Consider using 'dppx' units instead of '%replacementUnits%', as in CSS '%replacementUnits%' means dots-per-CSS-%lengthUnit%, not dots-per-physical-%lengthUnit%, so does not correspond to the actual '%replacementUnits%' of a screen. In media query expression: "));
    DEFINE_STATIC_LOCAL(String, mediaValueDPI, ("dpi"));
    DEFINE_STATIC_LOCAL(String, mediaValueDPCM, ("dpcm"));
    DEFINE_STATIC_LOCAL(String, lengthUnitInch, ("inch"));
    DEFINE_STATIC_LOCAL(String, lengthUnitCentimeter, ("centimeter"));

    String message;
    if (value->isDotsPerInch())
        message = String(mediaQueryMessage).replace("%replacementUnits%", mediaValueDPI).replace("%lengthUnit%", lengthUnitInch);
    else if (value->isDotsPerCentimeter())
        message = String(mediaQueryMessage).replace("%replacementUnits%", mediaValueDPCM).replace("%lengthUnit%", lengthUnitCentimeter);
    else
        ASSERT_NOT_REACHED();

    message.append(serializedExpression);

    document->addConsoleMessage(CSSMessageSource, DebugMessageLevel, message);
}

static inline bool isResolutionMediaFeature(const AtomicString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::resolutionMediaFeature
        || mediaFeature == MediaFeatureNames::maxResolutionMediaFeature
        || mediaFeature == MediaFeatureNames::minResolutionMediaFeature;
}

void reportMediaQueryWarningIfNeeded(Document* document, const MediaQuerySet* mediaQuerySet)
{
    if (!mediaQuerySet || !document)
        return;

    const Vector<OwnPtr<MediaQuery> >& mediaQueries = mediaQuerySet->queryVector();
    const size_t queryCount = mediaQueries.size();

    if (!queryCount)
        return;

    for (size_t i = 0; i < queryCount; ++i) {
        const MediaQuery* query = mediaQueries[i].get();
        if (equalIgnoringCase(query->mediaType(), "print"))
            continue;

        const ExpressionVector& expressions = query->expressions();
        for (size_t j = 0; j < expressions.size(); ++j) {
            const MediaQueryExp* expression = expressions.at(j).get();
            if (isResolutionMediaFeature(expression->mediaFeature())) {
                CSSValue* cssValue =  expression->value();
                if (cssValue && cssValue->isPrimitiveValue()) {
                    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(cssValue);
                    if (primitiveValue->isDotsPerInch() || primitiveValue->isDotsPerCentimeter())
                        addResolutionWarningMessageToConsole(document, mediaQuerySet->mediaText(), primitiveValue);
                }
            }
        }
    }
}

}
