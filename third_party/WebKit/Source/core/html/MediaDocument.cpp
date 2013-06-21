/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "core/html/MediaDocument.h"

#include "HTMLNames.h"
#include "core/dom/EventNames.h"
#include "core/dom/ExceptionCodePlaceholder.h"
#include "core/dom/KeyboardEvent.h"
#include "core/dom/NodeList.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Frame.h"
#include "core/platform/chromium/KeyboardCodes.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: Share more code with PluginDocumentParser.
class MediaDocumentParser : public RawDataDocumentParser {
public:
    static PassRefPtr<MediaDocumentParser> create(MediaDocument* document)
    {
        return adoptRef(new MediaDocumentParser(document));
    }
    
private:
    MediaDocumentParser(Document* document)
        : RawDataDocumentParser(document)
        , m_mediaElement(0)
    {
    }

    virtual size_t appendBytes(const char*, size_t) OVERRIDE;

    void createDocumentStructure();

    HTMLMediaElement* m_mediaElement;
};
    
void MediaDocumentParser::createDocumentStructure()
{
    RefPtr<Element> rootElement = document()->createElement(htmlTag, false);
    document()->appendChild(rootElement, IGNORE_EXCEPTION);
    document()->setCSSTarget(rootElement.get());
    static_cast<HTMLHtmlElement*>(rootElement.get())->insertedByParser();

    if (document()->frame())
        document()->frame()->loader()->dispatchDocumentElementAvailable();
        
    RefPtr<Element> body = document()->createElement(bodyTag, false);
    rootElement->appendChild(body, IGNORE_EXCEPTION);

    RefPtr<Element> mediaElement = document()->createElement(videoTag, false);

    m_mediaElement = static_cast<HTMLVideoElement*>(mediaElement.get());
    m_mediaElement->setAttribute(controlsAttr, "");
    m_mediaElement->setAttribute(autoplayAttr, "");

    m_mediaElement->setAttribute(nameAttr, "media");

    RefPtr<Element> sourceElement = document()->createElement(sourceTag, false);
    HTMLSourceElement* source = static_cast<HTMLSourceElement*>(sourceElement.get());
    source->setSrc(document()->url());

    if (DocumentLoader* loader = document()->loader())
        source->setType(loader->responseMIMEType());

    m_mediaElement->appendChild(sourceElement, IGNORE_EXCEPTION);
    body->appendChild(mediaElement, IGNORE_EXCEPTION);
}

size_t MediaDocumentParser::appendBytes(const char*, size_t)
{
    if (m_mediaElement)
        return 0;

    createDocumentStructure();
    finish();
    return 0;
}
    
MediaDocument::MediaDocument(Frame* frame, const KURL& url)
    : HTMLDocument(frame, url, MediaDocumentClass)
{
    setCompatibilityMode(QuirksMode);
    lockCompatibilityMode();
}

PassRefPtr<DocumentParser> MediaDocument::createParser()
{
    return MediaDocumentParser::create(this);
}

static inline HTMLVideoElement* descendentVideoElement(Node* node)
{
    ASSERT(node);

    if (node->hasTagName(videoTag))
        return static_cast<HTMLVideoElement*>(node);

    RefPtr<NodeList> nodeList = node->getElementsByTagNameNS(videoTag.namespaceURI(), videoTag.localName());
   
    if (nodeList.get()->length() > 0)
        return static_cast<HTMLVideoElement*>(nodeList.get()->item(0));

    return 0;
}

void MediaDocument::defaultEventHandler(Event* event)
{
    // Match the default Quicktime plugin behavior to allow 
    // clicking and double-clicking to pause and play the media.
    Node* targetNode = event->target()->toNode();
    if (!targetNode)
        return;

    if (event->type() == eventNames().keydownEvent && event->isKeyboardEvent()) {
        HTMLVideoElement* video = descendentVideoElement(targetNode);
        if (!video)
            return;

        KeyboardEvent* keyboardEvent = static_cast<KeyboardEvent*>(event);
        if (keyboardEvent->keyIdentifier() == "U+0020" || keyboardEvent->keyCode() == VKEY_MEDIA_PLAY_PAUSE) {
            // space or media key (play/pause)
            if (video->paused()) {
                if (video->canPlay())
                    video->play();
            } else
                video->pause();
            event->setDefaultHandled();
        }
    }
}

}
