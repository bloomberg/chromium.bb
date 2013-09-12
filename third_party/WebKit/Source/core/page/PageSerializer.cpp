/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/PageSerializer.h"

#include "HTMLNames.h"
#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/MarkupAccumulator.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/parser/HTMLMetaCharsetParser.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/platform/SerializedResource.h"
#include "core/platform/graphics/Image.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/style/StyleFetchedImage.h"
#include "core/rendering/style/StyleImage.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

static bool isCharsetSpecifyingNode(Node* node)
{
    if (!node->isHTMLElement())
        return false;

    HTMLElement* element = toHTMLElement(node);
    if (!element->hasTagName(HTMLNames::metaTag))
        return false;
    HTMLMetaCharsetParser::AttributeList attributes;
    if (element->hasAttributes()) {
        for (unsigned i = 0; i < element->attributeCount(); ++i) {
            const Attribute* attribute = element->attributeItem(i);
            // FIXME: We should deal appropriately with the attribute if they have a namespace.
            attributes.append(std::make_pair(attribute->name().toString(), attribute->value().string()));
        }
    }
    WTF::TextEncoding textEncoding = HTMLMetaCharsetParser::encodingFromMetaAttributes(attributes);
    return textEncoding.isValid();
}

static bool shouldIgnoreElement(Element* element)
{
    return element->hasTagName(HTMLNames::scriptTag) || element->hasTagName(HTMLNames::noscriptTag) || isCharsetSpecifyingNode(element);
}

static const QualifiedName& frameOwnerURLAttributeName(const HTMLFrameOwnerElement& frameOwner)
{
    // FIXME: We should support all frame owners including applets.
    return frameOwner.hasTagName(HTMLNames::objectTag) ? HTMLNames::dataAttr : HTMLNames::srcAttr;
}

class SerializerMarkupAccumulator : public WebCore::MarkupAccumulator {
public:
    SerializerMarkupAccumulator(PageSerializer*, Document*, Vector<Node*>*);
    virtual ~SerializerMarkupAccumulator();

protected:
    virtual void appendText(StringBuilder& out, Text*);
    virtual void appendElement(StringBuilder& out, Element*, Namespaces*);
    virtual void appendCustomAttributes(StringBuilder& out, Element*, Namespaces*);
    virtual void appendEndTag(Node*);

private:
    PageSerializer* m_serializer;
    Document* m_document;
};

SerializerMarkupAccumulator::SerializerMarkupAccumulator(PageSerializer* serializer, Document* document, Vector<Node*>* nodes)
    : MarkupAccumulator(nodes, ResolveAllURLs)
    , m_serializer(serializer)
    , m_document(document)
{
}

SerializerMarkupAccumulator::~SerializerMarkupAccumulator()
{
}

void SerializerMarkupAccumulator::appendText(StringBuilder& out, Text* text)
{
    Element* parent = text->parentElement();
    if (parent && !shouldIgnoreElement(parent))
        MarkupAccumulator::appendText(out, text);
}

void SerializerMarkupAccumulator::appendElement(StringBuilder& out, Element* element, Namespaces* namespaces)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendElement(out, element, namespaces);

    if (element->hasTagName(HTMLNames::headTag)) {
        out.append("<meta charset=\"");
        out.append(m_document->charset());
        out.append("\">");
    }

    // FIXME: For object (plugins) tags and video tag we could replace them by an image of their current contents.
}

void SerializerMarkupAccumulator::appendCustomAttributes(StringBuilder& out, Element* element, Namespaces* namespaces)
{
    if (!element->isFrameOwnerElement())
        return;

    HTMLFrameOwnerElement* frameOwner = toHTMLFrameOwnerElement(element);
    Frame* frame = frameOwner->contentFrame();
    if (!frame)
        return;

    KURL url = frame->document()->url();
    if (url.isValid() && !url.isBlankURL())
        return;

    // We need to give a fake location to blank frames so they can be referenced by the serialized frame.
    url = m_serializer->urlForBlankFrame(frame);
    appendAttribute(out, element, Attribute(frameOwnerURLAttributeName(*frameOwner), url.string()), namespaces);
}

void SerializerMarkupAccumulator::appendEndTag(Node* node)
{
    if (node->isElementNode() && !shouldIgnoreElement(toElement(node)))
        MarkupAccumulator::appendEndTag(node);
}

PageSerializer::PageSerializer(Vector<SerializedResource>* resources)
    : m_resources(resources)
    , m_blankFrameCounter(0)
{
}

void PageSerializer::serialize(Page* page)
{
    serializeFrame(page->mainFrame());
}

void PageSerializer::serializeFrame(Frame* frame)
{
    Document* document = frame->document();
    KURL url = document->url();
    if (!url.isValid() || url.isBlankURL()) {
        // For blank frames we generate a fake URL so they can be referenced by their containing frame.
        url = urlForBlankFrame(frame);
    }

    if (m_resourceURLs.contains(url)) {
        // FIXME: We could have 2 frame with the same URL but which were dynamically changed and have now
        // different content. So we should serialize both and somehow rename the frame src in the containing
        // frame. Arg!
        return;
    }

    Vector<Node*> nodes;
    SerializerMarkupAccumulator accumulator(this, document, &nodes);
    WTF::TextEncoding textEncoding(document->charset());
    CString data;
    if (!textEncoding.isValid()) {
        // FIXME: iframes used as images trigger this. We should deal with them correctly.
        return;
    }
    String text = accumulator.serializeNodes(document, IncludeNode);
    CString frameHTML = textEncoding.normalizeAndEncode(text, WTF::EntitiesForUnencodables);
    m_resources->append(SerializedResource(url, document->suggestedMIMEType(), SharedBuffer::create(frameHTML.data(), frameHTML.length())));
    m_resourceURLs.add(url);

    for (Vector<Node*>::iterator iter = nodes.begin(); iter != nodes.end(); ++iter) {
        Node* node = *iter;
        if (!node->isElementNode())
            continue;

        Element* element = toElement(node);
        // We have to process in-line style as it might contain some resources (typically background images).
        if (element->isStyledElement())
            retrieveResourcesForProperties(element->inlineStyle(), document);

        if (element->hasTagName(HTMLNames::imgTag)) {
            HTMLImageElement* imageElement = toHTMLImageElement(element);
            KURL url = document->completeURL(imageElement->getAttribute(HTMLNames::srcAttr));
            ImageResource* cachedImage = imageElement->cachedImage();
            addImageToResources(cachedImage, imageElement->renderer(), url);
        } else if (element->hasTagName(HTMLNames::inputTag)) {
            HTMLInputElement* inputElement = toHTMLInputElement(element);
            if (inputElement->isImageButton() && inputElement->hasImageLoader()) {
                KURL url = inputElement->src();
                ImageResource* cachedImage = inputElement->imageLoader()->image();
                addImageToResources(cachedImage, inputElement->renderer(), url);
            }
        } else if (element->hasTagName(HTMLNames::linkTag)) {
            HTMLLinkElement* linkElement = toHTMLLinkElement(element);
            if (CSSStyleSheet* sheet = linkElement->sheet()) {
                KURL url = document->completeURL(linkElement->getAttribute(HTMLNames::hrefAttr));
                serializeCSSStyleSheet(sheet, url);
                ASSERT(m_resourceURLs.contains(url));
            }
        } else if (element->hasTagName(HTMLNames::styleTag)) {
            HTMLStyleElement* styleElement = toHTMLStyleElement(element);
            if (CSSStyleSheet* sheet = styleElement->sheet())
                serializeCSSStyleSheet(sheet, KURL());
        }
    }

    for (Frame* childFrame = frame->tree()->firstChild(); childFrame; childFrame = childFrame->tree()->nextSibling())
        serializeFrame(childFrame);
}

void PageSerializer::serializeCSSStyleSheet(CSSStyleSheet* styleSheet, const KURL& url)
{
    StringBuilder cssText;
    for (unsigned i = 0; i < styleSheet->length(); ++i) {
        CSSRule* rule = styleSheet->item(i);
        String itemText = rule->cssText();
        if (!itemText.isEmpty()) {
            cssText.append(itemText);
            if (i < styleSheet->length() - 1)
                cssText.append("\n\n");
        }
        Document* document = styleSheet->ownerDocument();
        // Some rules have resources associated with them that we need to retrieve.
        if (rule->type() == CSSRule::IMPORT_RULE) {
            CSSImportRule* importRule = toCSSImportRule(rule);
            KURL importURL = document->completeURL(importRule->href());
            if (m_resourceURLs.contains(importURL))
                continue;
            serializeCSSStyleSheet(importRule->styleSheet(), importURL);
        } else if (rule->type() == CSSRule::FONT_FACE_RULE) {
            retrieveResourcesForProperties(toCSSFontFaceRule(rule)->styleRule()->properties(), document);
        } else if (rule->type() == CSSRule::STYLE_RULE) {
            retrieveResourcesForProperties(toCSSStyleRule(rule)->styleRule()->properties(), document);
        }
    }

    if (url.isValid() && !m_resourceURLs.contains(url)) {
        // FIXME: We should check whether a charset has been specified and if none was found add one.
        WTF::TextEncoding textEncoding(styleSheet->contents()->charset());
        ASSERT(textEncoding.isValid());
        String textString = cssText.toString();
        CString text = textEncoding.normalizeAndEncode(textString, WTF::EntitiesForUnencodables);
        m_resources->append(SerializedResource(url, String("text/css"), SharedBuffer::create(text.data(), text.length())));
        m_resourceURLs.add(url);
    }
}

bool PageSerializer::shouldAddURL(const KURL& url)
{
    return url.isValid() && !m_resourceURLs.contains(url) && !url.protocolIsData();
}

void PageSerializer::addToResources(Resource* resource, PassRefPtr<SharedBuffer> data, const KURL& url)
{
    if (!data) {
        LOG_ERROR("No data for resource %s", url.string().utf8().data());
        return;
    }

    String mimeType = resource->response().mimeType();
    m_resources->append(SerializedResource(url, mimeType, data));
    m_resourceURLs.add(url);
}

void PageSerializer::addImageToResources(ImageResource* image, RenderObject* imageRenderer, const KURL& url)
{
    if (!shouldAddURL(url))
        return;

    if (!image || image->image() == Image::nullImage())
        return;

    RefPtr<SharedBuffer> data = imageRenderer ? image->imageForRenderer(imageRenderer)->data() : 0;
    if (!data)
        data = image->image()->data();

    addToResources(image, data, url);
}

void PageSerializer::addFontToResources(FontResource* font)
{
    if (!font || !shouldAddURL(font->url()) || !font->isLoaded() || !font->resourceBuffer()) {
        return;
    }
    RefPtr<SharedBuffer> data(font->resourceBuffer());

    addToResources(font, data, font->url());
}

void PageSerializer::retrieveResourcesForProperties(const StylePropertySet* styleDeclaration, Document* document)
{
    if (!styleDeclaration)
        return;

    // The background-image and list-style-image (for ul or ol) are the CSS properties
    // that make use of images. We iterate to make sure we include any other
    // image properties there might be.
    unsigned propertyCount = styleDeclaration->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        RefPtr<CSSValue> cssValue = styleDeclaration->propertyAt(i).value();
        retrieveResourcesForCSSValue(cssValue.get(), document);
    }
}

void PageSerializer::retrieveResourcesForCSSValue(CSSValue* cssValue, Document* document)
{
    if (cssValue->isImageValue()) {
        CSSImageValue* imageValue = toCSSImageValue(cssValue);
        StyleImage* styleImage = imageValue->cachedOrPendingImage();
        // Non cached-images are just place-holders and do not contain data.
        if (!styleImage || !styleImage->isImageResource())
            return;

        addImageToResources(styleImage->cachedImage(), 0, styleImage->cachedImage()->url());
    } else if (cssValue->isFontFaceSrcValue()) {
        CSSFontFaceSrcValue* fontFaceSrcValue = toCSSFontFaceSrcValue(cssValue);
        if (fontFaceSrcValue->isLocal()) {
            return;
        }

        addFontToResources(fontFaceSrcValue->fetch(document));
    } else if (cssValue->isValueList()) {
        CSSValueList* cssValueList = toCSSValueList(cssValue);
        for (unsigned i = 0; i < cssValueList->length(); i++)
            retrieveResourcesForCSSValue(cssValueList->item(i), document);
    }
}

KURL PageSerializer::urlForBlankFrame(Frame* frame)
{
    HashMap<Frame*, KURL>::iterator iter = m_blankFrameURLs.find(frame);
    if (iter != m_blankFrameURLs.end())
        return iter->value;
    String url = "wyciwyg://frame/" + String::number(m_blankFrameCounter++);
    KURL fakeURL(ParsedURLString, url);
    m_blankFrameURLs.add(frame, fakeURL);

    return fakeURL;
}

}
