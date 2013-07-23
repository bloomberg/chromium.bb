/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "core/dom/DecodedDataDocumentParser.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/loader/TextResourceDecoder.h"
#include "wtf/text/TextEncodingRegistry.h"

namespace WebCore {

namespace {

class TitleEncodingFixer {
public:
    explicit TitleEncodingFixer(Document* document)
        : m_document(document)
        , m_firstEncoding(document->decoder()->encoding())
    {
    }

    // It's possible for the encoding of the document to change while we're decoding
    // data. That can only occur while we're processing the <head> portion of the
    // document. There isn't much user-visible content in the <head>, but there is
    // the <title> element. This function detects that situation and re-decodes the
    // document's title so that the user doesn't see an incorrectly decoded title
    // in the title bar.
    inline void fixTitleEncodingIfNeeded()
    {
        if (m_firstEncoding == m_document->decoder()->encoding())
            return; // In the common case, the encoding doesn't change and there isn't any work to do.
        fixTitleEncoding();
    }

private:
    void fixTitleEncoding();

    Document* m_document;
    WTF::TextEncoding m_firstEncoding;
};

void TitleEncodingFixer::fixTitleEncoding()
{
    RefPtr<Element> titleElement = m_document->titleElement();
    if (!titleElement
        || titleElement->firstElementChild()
        || m_firstEncoding != Latin1Encoding()
        || !titleElement->textContent().containsOnlyLatin1())
        return; // Either we don't have a title yet or something bizzare as happened and we give up.
    CString originalBytes = titleElement->textContent().latin1();
    OwnPtr<TextCodec> codec = newTextCodec(m_document->decoder()->encoding());
    String correctlyDecodedTitle = codec->decode(originalBytes.data(), originalBytes.length(), true);
    titleElement->setTextContent(correctlyDecodedTitle, IGNORE_EXCEPTION);
}

}

DecodedDataDocumentParser::DecodedDataDocumentParser(Document* document)
    : DocumentParser(document)
{
}

size_t DecodedDataDocumentParser::appendBytes(const char* data, size_t length)
{
    if (!length)
        return 0;

    TitleEncodingFixer encodingFixer(document());

    String decoded = document()->decoder()->decode(data, length);

    encodingFixer.fixTitleEncodingIfNeeded();

    if (decoded.isEmpty())
        return 0;

    size_t consumedChars = decoded.length();
    append(decoded.releaseImpl());

    return consumedChars;
}

size_t DecodedDataDocumentParser::flush()
{
    // null decoder indicates there is no data received.
    // We have nothing to do in that case.
    TextResourceDecoder* decoder = document()->decoder();
    if (!decoder)
        return 0;
    String remainingData = decoder->flush();
    if (remainingData.isEmpty())
        return 0;

    size_t consumedChars = remainingData.length();
    append(remainingData.releaseImpl());

    return consumedChars;
}

};
