/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLMetaElement.h"

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLMetaElement::HTMLMetaElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(metaTag));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLMetaElement> HTMLMetaElement::create(Document& document)
{
    return adoptRef(new HTMLMetaElement(metaTag, document));
}

PassRefPtr<HTMLMetaElement> HTMLMetaElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLMetaElement(tagName, document));
}

static bool isInvalidSeparator(UChar c)
{
    return c == ';';
}

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't.
static bool isSeparator(UChar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' || c == ',' || c == '\0';
}

void HTMLMetaElement::parseContentAttribute(const String& content, KeyValuePairCallback callback, void* data)
{
    bool error = false;

    // Tread lightly in this code -- it was specifically designed to mimic Win IE's parsing behavior.
    int keyBegin, keyEnd;
    int valueBegin, valueEnd;

    int i = 0;
    int length = content.length();
    String buffer = content.lower();
    while (i < length) {
        // skip to first non-separator, but don't skip past the end of the string
        while (isSeparator(buffer[i])) {
            if (i >= length)
                break;
            i++;
        }
        keyBegin = i;

        // skip to first separator
        while (!isSeparator(buffer[i])) {
            error |= isInvalidSeparator(buffer[i]);
            i++;
        }
        keyEnd = i;

        // skip to first '=', but don't skip past a ',' or the end of the string
        while (buffer[i] != '=') {
            error |= isInvalidSeparator(buffer[i]);
            if (buffer[i] == ',' || i >= length)
                break;
            i++;
        }

        // skip to first non-separator, but don't skip past a ',' or the end of the string
        while (isSeparator(buffer[i])) {
            if (buffer[i] == ',' || i >= length)
                break;
            i++;
        }
        valueBegin = i;

        // skip to first separator
        while (!isSeparator(buffer[i])) {
            error |= isInvalidSeparator(buffer[i]);
            i++;
        }
        valueEnd = i;

        ASSERT_WITH_SECURITY_IMPLICATION(i <= length);

        String keyString = buffer.substring(keyBegin, keyEnd - keyBegin);
        String valueString = buffer.substring(valueBegin, valueEnd - valueBegin);
        callback(keyString, valueString, &document(), data);
    }
    if (error) {
        String message = "Error parsing a meta element's content: ';' is not a valid key-value pair separator. Please use ',' instead.";
        document().addConsoleMessage(RenderingMessageSource, WarningMessageLevel, message);
    }
}

void HTMLMetaElement::processViewportContentAttribute(const String& content, ViewportDescription::Type origin)
{
    ASSERT(!content.isNull());

    if (!document().page() || !document().shouldOverrideLegacyViewport(origin))
        return;

    ViewportDescription newDescriptionFromLegacyTag(origin);
    parseContentAttribute(content, &processViewportKeyValuePair, (void*)&newDescriptionFromLegacyTag);

    if (newDescriptionFromLegacyTag.minZoom == ViewportDescription::ValueAuto)
        newDescriptionFromLegacyTag.minZoom = 0.25;

    if (newDescriptionFromLegacyTag.maxZoom == ViewportDescription::ValueAuto) {
        newDescriptionFromLegacyTag.maxZoom = 5;
        newDescriptionFromLegacyTag.minZoom = std::min(newDescriptionFromLegacyTag.minZoom, float(5));
    }

    const Settings& settings = document().page()->settings();

    if (newDescriptionFromLegacyTag.maxWidth.isAuto()) {
        if (newDescriptionFromLegacyTag.zoom == ViewportDescription::ValueAuto) {
            newDescriptionFromLegacyTag.minWidth = Length(ExtendToZoom);
            newDescriptionFromLegacyTag.maxWidth = Length(settings.layoutFallbackWidth(), Fixed);
        } else if (newDescriptionFromLegacyTag.maxHeight.isAuto()) {
            newDescriptionFromLegacyTag.minWidth = Length(ExtendToZoom);
            newDescriptionFromLegacyTag.maxWidth = Length(ExtendToZoom);
        }
    }

    if (settings.viewportMetaZeroValuesQuirk()
        && newDescriptionFromLegacyTag.type == ViewportDescription::ViewportMeta
        && newDescriptionFromLegacyTag.maxWidth.type() == ViewportPercentageWidth
        && !newDescriptionFromLegacyTag.zoom) {
        newDescriptionFromLegacyTag.zoom = 1.0;
    }

    document().setViewportDescription(newDescriptionFromLegacyTag);
}


void HTMLMetaElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == http_equivAttr)
        process();
    else if (name == contentAttr)
        process();
    else if (name == nameAttr) {
        // Do nothing
    } else
        HTMLElement::parseAttribute(name, value);
}

Node::InsertionNotificationRequest HTMLMetaElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument())
        process();
    return InsertionDone;
}

void HTMLMetaElement::process()
{
    if (!inDocument())
        return;

    const AtomicString& contentValue = fastGetAttribute(contentAttr);
    if (contentValue.isNull())
        return;

    if (equalIgnoringCase(name(), "viewport"))
        processViewportContentAttribute(contentValue, ViewportDescription::ViewportMeta);
    else if (equalIgnoringCase(name(), "referrer"))
        document().processReferrerPolicy(contentValue);
    else if (equalIgnoringCase(name(), "handheldfriendly") && equalIgnoringCase(contentValue, "true"))
        processViewportContentAttribute("width=device-width", ViewportDescription::HandheldFriendlyMeta);
    else if (equalIgnoringCase(name(), "mobileoptimized"))
        processViewportContentAttribute("width=device-width, initial-scale=1", ViewportDescription::MobileOptimizedMeta);

    // Get the document to process the tag, but only if we're actually part of DOM tree (changing a meta tag while
    // it's not in the tree shouldn't have any effect on the document)
    const AtomicString& httpEquivValue = fastGetAttribute(http_equivAttr);
    if (!httpEquivValue.isNull())
        document().processHttpEquiv(httpEquivValue, contentValue);
}

String HTMLMetaElement::content() const
{
    return getAttribute(contentAttr);
}

String HTMLMetaElement::httpEquiv() const
{
    return getAttribute(http_equivAttr);
}

String HTMLMetaElement::name() const
{
    return getNameAttribute();
}

}
