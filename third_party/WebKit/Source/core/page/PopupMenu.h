// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PopupMenu_h
#define PopupMenu_h

#include "core/CoreExport.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PopupMenu.h"
#include "platform/heap/Handle.h"

namespace blink {

class ChromeClient;
class PagePopup;
class HTMLElement;
class HTMLHRElement;
class HTMLOptGroupElement;
class HTMLOptionElement;
class HTMLSelectElement;

class CORE_EXPORT PopupMenu : public GarbageCollectedFinalized<PopupMenu>,
                              public PagePopupClient {
 public:
  static PopupMenu* Create(ChromeClient*, HTMLSelectElement&);
  virtual ~PopupMenu();
  DECLARE_VIRTUAL_TRACE();

  void Update();

  void Dispose();

  virtual void Show();
  virtual void Hide();
  enum UpdateReason {
    kBySelectionChange,
    kByStyleChange,
    kByDOMChange,
  };
  virtual void UpdateFromElement(UpdateReason);
  virtual void DisconnectClient();

 protected:
  PopupMenu(ChromeClient*, HTMLSelectElement&);
  HTMLSelectElement* GetOwnerElement() const { return owner_element_.Get(); }
  void ClearOwnerElement() { owner_element_ = nullptr; }

 private:
  class ItemIterationContext;
  void AddOption(ItemIterationContext&, HTMLOptionElement&);
  void AddOptGroup(ItemIterationContext&, HTMLOptGroupElement&);
  void AddSeparator(ItemIterationContext&, HTMLHRElement&);
  void AddElementStyle(ItemIterationContext&, HTMLElement&);

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

  Member<ChromeClient> chrome_client_;
  Member<HTMLSelectElement> owner_element_;
  PagePopup* popup_;
  bool needs_update_;
};

}  // namespace blink

#endif  // PopupMenu_h
