// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputMethodControllerImpl_h
#define WebInputMethodControllerImpl_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "public/web/WebImeTextSpan.h"
#include "public/web/WebInputMethodController.h"

namespace blink {

class InputMethodController;
class LocalFrame;
class WebLocalFrameImpl;
class WebPlugin;
class WebRange;
class WebString;

class CORE_EXPORT WebInputMethodControllerImpl
    : public WebInputMethodController {
  DISALLOW_NEW();

 public:
  explicit WebInputMethodControllerImpl(WebLocalFrameImpl& web_frame);
  ~WebInputMethodControllerImpl() override;

  // WebInputMethodController overrides.
  bool SetComposition(const WebString& text,
                      const WebVector<WebImeTextSpan>& ime_text_spans,
                      const WebRange& replacement_range,
                      int selection_start,
                      int selection_end) override;
  bool CommitText(const WebString& text,
                  const WebVector<WebImeTextSpan>& ime_text_spans,
                  const WebRange& replacement_range,
                  int relative_caret_position) override;
  bool FinishComposingText(
      ConfirmCompositionBehavior selection_behavior) override;
  WebTextInputInfo TextInputInfo() override;
  int ComputeWebTextInputNextPreviousFlags() override;
  WebTextInputType TextInputType() override;
  WebRange CompositionRange() override;

  WebRange GetSelectionOffsets() const;

  void Trace(blink::Visitor*);

 private:
  LocalFrame* GetFrame() const;
  InputMethodController& GetInputMethodController() const;
  WebPlugin* FocusedPluginIfInputMethodSupported() const;

  const Member<WebLocalFrameImpl> web_frame_;

  DISALLOW_COPY_AND_ASSIGN(WebInputMethodControllerImpl);
};
}  // namespace blink

#endif
