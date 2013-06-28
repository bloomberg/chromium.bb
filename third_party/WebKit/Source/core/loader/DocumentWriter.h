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
 * 3.  Neither the name of Adam Barth. ("Adam Barth") nor the names of
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

#ifndef DocumentWriter_h
#define DocumentWriter_h

#include "core/loader/TextResourceDecoderBuilder.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class DocumentParser;
class Frame;
class KURL;
class SecurityOrigin;
class TextResourceDecoder;

class DocumentWriter {
    WTF_MAKE_NONCOPYABLE(DocumentWriter);
public:
    explicit DocumentWriter(Frame*);

    // This is only called by ScriptController::executeScriptIfJavaScriptURL
    // and always contains the result of evaluating a javascript: url.
    void replaceDocument(const String&, Document* ownerDocument);

    void begin();
    void begin(const KURL&, bool dispatchWindowObjectAvailable = true, Document* ownerDocument = 0);
    void addData(const char* bytes, size_t length);
    void end();
    
    void setFrame(Frame* frame) { m_frame = frame; }

    const String& mimeType() const { return m_decoderBuilder.mimeType(); }
    void setMIMEType(const String& type) { m_decoderBuilder.setMIMEType(type); }
    void setEncoding(const String& encoding, bool userChosen) { m_decoderBuilder.setEncoding(encoding, userChosen); }

    // Exposed for DocumentParser::appendBytes.
    void reportDataReceived();

    void setDocumentWasLoadedAsPartOfNavigation();

private:
    PassRefPtr<Document> createDocument(const KURL&);
    void clear();

    Frame* m_frame;

    bool m_hasReceivedSomeData;
    TextResourceDecoderBuilder m_decoderBuilder;

    RefPtr<TextResourceDecoder> m_decoder;
    RefPtr<DocumentParser> m_parser;

    enum WriterState {
        NotStartedWritingState,
        StartedWritingState,
        FinishedWritingState,
    };
    WriterState m_state;
};

} // namespace WebCore

#endif // DocumentWriter_h
