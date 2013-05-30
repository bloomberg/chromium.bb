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

#include "core/css/CSSParser.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaFeatureNames.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaQueryExp.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/page/DOMWindow.h"
#include "wtf/MemoryInstrumentationVector.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

/* MediaList is used to store 3 types of media related entities which mean the same:
 *
 * Media Queries, Media Types and Media Descriptors.
 *
 * Slight problem with syntax error handling:
 * CSS 2.1 Spec (http://www.w3.org/TR/CSS21/media.html)
 * specifies that failing media type parsing is a syntax error
 * CSS 3 Media Queries Spec (http://www.w3.org/TR/css3-mediaqueries/)
 * specifies that failing media query is a syntax error
 * HTML 4.01 spec (http://www.w3.org/TR/REC-html40/present/styles.html#adef-media)
 * specifies that Media Descriptors should be parsed with forward-compatible syntax
 * DOM Level 2 Style Sheet spec (http://www.w3.org/TR/DOM-Level-2-Style/)
 * talks about MediaList.mediaText and refers
 *   -  to Media Descriptors of HTML 4.0 in context of StyleSheet
 *   -  to Media Types of CSS 2.0 in context of CSSMediaRule and CSSImportRule
 *
 * These facts create situation where same (illegal) media specification may result in
 * different parses depending on whether it is media attr of style element or part of
 * css @media rule.
 * <style media="screen and resolution > 40dpi"> ..</style> will be enabled on screen devices where as
 * @media screen and resolution > 40dpi {..} will not.
 * This gets more counter-intuitive in JavaScript:
 * document.styleSheets[0].media.mediaText = "screen and resolution > 40dpi" will be ok and
 * enabled, while
 * document.styleSheets[0].cssRules[0].media.mediaText = "screen and resolution > 40dpi" will
 * throw SYNTAX_ERR exception.
 */

MediaQuerySet::MediaQuerySet()
    : m_parserMode(MediaQueryNormalMode)
    , m_lastLine(0)
{
}

MediaQuerySet::MediaQuerySet(const String& mediaString, MediaQueryParserMode mode)
    : m_parserMode(mode)
    , m_lastLine(0)
{
    set(mediaString);
}

MediaQuerySet::MediaQuerySet(const MediaQuerySet& o)
    : RefCounted<MediaQuerySet>()
    , m_parserMode(o.m_parserMode)
    , m_lastLine(o.m_lastLine)
    , m_queries(o.m_queries.size())
{
    for (unsigned i = 0; i < m_queries.size(); ++i)
        m_queries[i] = o.m_queries[i]->copy();
}

MediaQuerySet::~MediaQuerySet()
{
}

static String parseMediaDescriptor(const String& string)
{
    // http://www.w3.org/TR/REC-html40/types.html#type-media-descriptors
    // "Each entry is truncated just before the first character that isn't a
    // US ASCII letter [a-zA-Z] (ISO 10646 hex 41-5a, 61-7a), digit [0-9] (hex 30-39),
    // or hyphen (hex 2d)."
    unsigned length = string.length();
    unsigned i = 0;
    for (; i < length; ++i) {
        unsigned short c = string[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '1' && c <= '9') || (c == '-')))
            break;
    }
    return string.left(i);
}

PassOwnPtr<MediaQuery> MediaQuerySet::parseMediaQuery(const String& queryString, MediaQueryParserMode mode)
{
    CSSParser parser(CSSStrictMode);
    OwnPtr<MediaQuery> parsedQuery = parser.parseMediaQuery(queryString);

    if (parsedQuery)
        return parsedQuery.release();

    switch (mode) {
    case MediaQueryForwardCompatibleSyntaxMode: {
        String medium = parseMediaDescriptor(queryString);
        if (!medium.isNull())
            return adoptPtr(new MediaQuery(MediaQuery::None, medium, nullptr));
        // Fall through.
    }
    case MediaQueryNormalMode:
        return adoptPtr(new MediaQuery(MediaQuery::None, "not all", nullptr));
    case MediaQueryStrictMode:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return nullptr;
}

void MediaQuerySet::parseMediaQueryList(const String& mediaString, MediaQueryParserMode mode, Vector<OwnPtr<MediaQuery> >& result)
{
    if (mediaString.isEmpty()) {
        result.clear();
        return;
    }

    Vector<String> list;
    // FIXME: This is too simple as it shouldn't split when the ',' is inside
    // other allowed matching pairs such as (), [], {}, "", and ''.
    mediaString.split(',', /* allowEmptyEntries */ true, list);

    result.reserveInitialCapacity(list.size());

    for (unsigned i = 0; i < list.size(); ++i) {
        String queryString = list[i].stripWhiteSpace();
        OwnPtr<MediaQuery> parsedQuery = parseMediaQuery(queryString, mode);
        if (parsedQuery)
            result.uncheckedAppend(parsedQuery.release());
    }
}

bool MediaQuerySet::set(const String& mediaString)
{
    Vector<OwnPtr<MediaQuery> > result;
    parseMediaQueryList(mediaString, parserMode(), result);
    m_queries.swap(result);
    return true;
}

bool MediaQuerySet::add(const String& queryString)
{
    // To "parse a media query" for a given string means to follow "the parse
    // a media query list" steps and return "null" if more than one media query
    // is returned, or else the returned media query.
    Vector<OwnPtr<MediaQuery> > queries;
    parseMediaQueryList(queryString, MediaQueryStrictMode, queries);

    // Only continue if exactly one media query is found, as described above.
    if (queries.size() != 1)
        return true;

    OwnPtr<MediaQuery> newQuery = queries[0].release();
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
    Vector<OwnPtr<MediaQuery> > queries;
    parseMediaQueryList(queryStringToRemove, MediaQueryStrictMode, queries);

    // Only continue if exactly one media query is found, as described above.
    if (queries.size() != 1)
        return true;

    OwnPtr<MediaQuery> newQuery = queries[0].release();
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

void MediaQuerySet::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_queries, "queries");
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

void MediaList::deleteMedium(const String& medium, ExceptionCode& ec)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->remove(medium);
    if (!success) {
        ec = NOT_FOUND_ERR;
        return;
    }
    if (m_parentStyleSheet)
        m_parentStyleSheet->didMutate();
}

void MediaList::appendMedium(const String& medium, ExceptionCode& ec)
{
    CSSStyleSheet::RuleMutationScope mutationScope(m_parentRule);

    bool success = m_mediaQueries->add(medium);
    if (!success) {
        ec = INVALID_CHARACTER_ERR;
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

void MediaList::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_mediaQueries, "mediaQueries");
    info.addMember(m_parentStyleSheet, "parentStyleSheet");
    info.addMember(m_parentRule, "parentRule");
}

static void addResolutionWarningMessageToConsole(Document* document, const String& serializedExpression, const CSSPrimitiveValue* value)
{
    ASSERT(document);
    ASSERT(value);

    DEFINE_STATIC_LOCAL(String, mediaQueryMessage, (ASCIILiteral("Consider using 'dppx' units instead of '%replacementUnits%', as in CSS '%replacementUnits%' means dots-per-CSS-%lengthUnit%, not dots-per-physical-%lengthUnit%, so does not correspond to the actual '%replacementUnits%' of a screen. In media query expression: ")));
    DEFINE_STATIC_LOCAL(String, mediaValueDPI, (ASCIILiteral("dpi")));
    DEFINE_STATIC_LOCAL(String, mediaValueDPCM, (ASCIILiteral("dpcm")));
    DEFINE_STATIC_LOCAL(String, lengthUnitInch, (ASCIILiteral("inch")));
    DEFINE_STATIC_LOCAL(String, lengthUnitCentimeter, (ASCIILiteral("centimeter")));

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
        if (query->ignored() || equalIgnoringCase(query->mediaType(), "print"))
            continue;

        const Vector<OwnPtr<MediaQueryExp> >* exps = query->expressions();
        for (size_t j = 0; j < exps->size(); ++j) {
            const MediaQueryExp* exp = exps->at(j).get();
            if (isResolutionMediaFeature(exp->mediaFeature())) {
                CSSValue* cssValue =  exp->value();
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
