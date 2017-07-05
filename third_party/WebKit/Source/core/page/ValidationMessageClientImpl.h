/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ValidationMessageClientImpl_h
#define ValidationMessageClientImpl_h

#include "core/CoreExport.h"
#include "core/page/PopupOpeningObserver.h"
#include "core/page/ValidationMessageClient.h"
#include "platform/Timer.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LocalFrameView;
class PageOverlay;
class WebViewBase;

class CORE_EXPORT ValidationMessageClientImpl final
    : public GarbageCollectedFinalized<ValidationMessageClientImpl>,
      public NON_EXPORTED_BASE(ValidationMessageClient),
      private PopupOpeningObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ValidationMessageClientImpl);

 public:
  static ValidationMessageClientImpl* Create(WebViewBase&);
  ~ValidationMessageClientImpl() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  ValidationMessageClientImpl(WebViewBase&);
  void CheckAnchorStatus(TimerBase*);
  LocalFrameView* CurrentView();

  void ShowValidationMessage(const Element& anchor,
                             const String& message,
                             TextDirection message_dir,
                             const String& sub_message,
                             TextDirection sub_message_dir) override;
  void HideValidationMessage(const Element& anchor) override;
  bool IsValidationMessageVisible(const Element& anchor) override;
  void DocumentDetached(const Document&) override;
  void WillBeDestroyed() override;
  void LayoutOverlay() override;
  void PaintOverlay() override;

  // PopupOpeningObserver function
  void WillOpenPopup() override;

  WebViewBase& web_view_;
  Member<const Element> current_anchor_;
  String message_;
  IntRect last_anchor_rect_in_screen_;
  float last_page_scale_factor_;
  double finish_time_;
  std::unique_ptr<TimerBase> timer_;
  std::unique_ptr<PageOverlay> overlay_;
};

}  // namespace blink

#endif
