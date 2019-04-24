// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_UIA_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_UIA_WIN_H_

#include <ole2.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include "base/win/atl.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"

namespace content {

class AccessibilityEventRecorderUia : public AccessibilityEventRecorder {
 public:
  AccessibilityEventRecorderUia(
      BrowserAccessibilityManager* manager = nullptr,
      base::ProcessId pid = 0,
      const base::StringPiece& application_name_match_pattern =
          base::StringPiece());
  ~AccessibilityEventRecorderUia() override;

  static std::unique_ptr<AccessibilityEventRecorder> CreateUia(
      BrowserAccessibilityManager* manager = nullptr,
      base::ProcessId pid = 0,
      const base::StringPiece& application_name_match_pattern =
          base::StringPiece());

 private:
  Microsoft::WRL::ComPtr<IUIAutomation> uia_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request_;
  static AccessibilityEventRecorderUia* instance_;

  // An implementation of various UIA interface that forward event
  // notifications to the owning event recorder.
  class EventHandler : public CComObjectRootEx<CComMultiThreadModel>,
                       public IUIAutomationFocusChangedEventHandler {
   public:
    explicit EventHandler();
    virtual ~EventHandler();

    void Init(AccessibilityEventRecorderUia* owner,
              Microsoft::WRL::ComPtr<IUIAutomationElement> root);

    BEGIN_COM_MAP(EventHandler)
    COM_INTERFACE_ENTRY(IUIAutomationFocusChangedEventHandler)
    END_COM_MAP()

    // IUIAutomationFocusChangedEventHandler interface.
    STDMETHOD(HandleFocusChangedEvent)(IUIAutomationElement* sender) override;

    // Points to the event recorder to receive notifications.
    AccessibilityEventRecorderUia* owner_;

   private:
    std::string GetSenderInfo(IUIAutomationElement* sender);

    Microsoft::WRL::ComPtr<IUIAutomationElement> root_;

    DISALLOW_COPY_AND_ASSIGN(EventHandler);
  };
  Microsoft::WRL::ComPtr<CComObject<EventHandler>> uia_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorderUia);
};

}  // namespace content

#endif
