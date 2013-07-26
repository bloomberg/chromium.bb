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

#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/loader/TextResourceDecoder.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Settings.h"
#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

PassRefPtr<DocumentWriter> DocumentWriter::create(Document* document, const String& mimeType, const String& encoding, bool encodingUserChoosen)
{
    return adoptRef(new DocumentWriter(document, mimeType, encoding, encodingUserChoosen));
}

DocumentWriter::DocumentWriter(Document* document, const String& mimeType, const String& encoding, bool encodingUserChoosen)
    : m_document(document)
    , m_hasReceivedSomeData(false)
    , m_decoderBuilder(mimeType, encoding, encodingUserChoosen)
    // We grab a reference to the parser so that we'll always send data to the
    // original parser, even if the document acquires a new parser (e.g., via
    // document.open).
    , m_parser(m_document->implicitOpen())
{
    if (m_document->frame()) {
        if (FrameView* view = m_document->frame()->view())
            view->setContentsSize(IntSize());
    }
}

DocumentWriter::~DocumentWriter()
{
}

void DocumentWriter::appendReplacingData(const String& source)
{
    ASSERT(!m_hasReceivedSomeData);
    m_hasReceivedSomeData = true;
    m_document->setCompatibilityMode(Document::NoQuirksMode);

    // FIXME: This should call DocumentParser::appendBytes instead of append
    // to support RawDataDocumentParsers.
    if (DocumentParser* parser = m_document->parser()) {
        parser->pinToMainThread();
        // Because we're pinned to the main thread we don't need to worry about
        // passing ownership of the source string.
        parser->append(source.impl());
    }
}

void DocumentWriter::reportDataReceived()
{
    ASSERT(m_decoder);
    if (m_hasReceivedSomeData)
        return;
    m_hasReceivedSomeData = true;
    if (m_decoder->encoding().usesVisualOrdering())
        m_document->setVisuallyOrdered();
}

void DocumentWriter::addData(const char* bytes, size_t length)
{
    ASSERT(m_parser);
    if (!m_decoder && m_parser->needsDecoder() && 0 < length)
        m_decoder = m_decoderBuilder.buildFor(m_document);
    // appendBytes() can result replacing DocumentLoader::m_writer.
    RefPtr<DocumentWriter> protectingThis(this);
    size_t consumedChars = m_parser->appendBytes(bytes, length);
    if (consumedChars)
        reportDataReceived();
}

void DocumentWriter::end()
{
    ASSERT(m_document);

    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it can be deleted by checkCompleted(),
    // so we'll add a protective refcount
    RefPtr<Frame> protector(m_document->frame());

    if (!m_parser)
        return;

    if (!m_decoder && m_parser->needsDecoder())
        m_decoder = m_decoderBuilder.buildFor(m_document);
    // flush() can result replacing DocumentLoader::m_writer.
    RefPtr<DocumentWriter> protectingThis(this);
    size_t consumedChars = m_parser->flush();
    if (consumedChars)
        reportDataReceived();
    if (!m_parser)
        return;

    m_parser->finish();
    m_parser = 0;
    m_document = 0;
}

void DocumentWriter::setDocumentWasLoadedAsPartOfNavigation()
{
    ASSERT(m_parser && !m_parser->isStopped());
    m_parser->setDocumentWasLoadedAsPartOfNavigation();
}

} // namespace WebCore
