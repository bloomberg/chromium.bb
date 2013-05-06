/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "core/dom/DOMImplementation.h"

#include "HTMLNames.h"
#include "XMLNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLViewSourceDocument.h"
#include "core/html/ImageDocument.h"
#include "core/html/MediaDocument.h"
#include "core/html/PluginDocument.h"
#include "core/html/TextDocument.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/SecurityOrigin.h"
#include "core/page/Settings.h"
#include "core/platform/ContentType.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/graphics/Image.h"
#include "core/platform/graphics/MediaPlayer.h"
#include "core/plugins/PluginData.h"
#include <wtf/StdLibExtras.h>

#if ENABLE(SVG)
#include "SVGNames.h"
#include "core/svg/SVGDocument.h"
#endif

namespace WebCore {

typedef HashSet<String, CaseFoldingHash> FeatureSet;

static void addString(FeatureSet& set, const char* string)
{
    set.add(string);
}

#if ENABLE(SVG)

static bool isSVG10Feature(const String &feature, const String &version)
{
    if (!version.isEmpty() && version != "1.0")
        return false;

    static bool initialized = false;
    DEFINE_STATIC_LOCAL(FeatureSet, svgFeatures, ());
    if (!initialized) {
#if ENABLE(SVG_FONTS)
        addString(svgFeatures, "svg");
        addString(svgFeatures, "svg.static");
#endif
//      addString(svgFeatures, "svg.animation");
//      addString(svgFeatures, "svg.dynamic");
//      addString(svgFeatures, "svg.dom.animation");
//      addString(svgFeatures, "svg.dom.dynamic");
#if ENABLE(SVG_FONTS)
        addString(svgFeatures, "dom");
        addString(svgFeatures, "dom.svg");
        addString(svgFeatures, "dom.svg.static");
#endif
//      addString(svgFeatures, "svg.all");
//      addString(svgFeatures, "dom.svg.all");
        initialized = true;
    }
    return feature.startsWith("org.w3c.", false)
        && svgFeatures.contains(feature.right(feature.length() - 8));
}

static bool isSVG11Feature(const String &feature, const String &version)
{
    if (!version.isEmpty() && version != "1.1")
        return false;

    static bool initialized = false;
    DEFINE_STATIC_LOCAL(FeatureSet, svgFeatures, ());
    if (!initialized) {
        // Sadly, we cannot claim to implement any of the SVG 1.1 generic feature sets
        // lack of Font and Filter support.
        // http://bugs.webkit.org/show_bug.cgi?id=15480
#if ENABLE(SVG_FONTS)
        addString(svgFeatures, "SVG");
        addString(svgFeatures, "SVGDOM");
        addString(svgFeatures, "SVG-static");
        addString(svgFeatures, "SVGDOM-static");
#endif
        addString(svgFeatures, "SVG-animation");
        addString(svgFeatures, "SVGDOM-animation");
//      addString(svgFeatures, "SVG-dynamic);
//      addString(svgFeatures, "SVGDOM-dynamic);
        addString(svgFeatures, "CoreAttribute");
        addString(svgFeatures, "Structure");
        addString(svgFeatures, "BasicStructure");
        addString(svgFeatures, "ContainerAttribute");
        addString(svgFeatures, "ConditionalProcessing");
        addString(svgFeatures, "Image");
        addString(svgFeatures, "Style");
        addString(svgFeatures, "ViewportAttribute");
        addString(svgFeatures, "Shape");
        addString(svgFeatures, "Text");
        addString(svgFeatures, "BasicText");
        addString(svgFeatures, "PaintAttribute");
        addString(svgFeatures, "BasicPaintAttribute");
        addString(svgFeatures, "OpacityAttribute");
        addString(svgFeatures, "GraphicsAttribute");
        addString(svgFeatures, "BaseGraphicsAttribute");
        addString(svgFeatures, "Marker");
//      addString(svgFeatures, "ColorProfile"); // requires color-profile, bug 6037
        addString(svgFeatures, "Gradient");
        addString(svgFeatures, "Pattern");
        addString(svgFeatures, "Clip");
        addString(svgFeatures, "BasicClip");
        addString(svgFeatures, "Mask");
        addString(svgFeatures, "Filter");
        addString(svgFeatures, "BasicFilter");
        addString(svgFeatures, "DocumentEventsAttribute");
        addString(svgFeatures, "GraphicalEventsAttribute");
//      addString(svgFeatures, "AnimationEventsAttribute");
        addString(svgFeatures, "Cursor");
        addString(svgFeatures, "Hyperlinking");
        addString(svgFeatures, "XlinkAttribute");
        addString(svgFeatures, "ExternalResourcesRequired");
        addString(svgFeatures, "View");
        addString(svgFeatures, "Script");
        addString(svgFeatures, "Animation"); 
#if ENABLE(SVG_FONTS)
        addString(svgFeatures, "Font");
        addString(svgFeatures, "BasicFont");
#endif
        addString(svgFeatures, "Extensibility");
        initialized = true;
    }
    return feature.startsWith("http://www.w3.org/tr/svg11/feature#", false)
        && svgFeatures.contains(feature.right(feature.length() - 35));
}
#endif

static bool isEvents2Feature(const String &feature, const String &version)
{
    if (!version.isEmpty() && version != "2.0")
        return false;

    static bool initialized = false;
    DEFINE_STATIC_LOCAL(FeatureSet, events2Features, ());
    if (!initialized) {
        addString(events2Features, "Events");
        addString(events2Features, "HTMLEvents");
        addString(events2Features, "MouseEvents");
        addString(events2Features, "MutationEvents");
        addString(events2Features, "UIEvents");
        initialized = true;
    }
    return events2Features.contains(feature);
}

static bool isEvents3Feature(const String &feature, const String &version)
{
    if (!version.isEmpty() && version != "3.0")
        return false;

    static bool initialized = false;
    DEFINE_STATIC_LOCAL(FeatureSet, events3Features, ());
    if (!initialized) {
        // FIXME: We probably support many of these features.
//        addString(events3Features, "CompositionEvents");
//        addString(events3Features, "Events");
//        addString(events3Features, "FocusEvents");
//        addString(events3Features, "HTMLEvents");
//        addString(events3Features, "KeyboardEvents");
//        addString(events3Features, "MouseEvents");
//        addString(events3Features, "MutationEvents");
//        addString(events3Features, "MutationNameEvents");
        addString(events3Features, "TextEvents");
//        addString(events3Features, "UIEvents");
//        addString(events3Features, "WheelEvents");
        initialized = true;
    }
    // FIXME: We do not yet support Events 3 "extended feature strings".
    return events3Features.contains(feature);
}

DOMImplementation::DOMImplementation(Document* document)
    : m_document(document)
{
    ScriptWrappable::init(this);
}

bool DOMImplementation::hasFeature(const String& feature, const String& version)
{
    String lower = feature.lower();
    if (lower == "core" || lower == "html" || lower == "xml" || lower == "xhtml")
        return version.isEmpty() || version == "1.0" || version == "2.0";
    if (lower == "css"
            || lower == "css2"
            || lower == "range"
            || lower == "stylesheets"
            || lower == "traversal"
            || lower == "views")
        return version.isEmpty() || version == "2.0";
    if (isEvents2Feature(feature, version))
        return true;
    if (lower == "xpath")
        return version.isEmpty() || version == "3.0";
    if (isEvents3Feature(feature, version))
        return true;

#if ENABLE(SVG)
    if (isSVG11Feature(feature, version))
        return true;
    if (isSVG10Feature(feature, version))
        return true;
#endif

    return false;
}

PassRefPtr<DocumentType> DOMImplementation::createDocumentType(const String& qualifiedName,
    const String& publicId, const String& systemId, ExceptionCode& ec)
{
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, ec))
        return 0;

    return DocumentType::create(0, qualifiedName, publicId, systemId);
}

DOMImplementation* DOMImplementation::getInterface(const String& /*feature*/)
{
    return 0;
}

PassRefPtr<Document> DOMImplementation::createDocument(const String& namespaceURI,
    const String& qualifiedName, DocumentType* doctype, ExceptionCode& ec)
{
    RefPtr<Document> doc;
#if ENABLE(SVG)
    if (namespaceURI == SVGNames::svgNamespaceURI)
        doc = SVGDocument::create(0, KURL());
    else
#endif
    if (namespaceURI == HTMLNames::xhtmlNamespaceURI)
        doc = Document::createXHTML(0, KURL());
    else
        doc = Document::create(0, KURL());

    doc->setSecurityOrigin(m_document->securityOrigin());
    doc->setContextFeatures(m_document->contextFeatures());

    RefPtr<Node> documentElement;
    if (!qualifiedName.isEmpty()) {
        documentElement = doc->createElementNS(namespaceURI, qualifiedName, ec);
        if (ec)
            return 0;
    }

    // WRONG_DOCUMENT_ERR: Raised if doctype has already been used with a different document or was
    // created from a different implementation.
    // Hixie's interpretation of the DOM Core spec suggests we should prefer
    // other exceptions to WRONG_DOCUMENT_ERR (based on order mentioned in spec),
    // but this matches the new DOM Core spec (http://www.w3.org/TR/domcore/).
    if (doctype && doctype->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    if (doctype)
        doc->appendChild(doctype);
    if (documentElement)
        doc->appendChild(documentElement.release());

    return doc.release();
}

PassRefPtr<CSSStyleSheet> DOMImplementation::createCSSStyleSheet(const String&, const String& media, ExceptionCode&)
{
    // FIXME: Title should be set.
    // FIXME: Media could have wrong syntax, in which case we should generate an exception.
    RefPtr<CSSStyleSheet> sheet = CSSStyleSheet::create(StyleSheetContents::create());
    sheet->setMediaQueries(MediaQuerySet::createAllowingDescriptionSyntax(media));
    return sheet;
}

bool DOMImplementation::isXMLMIMEType(const String& mimeType)
{
    if (mimeType == "text/xml" || mimeType == "application/xml" || mimeType == "text/xsl")
        return true;

    // Per RFCs 3023 and 2045 a mime type is of the form:
    // ^[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+/[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+\+xml$

    int length = mimeType.length();
    if (length < 7)
        return false;

    if (mimeType[0] == '/' ||
        mimeType[length - 5] == '/' ||
        mimeType[length - 4] != '+' ||
        mimeType[length - 3] != 'x' ||
        mimeType[length - 2] != 'm' ||
        mimeType[length - 1] != 'l')
        return false;

    bool hasSlash = false;
    for (int i = 0; i < length - 4; ++i) {
        UChar ch = mimeType[i];
        if (ch >= '0' && ch <= '9')
            continue;
        if (ch >= 'a' && ch <= 'z')
            continue;
        if (ch >= 'A' && ch <= 'Z')
            continue;
        switch (ch) {
        case '_':
        case '-':
        case '+':
        case '~':
        case '!':
        case '$':
        case '^':
        case '{':
        case '}':
        case '|':
        case '.':
        case '%':
        case '\'':
        case '`':
        case '#':
        case '&':
        case '*':
            continue;
        case '/':
            if (hasSlash)
                return false;
            hasSlash = true;
            continue;
        default:
            return false;
        }
    }

    return true;
}

bool DOMImplementation::isTextMIMEType(const String& mimeType)
{
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType)
        || mimeType == "application/json" // Render JSON as text/plain.
        || (mimeType.startsWith("text/") && mimeType != "text/html"
            && mimeType != "text/xml" && mimeType != "text/xsl"))
        return true;

    return false;
}

PassRefPtr<HTMLDocument> DOMImplementation::createHTMLDocument(const String& title)
{
    RefPtr<HTMLDocument> d = HTMLDocument::create(0, KURL());
    d->open();
    d->write("<!doctype html><html><body></body></html>");
    if (!title.isNull())
        d->setTitle(title);
    d->setSecurityOrigin(m_document->securityOrigin());
    d->setContextFeatures(m_document->contextFeatures());
    return d.release();
}

PassRefPtr<Document> DOMImplementation::createDocument(const String& type, Frame* frame, const KURL& url, bool inViewSourceMode)
{
    if (inViewSourceMode)
        return HTMLViewSourceDocument::create(frame, url, type);

    // Plugins cannot take HTML and XHTML from us, and we don't even need to initialize the plugin database for those.
    if (type == "text/html")
        return HTMLDocument::create(frame, url);
    if (type == "application/xhtml+xml")
        return Document::createXHTML(frame, url);

    PluginData* pluginData = 0;
    if (frame && frame->page() && frame->loader()->subframeLoader()->allowPlugins(NotAboutToInstantiatePlugin))
        pluginData = frame->page()->pluginData();

    // PDF is one image type for which a plugin can override built-in support.
    // We do not want QuickTime to take over all image types, obviously.
    if ((type == "application/pdf" || type == "text/pdf") && pluginData && pluginData->supportsMimeType(type))
        return PluginDocument::create(frame, url);
    if (Image::supportsType(type))
        return ImageDocument::create(frame, url);

    // Check to see if the type can be played by our MediaPlayer, if so create a MediaDocument
    // Key system is not applicable here.
    if (MediaPlayer::supportsType(ContentType(type), String(), url))
        return MediaDocument::create(frame, url);

    // Everything else except text/plain can be overridden by plugins. In particular, Adobe SVG Viewer should be used for SVG, if installed.
    // Disallowing plug-ins to use text/plain prevents plug-ins from hijacking a fundamental type that the browser is expected to handle,
    // and also serves as an optimization to prevent loading the plug-in database in the common case.
    if (type != "text/plain" && pluginData && pluginData->supportsMimeType(type)) 
        return PluginDocument::create(frame, url);
    if (isTextMIMEType(type))
        return TextDocument::create(frame, url);

#if ENABLE(SVG)
    if (type == "image/svg+xml")
        return SVGDocument::create(frame, url);
#endif
    if (isXMLMIMEType(type))
        return Document::create(frame, url);

    return HTMLDocument::create(frame, url);
}

}
