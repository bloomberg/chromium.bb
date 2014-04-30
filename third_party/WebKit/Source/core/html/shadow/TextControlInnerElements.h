/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextControlInnerElements_h
#define TextControlInnerElements_h

#include "core/html/HTMLDivElement.h"
#include "core/speech/SpeechInputListener.h"
#include "wtf/Forward.h"

namespace WebCore {

class SpeechInput;

class TextControlInnerContainer FINAL : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerContainer> create(Document&);
protected:
    TextControlInnerContainer(Document&);
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
};

class EditingViewPortElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<EditingViewPortElement> create(Document&);

protected:
    EditingViewPortElement(Document&);
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;

private:
    virtual bool supportsFocus() const OVERRIDE { return false; }
};

class TextControlInnerTextElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<TextControlInnerTextElement> create(Document&);

    virtual void defaultEventHandler(Event*) OVERRIDE;

private:
    TextControlInnerTextElement(Document&);
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;
    virtual bool supportsFocus() const OVERRIDE { return false; }
};

class SearchFieldDecorationElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<SearchFieldDecorationElement> create(Document&);

    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual bool willRespondToMouseClickEvents() OVERRIDE;

private:
    SearchFieldDecorationElement(Document&);
    virtual const AtomicString& shadowPseudoId() const OVERRIDE;
    virtual bool supportsFocus() const OVERRIDE { return false; }
};

class SearchFieldCancelButtonElement FINAL : public HTMLDivElement {
public:
    static PassRefPtr<SearchFieldCancelButtonElement> create(Document&);

    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual bool willRespondToMouseClickEvents() OVERRIDE;

private:
    SearchFieldCancelButtonElement(Document&);
    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual bool supportsFocus() const OVERRIDE { return false; }

    bool m_capturing;
};

#if ENABLE(INPUT_SPEECH)

class InputFieldSpeechButtonElement FINAL
    : public HTMLDivElement,
      public SpeechInputListener {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(InputFieldSpeechButtonElement);
public:
    enum SpeechInputState {
        Idle,
        Recording,
        Recognizing,
    };

    static PassRefPtr<InputFieldSpeechButtonElement> create(Document&);
    virtual ~InputFieldSpeechButtonElement();

    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual void defaultEventHandler(Event*) OVERRIDE;
    virtual bool willRespondToMouseClickEvents() OVERRIDE;
    virtual bool isInputFieldSpeechButtonElement() const OVERRIDE { return true; }
    SpeechInputState state() const { return m_state; }
    void startSpeechInput();
    void stopSpeechInput();

    // SpeechInputListener methods.
    virtual void didCompleteRecording(int) OVERRIDE;
    virtual void didCompleteRecognition(int) OVERRIDE;
    virtual void setRecognitionResult(int, const SpeechInputResultArray&) OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    InputFieldSpeechButtonElement(Document&);
    SpeechInput* speechInput();
    void setState(SpeechInputState state);
    virtual bool isMouseFocusable() const OVERRIDE { return false; }
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;

    bool m_capturing;
    SpeechInputState m_state;
    int m_listenerId;
    SpeechInputResultArray m_results;
};

DEFINE_TYPE_CASTS(InputFieldSpeechButtonElement, Element, element, element->isInputFieldSpeechButtonElement(), element.isInputFieldSpeechButtonElement());

#endif // ENABLE(INPUT_SPEECH)

} // namespace

#endif
