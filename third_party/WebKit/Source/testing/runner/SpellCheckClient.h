/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef SpellCheckClient_h
#define SpellCheckClient_h

#include "MockSpellCheck.h"
#include "public/platform/WebNonCopyable.h"
#include "public/testing/WebTask.h"
#include "public/web/WebSpellCheckClient.h"

namespace WebTestRunner {

class WebTestDelegate;
class WebTestProxyBase;

class SpellCheckClient : public blink::WebSpellCheckClient, public blink::WebNonCopyable {
public:
    explicit SpellCheckClient(WebTestProxyBase*);
    virtual ~SpellCheckClient();

    void setDelegate(WebTestDelegate*);

    WebTaskList* taskList() { return &m_taskList; }
    MockSpellCheck* mockSpellCheck() { return &m_spellcheck; }

    // blink::WebSpellCheckClient implementation.
    virtual void spellCheck(const blink::WebString&, int& offset, int& length, blink::WebVector<blink::WebString>* optionalSuggestions);
    virtual void checkTextOfParagraph(const blink::WebString&, blink::WebTextCheckingTypeMask, blink::WebVector<blink::WebTextCheckingResult>*);
    virtual void requestCheckingOfText(const blink::WebString&,
                                       const blink::WebVector<uint32_t>&,
                                       const blink::WebVector<unsigned>&,
                                       blink::WebTextCheckingCompletion*);
    virtual blink::WebString autoCorrectWord(const blink::WebString&);

private:
    void finishLastTextCheck();

    // The mock spellchecker used in spellCheck().
    MockSpellCheck m_spellcheck;

    blink::WebString m_lastRequestedTextCheckString;
    blink::WebTextCheckingCompletion* m_lastRequestedTextCheckingCompletion;

    WebTaskList m_taskList;

    WebTestDelegate* m_delegate;

    WebTestProxyBase* m_webTestProxy;
};

}

#endif // SpellCheckClient_h
