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
#include "core/fetch/TextResourceDecoder.h"

namespace WebCore {

DecodedDataDocumentParser::DecodedDataDocumentParser(Document* document)
    : DocumentParser(document)
{
}

size_t DecodedDataDocumentParser::appendBytes(const char* data, size_t length)
{
    if (!length)
        return 0;

    if (isStopped())
        return 0;

    String decoded = document()->decoder()->decode(data, length);
    document()->setEncoding(document()->decoder()->encoding());

    if (decoded.isEmpty())
        return 0;

    size_t consumedChars = decoded.length();
    append(decoded.releaseImpl());

    return consumedChars;
}

size_t DecodedDataDocumentParser::flush()
{
    if (isStopped())
        return 0;

    // null decoder indicates there is no data received.
    // We have nothing to do in that case.
    TextResourceDecoder* decoder = document()->decoder();
    if (!decoder)
        return 0;
    String remainingData = decoder->flush();
    document()->setEncoding(document()->decoder()->encoding());
    if (remainingData.isEmpty())
        return 0;

    size_t consumedChars = remainingData.length();
    append(remainingData.releaseImpl());

    return consumedChars;
}

};
