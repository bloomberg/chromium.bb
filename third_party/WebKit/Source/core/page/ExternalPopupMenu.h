/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef ExternalPopupMenu_h
#define ExternalPopupMenu_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/PopupMenu.h"
#include "platform/Timer.h"
#include "platform/wtf/Compiler.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebScrollbar.h"
#include "public/web/WebExternalPopupMenuClient.h"

namespace blink {

class HTMLSelectElement;
class LocalFrame;
class WebExternalPopupMenu;
class WebMouseEvent;
class WebView;
struct WebPopupMenuInfo;

// The ExternalPopupMenu connects the actual implementation of the popup menu
// to the WebCore popup menu.
class CORE_EXPORT ExternalPopupMenu final : NON_EXPORTED_BASE(public PopupMenu),
                                            public WebExternalPopupMenuClient {
 public:
  ExternalPopupMenu(LocalFrame&, HTMLSelectElement&, WebView&);
  ~ExternalPopupMenu() override;

  // Fills |info| with the popup menu information contained in the
  // PopupMenuClient associated with this ExternalPopupMenu.
  // FIXME: public only for test access. Need to revert once gtest
  // helpers from chromium are available for blink.
  static void GetPopupMenuInfo(WebPopupMenuInfo&, HTMLSelectElement&);
  static int ToPopupMenuItemIndex(int index, HTMLSelectElement&);
  static int ToExternalPopupMenuItemIndex(int index, HTMLSelectElement&);

  DECLARE_VIRTUAL_TRACE();

 private:
  // PopupMenu methods:
  void Show() override;
  void Hide() override;
  void UpdateFromElement(UpdateReason) override;
  void DisconnectClient() override;

  // WebExternalPopupClient methods:
  void DidChangeSelection(int index) override;
  void DidAcceptIndex(int index) override;
  void DidAcceptIndices(const WebVector<int>& indices) override;
  void DidCancel() override;

  bool ShowInternal();
  void DispatchEvent(TimerBase*);
  void Update();

  Member<HTMLSelectElement> owner_element_;
  Member<LocalFrame> local_frame_;
  WebView& web_view_;
  std::unique_ptr<WebMouseEvent> synthetic_event_;
  TaskRunnerTimer<ExternalPopupMenu> dispatch_event_timer_;
  // The actual implementor of the show menu.
  WebExternalPopupMenu* web_external_popup_menu_;
  bool needs_update_ = false;
};

}  // namespace blink

#endif  // ExternalPopupMenu_h
