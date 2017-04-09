// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PopupMenuImpl_h
#define PopupMenuImpl_h

#include "core/page/PagePopupClient.h"
#include "platform/PopupMenu.h"

namespace blink {

class ChromeClientImpl;
class PagePopup;
class HTMLElement;
class HTMLHRElement;
class HTMLOptGroupElement;
class HTMLOptionElement;
class HTMLSelectElement;

class PopupMenuImpl final : public PopupMenu, public PagePopupClient {
 public:
  static PopupMenuImpl* Create(ChromeClientImpl*, HTMLSelectElement&);
  ~PopupMenuImpl() override;
  DECLARE_VIRTUAL_TRACE();

  void Update();

  void Dispose();

 private:
  PopupMenuImpl(ChromeClientImpl*, HTMLSelectElement&);

  class ItemIterationContext;
  void AddOption(ItemIterationContext&, HTMLOptionElement&);
  void AddOptGroup(ItemIterationContext&, HTMLOptGroupElement&);
  void AddSeparator(ItemIterationContext&, HTMLHRElement&);
  void AddElementStyle(ItemIterationContext&, HTMLElement&);

  // PopupMenu functions:
  void Show() override;
  void Hide() override;
  void DisconnectClient() override;
  void UpdateFromElement(UpdateReason) override;

  // PagePopupClient functions:
  void WriteDocument(SharedBuffer*) override;
  void SelectFontsFromOwnerDocument(Document&) override;
  void SetValueAndClosePopup(int, const String&) override;
  void SetValue(const String&) override;
  void ClosePopup() override;
  Element& OwnerElement() override;
  float ZoomFactor() override { return 1.0; }
  Locale& GetLocale() override;
  void DidClosePopup() override;

  Member<ChromeClientImpl> chrome_client_;
  Member<HTMLSelectElement> owner_element_;
  PagePopup* popup_;
  bool needs_update_;
};

}  // namespace blink

#endif  // PopupMenuImpl_h
