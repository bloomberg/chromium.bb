// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputMethodControllerImpl_h
#define WebInputMethodControllerImpl_h

#include "platform/heap/Handle.h"
#include "public/web/WebCompositionUnderline.h"
#include "public/web/WebInputMethodController.h"

namespace blink {

class InputMethodController;
class LocalFrame;
class WebLocalFrameImpl;
class WebPlugin;
class WebRange;
class WebString;

class WebInputMethodControllerImpl : public WebInputMethodController {
  WTF_MAKE_NONCOPYABLE(WebInputMethodControllerImpl);

 public:
  explicit WebInputMethodControllerImpl(WebLocalFrameImpl* ownerFrame);
  ~WebInputMethodControllerImpl() override;

  static WebInputMethodControllerImpl* fromFrame(LocalFrame*);

  // WebInputMethodController overrides.
  bool setComposition(const WebString& text,
                      const WebVector<WebCompositionUnderline>& underlines,
                      const WebRange& replacementRange,
                      int selectionStart,
                      int selectionEnd) override;
  bool commitText(const WebString& text,
                  const WebVector<WebCompositionUnderline>& underlines,
                  const WebRange& replacementRange,
                  int relativeCaretPosition) override;
  bool finishComposingText(
      ConfirmCompositionBehavior selectionBehavior) override;
  WebTextInputInfo textInputInfo() override;
  WebTextInputType textInputType() override;

  void setSuppressNextKeypressEvent(bool suppress) {
    m_suppressNextKeypressEvent = suppress;
  }

  DECLARE_TRACE();

 private:
  LocalFrame* frame() const;
  InputMethodController& inputMethodController() const;
  WebPlugin* focusedPluginIfInputMethodSupported() const;

  WeakMember<WebLocalFrameImpl> m_webLocalFrame;
  bool m_suppressNextKeypressEvent;
};
}  // namespace blink

#endif
