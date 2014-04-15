/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "core/html/HTMLImageElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/css/MediaValuesDynamic.h"
#include "core/css/parser/SizesAttributeParser.h"
#include "core/dom/Attribute.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLSrcsetParser.h"
#include "core/rendering/RenderImage.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

HTMLImageElement::HTMLImageElement(Document& document, HTMLFormElement* form)
    : HTMLElement(imgTag, document)
    , m_imageLoader(this)
    , m_compositeOperator(CompositeSourceOver)
    , m_imageDevicePixelRatio(1.0f)
    , m_formWasSetByParser(false)
    , m_effectiveSize(0)
{
    ScriptWrappable::init(this);
    if (form && form->inDocument()) {
        m_form = form->createWeakPtr();
        m_formWasSetByParser = true;
        m_form->associate(*this);
        m_form->didAssociateByParser();
    }
}

PassRefPtr<HTMLImageElement> HTMLImageElement::create(Document& document)
{
    return adoptRef(new HTMLImageElement(document));
}

PassRefPtr<HTMLImageElement> HTMLImageElement::create(Document& document, HTMLFormElement* form)
{
    return adoptRef(new HTMLImageElement(document, form));
}

HTMLImageElement::~HTMLImageElement()
{
    if (m_form)
        m_form->disassociate(*this);
}

PassRefPtr<HTMLImageElement> HTMLImageElement::createForJSConstructor(Document& document, int width, int height)
{
    RefPtr<HTMLImageElement> image = adoptRef(new HTMLImageElement(document));
    if (width)
        image->setWidth(width);
    if (height)
        image->setHeight(height);
    return image.release();
}

bool HTMLImageElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == borderAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == valignAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLImageElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == borderAttr)
        applyBorderAttributeToStyle(value, style);
    else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else if (name == valignAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, value);
    else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

const AtomicString HTMLImageElement::imageSourceURL() const
{
    return m_bestFitImageURL.isNull() ? fastGetAttribute(srcAttr) : m_bestFitImageURL;
}

HTMLFormElement* HTMLImageElement::formOwner() const
{
    return m_form.get();
}

void HTMLImageElement::formRemovedFromTree(const Node& formRoot)
{
    ASSERT(m_form);
    if (highestAncestor() != formRoot)
        resetFormOwner();
}

void HTMLImageElement::resetFormOwner()
{
    m_formWasSetByParser = false;
    HTMLFormElement* nearestForm = findFormAncestor();
    if (m_form) {
        if (nearestForm == m_form.get())
            return;
        m_form->disassociate(*this);
    }
    if (nearestForm) {
        m_form = nearestForm->createWeakPtr();
        m_form->associate(*this);
    } else {
        m_form = WeakPtr<HTMLFormElement>();
    }
}

void HTMLImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == altAttr) {
        if (renderer() && renderer()->isImage())
            toRenderImage(renderer())->updateAltText();
    } else if (name == srcAttr || name == srcsetAttr) {
        int effectiveSize = -1; // FIXME - hook up the real value from `sizes`
        ImageCandidate candidate = bestFitSourceForImageAttributes(document().devicePixelRatio(), effectiveSize, fastGetAttribute(srcAttr), fastGetAttribute(srcsetAttr));
        m_bestFitImageURL = candidate.toAtomicString();
        float candidateScaleFactor = candidate.scaleFactor();
        if (candidateScaleFactor > 0)
            m_imageDevicePixelRatio = 1 / candidateScaleFactor;
        if (renderer() && renderer()->isImage())
            toRenderImage(renderer())->setImageDevicePixelRatio(m_imageDevicePixelRatio);
        m_imageLoader.updateFromElementIgnoringPreviousError();
    } else if (RuntimeEnabledFeatures::pictureSizesEnabled() && name == sizesAttr) {
        m_effectiveSize = SizesAttributeParser::findEffectiveSize(value, MediaValuesDynamic::create(document()));
    } else if (name == usemapAttr) {
        setIsLink(!value.isNull());
    } else if (name == compositeAttr) {
        // FIXME: images don't support blend modes in their compositing attribute.
        blink::WebBlendMode blendOp = blink::WebBlendModeNormal;
        if (!parseCompositeAndBlendOperator(value, m_compositeOperator, blendOp))
            m_compositeOperator = CompositeSourceOver;
    } else {
        HTMLElement::parseAttribute(name, value);
    }
}

const AtomicString& HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    const AtomicString& alt = fastGetAttribute(altAttr);
    if (!alt.isNull())
        return alt;
    // fall back to title attribute
    return fastGetAttribute(titleAttr);
}

RenderObject* HTMLImageElement::createRenderer(RenderStyle* style)
{
    if (style->hasContent())
        return RenderObject::createObject(this, style);

    RenderImage* image = new RenderImage(this);
    image->setImageResource(RenderImageResource::create());
    image->setImageDevicePixelRatio(m_imageDevicePixelRatio);
    return image;
}

bool HTMLImageElement::canStartSelection() const
{
    if (shadow())
        return HTMLElement::canStartSelection();

    return false;
}

void HTMLImageElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);

    if (renderer() && renderer()->isImage()) {
        RenderImage* renderImage = toRenderImage(renderer());
        RenderImageResource* renderImageResource = renderImage->imageResource();
        if (renderImageResource->hasImage())
            return;

        // If we have no image at all because we have no src attribute, set
        // image height and width for the alt text instead.
        if (!m_imageLoader.image() && !renderImageResource->cachedImage())
            renderImage->setImageSizeForAltText();
        else
            renderImageResource->setImageResource(m_imageLoader.image());

    }
}

Node::InsertionNotificationRequest HTMLImageElement::insertedInto(ContainerNode* insertionPoint)
{
    if (!m_formWasSetByParser || insertionPoint->highestAncestor() != m_form->highestAncestor())
        resetFormOwner();

    // If we have been inserted from a renderer-less document,
    // our loader may have not fetched the image, so do it now.
    if (insertionPoint->inDocument() && !m_imageLoader.image())
        m_imageLoader.updateFromElement();

    return HTMLElement::insertedInto(insertionPoint);
}

void HTMLImageElement::removedFrom(ContainerNode* insertionPoint)
{
    if (!m_form || m_form->highestAncestor() != highestAncestor())
        resetFormOwner();
    HTMLElement::removedFrom(insertionPoint);
}

int HTMLImageElement::width(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int width = getAttribute(widthAttr).toInt(&ok);
        if (ok)
            return width;

        // if the image is available, use its width
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).width();
    }

    if (ignorePendingStylesheets)
        document().updateLayoutIgnorePendingStylesheets();
    else
        document().updateLayout();

    RenderBox* box = renderBox();
    return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedWidth(), box) : 0;
}

int HTMLImageElement::height(bool ignorePendingStylesheets)
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int height = getAttribute(heightAttr).toInt(&ok);
        if (ok)
            return height;

        // if the image is available, use its height
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).height();
    }

    if (ignorePendingStylesheets)
        document().updateLayoutIgnorePendingStylesheets();
    else
        document().updateLayout();

    RenderBox* box = renderBox();
    return box ? adjustForAbsoluteZoom(box->contentBoxRect().pixelSnappedHeight(), box) : 0;
}

int HTMLImageElement::naturalWidth() const
{
    if (!m_imageLoader.image())
        return 0;

    return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).width();
}

int HTMLImageElement::naturalHeight() const
{
    if (!m_imageLoader.image())
        return 0;

    return m_imageLoader.image()->imageSizeForRenderer(renderer(), 1.0f).height();
}

bool HTMLImageElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr
        || attribute.name() == lowsrcAttr
        || attribute.name() == longdescAttr
        || (attribute.name() == usemapAttr && attribute.value().string()[0] != '#')
        || HTMLElement::isURLAttribute(attribute);
}

bool HTMLImageElement::hasLegalLinkAttribute(const QualifiedName& name) const
{
    return name == srcAttr || HTMLElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLImageElement::subResourceAttributeName() const
{
    return srcAttr;
}

const AtomicString& HTMLImageElement::alt() const
{
    return fastGetAttribute(altAttr);
}

bool HTMLImageElement::draggable() const
{
    // Image elements are draggable by default.
    return !equalIgnoringCase(getAttribute(draggableAttr), "false");
}

void HTMLImageElement::setHeight(int value)
{
    setIntegralAttribute(heightAttr, value);
}

KURL HTMLImageElement::src() const
{
    return document().completeURL(getAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
{
    setAttribute(srcAttr, AtomicString(value));
}

void HTMLImageElement::setWidth(int value)
{
    setIntegralAttribute(widthAttr, value);
}

int HTMLImageElement::x() const
{
    RenderObject* r = renderer();
    if (!r)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = r->localToAbsolute();
    return absPos.x();
}

int HTMLImageElement::y() const
{
    RenderObject* r = renderer();
    if (!r)
        return 0;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = r->localToAbsolute();
    return absPos.y();
}

bool HTMLImageElement::complete() const
{
    return m_imageLoader.imageComplete();
}

void HTMLImageElement::didMoveToNewDocument(Document& oldDocument)
{
    m_imageLoader.elementDidMoveToNewDocument();
    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLImageElement::isServerMap() const
{
    if (!fastHasAttribute(ismapAttr))
        return false;

    const AtomicString& usemap = fastGetAttribute(usemapAttr);

    // If the usemap attribute starts with '#', it refers to a map element in the document.
    if (usemap.string()[0] == '#')
        return false;

    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(usemap)).isEmpty();
}

Image* HTMLImageElement::imageContents()
{
    if (!m_imageLoader.imageComplete())
        return 0;

    return m_imageLoader.image()->image();
}

bool HTMLImageElement::isInteractiveContent() const
{
    return fastHasAttribute(usemapAttr);
}

PassRefPtr<Image> HTMLImageElement::getSourceImageForCanvas(SourceImageMode, SourceImageStatus* status) const
{
    if (!complete() || !cachedImage()) {
        *status = IncompleteSourceImageStatus;
        return nullptr;
    }

    if (cachedImage()->errorOccurred()) {
        *status = UndecodableSourceImageStatus;
        return nullptr;
    }

    RefPtr<Image> sourceImage = cachedImage()->imageForRenderer(renderer());

    // We need to synthesize a container size if a renderer is not available to provide one.
    if (!renderer() && sourceImage->usesContainerSize())
        sourceImage->setContainerSize(sourceImage->size());

    *status = NormalSourceImageStatus;
    return sourceImage.release();
}

bool HTMLImageElement::wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const
{
    ImageResource* image = cachedImage();
    if (!image)
        return false;
    return !image->isAccessAllowed(destinationSecurityOrigin);
}

FloatSize HTMLImageElement::sourceSize() const
{
    ImageResource* image = cachedImage();
    if (!image)
        return FloatSize();
    LayoutSize size;
    size = image->imageSizeForRenderer(renderer(), 1.0f); // FIXME: Not sure about this.

    return size;
}

FloatSize HTMLImageElement::defaultDestinationSize() const
{
    ImageResource* image = cachedImage();
    if (!image)
        return FloatSize();
    LayoutSize size;
    size = image->imageSizeForRenderer(renderer(), 1.0f); // FIXME: Not sure about this.
    if (renderer() && renderer()->isRenderImage() && image->image() && !image->image()->hasRelativeWidth())
        size.scale(toRenderImage(renderer())->imageDevicePixelRatio());
    return size;
}

}
