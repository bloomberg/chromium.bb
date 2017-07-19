// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInputMethodControllerImpl_h
#define WebInputMethodControllerImpl_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "public/web/WebCompositionUnderline.h"
#include "public/web/WebInputMethodController.h"

namespace blink {

class InputMethodController;
class LocalFrame;
class WebLocalFrameBase;
class WebPlugin;
class WebRange;
class WebString;

class CORE_EXPORT WebInputMethodControllerImpl
    : public NON_EXPORTED_BASE(WebInputMethodController) {
  WTF_MAKE_NONCOPYABLE(WebInputMethodControllerImpl);
  DISALLOW_NEW();

 public:
  explicit WebInputMethodControllerImpl(WebLocalFrameBase& web_frame);
  ~WebInputMethodControllerImpl() override;

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
  WebRange GetSelectionOffsets() const;

  DECLARE_TRACE();

 private:
  LocalFrame* GetFrame() const;
  InputMethodController& GetInputMethodController() const;
  WebPlugin* FocusedPluginIfInputMethodSupported() const;

  const Member<WebLocalFrameBase> web_frame_;
};
}  // namespace blink

#endif
