/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MockWebSpeechInputController_h
#define MockWebSpeechInputController_h

#include "TestCommon.h"
#include "public/platform/WebNonCopyable.h"
#include "public/platform/WebRect.h"
#include "public/testing/WebTask.h"
#include "public/web/WebSpeechInputController.h"
#include "public/web/WebSpeechInputResult.h"
#include <map>
#include <string>
#include <vector>

namespace blink {
class WebSecurityOrigin;
class WebSpeechInputListener;
class WebString;
}

namespace WebTestRunner {

class WebTestDelegate;

class MockWebSpeechInputController : public blink::WebSpeechInputController, public blink::WebNonCopyable {
public:
    explicit MockWebSpeechInputController(blink::WebSpeechInputListener*);
    ~MockWebSpeechInputController();

    void addMockRecognitionResult(const blink::WebString& result, double confidence, const blink::WebString& language);
    void setDumpRect(bool);
    void clearResults();
    void setDelegate(WebTestDelegate*);

    // WebSpeechInputController implementation:
    virtual bool startRecognition(int requestId, const blink::WebRect& elementRect, const blink::WebString& language, const blink::WebString& grammar, const blink::WebSecurityOrigin&) OVERRIDE;
    virtual void cancelRecognition(int requestId) OVERRIDE;
    virtual void stopRecording(int requestId) OVERRIDE;

    WebTaskList* taskList() { return &m_taskList; }

private:
    void speechTaskFired();

    class SpeechTask : public WebMethodTask<MockWebSpeechInputController> {
    public:
        SpeechTask(MockWebSpeechInputController*);
        void stop();

    private:
        virtual void runIfValid() OVERRIDE;
    };

    blink::WebSpeechInputListener* m_listener;

    WebTaskList m_taskList;
    SpeechTask* m_speechTask;

    bool m_recording;
    int m_requestId;
    blink::WebRect m_requestRect;
    std::string m_language;

    std::map<std::string, std::vector<blink::WebSpeechInputResult> > m_recognitionResults;
    std::vector<blink::WebSpeechInputResult> m_resultsForEmptyLanguage;
    bool m_dumpRect;

    WebTestDelegate* m_delegate;
};

}

#endif // MockWebSpeechInputController_h
