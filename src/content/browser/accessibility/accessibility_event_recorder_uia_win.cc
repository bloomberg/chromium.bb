// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder_uia_win.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/base/win/atl_module.h"

namespace content {

namespace {

}  // namespace

// static
AccessibilityEventRecorderUia* AccessibilityEventRecorderUia::instance_ =
    nullptr;

// static
std::unique_ptr<AccessibilityEventRecorder>
AccessibilityEventRecorderUia::CreateUia(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern) {
  return std::make_unique<AccessibilityEventRecorderUia>(
      manager, pid, application_name_match_pattern);
}

AccessibilityEventRecorderUia::AccessibilityEventRecorderUia(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern)
    : AccessibilityEventRecorder(manager) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " AccessibilityEventRecorder at a time.";

  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomation, &uia_);
  CHECK(uia_.Get());

  uia_->CreateCacheRequest(&cache_request_);
  CHECK(cache_request_.Get());

  // Find the root IUIAutomationElement for the content window
  HWND hwnd = manager->GetRoot()->GetTargetForNativeAccessibilityEvent();
  CHECK(hwnd);

  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  uia_->ElementFromHandle(hwnd, &root);
  CHECK(root.Get());

  // Create the event handler
  ui::win::CreateATLModuleIfNeeded();
  CHECK(
      SUCCEEDED(CComObject<EventHandler>::CreateInstance(&uia_event_handler_)));
  uia_event_handler_->Init(this, root);

  // Subscribe to focus events
  uia_->AddFocusChangedEventHandler(nullptr, uia_event_handler_.Get());

  instance_ = this;
}

AccessibilityEventRecorderUia::~AccessibilityEventRecorderUia() {
  uia_event_handler_->owner_ = nullptr;
  uia_event_handler_.Reset();
  instance_ = nullptr;
}

AccessibilityEventRecorderUia::EventHandler::EventHandler() {}

AccessibilityEventRecorderUia::EventHandler::~EventHandler() {}

void AccessibilityEventRecorderUia::EventHandler::Init(
    AccessibilityEventRecorderUia* owner,
    Microsoft::WRL::ComPtr<IUIAutomationElement> root) {
  owner_ = owner;
  root_ = root;
}

STDMETHODIMP
AccessibilityEventRecorderUia::EventHandler::HandleFocusChangedEvent(
    IUIAutomationElement* sender) {
  if (!owner_)
    return S_OK;

  base::win::ScopedBstr id;
  sender->get_CurrentAutomationId(id.Receive());
  base::win::ScopedVariant id_variant(id, id.Length());

  Microsoft::WRL::ComPtr<IUIAutomationElement> element_found;
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;

  owner_->uia_->CreatePropertyCondition(UIA_AutomationIdPropertyId, id_variant,
                                        &condition);
  CHECK(condition);
  root_->FindFirst(TreeScope::TreeScope_Subtree, condition.Get(),
                   &element_found);
  if (!element_found) {
    VLOG(1) << "Ignoring UIA focus event outside our frame";
    return S_OK;
  }

  std::string log = base::StringPrintf("UIA_AutomationFocusChangedEventId %s",
                                       GetSenderInfo(sender).c_str());
  owner_->OnEvent(log);

  return S_OK;
}

std::string AccessibilityEventRecorderUia::EventHandler::GetSenderInfo(
    IUIAutomationElement* sender) {
  std::string sender_info = "";

  auto append_property = [&](const char* name, auto getter) {
    base::win::ScopedBstr bstr;
    (sender->*getter)(bstr.Receive());
    if (bstr.Length() > 0) {
      sender_info +=
          base::StringPrintf("%s%s=%s", sender_info.empty() ? "" : ", ", name,
                             BstrToUTF8(bstr).c_str());
    }
  };

  append_property("role", &IUIAutomationElement::get_CurrentAriaRole);
  append_property("name", &IUIAutomationElement::get_CurrentName);

  if (!sender_info.empty())
    sender_info = "on " + sender_info;
  return sender_info;
}

}  // namespace content
