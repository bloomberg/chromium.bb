/*
 * Copyright (C) 2010. Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/DocumentWriter.h"

#include "bindings/v8/ScriptController.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/html/PluginDocument.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/loader/SinkDocument.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Settings.h"
#include "core/platform/text/SegmentedString.h"
#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

DocumentWriter::DocumentWriter(Frame* frame)
    : m_frame(frame)
    , m_hasReceivedSomeData(false)
    , m_state(NotStartedWritingState)
{
}

// This is only called by ScriptController::executeScriptIfJavaScriptURL
// and always contains the result of evaluating a javascript: url.
// This is the <iframe src="javascript:'html'"> case.
void DocumentWriter::replaceDocument(const String& source, Document* ownerDocument)
{
    m_frame->loader()->stopAllLoaders();
    begin(m_frame->document()->url(), true, ownerDocument);

    if (!source.isNull()) {
        if (!m_hasReceivedSomeData) {
            m_hasReceivedSomeData = true;
            m_frame->document()->setCompatibilityMode(Document::NoQuirksMode);
        }

        // FIXME: This should call DocumentParser::appendBytes instead of append
        // to support RawDataDocumentParsers.
        if (DocumentParser* parser = m_frame->document()->parser()) {
            parser->pinToMainThread();
            // Because we're pinned to the main thread we don't need to worry about
            // passing ownership of the source string.
            parser->append(source.impl());
        }
    }

    end();
}

void DocumentWriter::clear()
{
    m_decoder = 0;
    m_decoderBuilder.clear();
    m_hasReceivedSomeData = false;
}

void DocumentWriter::begin()
{
    begin(KURL());
}

PassRefPtr<Document> DocumentWriter::createDocument(const KURL& url)
{
    return DOMImplementation::createDocument(mimeType(), m_frame, url, m_frame->inViewSourceMode());
}

void DocumentWriter::begin(const KURL& urlReference, bool dispatch, Document* ownerDocument)
{
    // We grab a local copy of the URL because it's easy for callers to supply
    // a URL that will be deallocated during the execution of this function.
    // For example, see <https://bugs.webkit.org/show_bug.cgi?id=66360>.
    KURL url = urlReference;

    // Create a new document before clearing the frame, because it may need to
    // inherit an aliased security context.
    RefPtr<Document> document = createDocument(url);
    
    // If the new document is for a Plugin but we're supposed to be sandboxed from Plugins,
    // then replace the document with one whose parser will ignore the incoming data (bug 39323)
    if (document->isPluginDocument() && document->isSandboxed(SandboxPlugins))
        document = SinkDocument::create(m_frame, url);

    // FIXME: Do we need to consult the content security policy here about blocked plug-ins?

    bool shouldReuseDefaultView = m_frame->loader()->stateMachine()->isDisplayingInitialEmptyDocument() && m_frame->document()->isSecureTransitionTo(url);

    RefPtr<DOMWindow> originalDOMWindow;
    if (shouldReuseDefaultView)
        originalDOMWindow = m_frame->domWindow();
    m_frame->loader()->clear(!shouldReuseDefaultView, !shouldReuseDefaultView);
    clear();

    if (!shouldReuseDefaultView)
        m_frame->setDOMWindow(DOMWindow::create(m_frame));
    else {
        // Note that the old Document is still attached to the DOMWindow; the
        // setDocument() call below will detach the old Document.
        ASSERT(originalDOMWindow);
        m_frame->setDOMWindow(originalDOMWindow);
    }

    m_frame->loader()->setOutgoingReferrer(url);
    m_frame->domWindow()->setDocument(document);

    if (m_decoder)
        document->setDecoder(m_decoder.get());
    if (ownerDocument) {
        document->setCookieURL(ownerDocument->cookieURL());
        document->setSecurityOrigin(ownerDocument->securityOrigin());
    }

    m_frame->loader()->didBeginDocument(dispatch);

    document->implicitOpen();

    // We grab a reference to the parser so that we'll always send data to the
    // original parser, even if the document acquires a new parser (e.g., via
    // document.open).
    m_parser = document->parser();

    if (m_frame->view())
        m_frame->view()->setContentsSize(IntSize());

    m_state = StartedWritingState;
}

void DocumentWriter::reportDataReceived()
{
    ASSERT(m_decoder);
    if (m_hasReceivedSomeData)
        return;
    m_hasReceivedSomeData = true;
    if (m_decoder->encoding().usesVisualOrdering())
        m_frame->document()->setVisuallyOrdered();
}

void DocumentWriter::addData(const char* bytes, size_t length)
{
    // Check that we're inside begin()/end().
    // FIXME: Change these to ASSERT once https://bugs.webkit.org/show_bug.cgi?id=80427 has
    // been resolved.
    if (m_state == NotStartedWritingState)
        CRASH();
    if (m_state == FinishedWritingState)
        CRASH();

    ASSERT(m_parser);
    if (!m_decoder && m_parser->needsDecoder() && 0 < length)
        m_decoder = m_decoderBuilder.buildFor(m_frame->document());
    size_t consumedChars = m_parser->appendBytes(bytes, length);
    if (consumedChars)
        reportDataReceived();
}

void DocumentWriter::end()
{
    ASSERT(m_frame->page());
    ASSERT(m_frame->document());

    // The parser is guaranteed to be released after this point. begin() would
    // have to be called again before we can start writing more data.
    m_state = FinishedWritingState;

    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it can be deleted by checkCompleted(), 
    // so we'll add a protective refcount
    RefPtr<Frame> protector(m_frame);

    if (!m_parser)
        return;
    if (!m_decoder && m_parser->needsDecoder())
        m_decoder = m_decoderBuilder.buildFor(m_frame->document());
    size_t consumedChars = m_parser->flush();
    if (consumedChars)
        reportDataReceived();
    if (!m_parser)
        return;
    m_parser->finish();
    m_parser = 0;
}

void DocumentWriter::setDocumentWasLoadedAsPartOfNavigation()
{
    ASSERT(m_parser && !m_parser->isStopped());
    m_parser->setDocumentWasLoadedAsPartOfNavigation();
}

} // namespace WebCore
