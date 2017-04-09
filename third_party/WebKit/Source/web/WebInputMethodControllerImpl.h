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
  explicit WebInputMethodControllerImpl(WebLocalFrameImpl* owner_frame);
  ~WebInputMethodControllerImpl() override;

  static WebInputMethodControllerImpl* FromFrame(LocalFrame*);

  // WebInputMethodController overrides.
  bool SetComposition(const WebString& text,
                      const WebVector<WebCompositionUnderline>& underlines,
                      const WebRange& replacement_range,
                      int selection_start,
                      int selection_end) override;
  bool CommitText(const WebString& text,
                  const WebVector<WebCompositionUnderline>& underlines,
                  const WebRange& replacement_range,
                  int relative_caret_position) override;
  bool FinishComposingText(
      ConfirmCompositionBehavior selection_behavior) override;
  WebTextInputInfo TextInputInfo() override;
  WebTextInputType TextInputType() override;

  void SetSuppressNextKeypressEvent(bool suppress) {
    suppress_next_keypress_event_ = suppress;
  }

  DECLARE_TRACE();

 private:
  LocalFrame* GetFrame() const;
  InputMethodController& GetInputMethodController() const;
  WebPlugin* FocusedPluginIfInputMethodSupported() const;

  WeakMember<WebLocalFrameImpl> web_local_frame_;
  bool suppress_next_keypress_event_;
};
}  // namespace blink

#endif
