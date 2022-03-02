// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_

#include <list>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/dense_set.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

enum class ObservedUiEvents {
  kPreviewFormData,
  kFormDataFilled,
  kSuggestionShown,
  kNoEvent,
  kMaxValue = kNoEvent
};

class BrowserAutofillManagerTestDelegateImpl
    : public autofill::BrowserAutofillManagerTestDelegate {
 public:
  BrowserAutofillManagerTestDelegateImpl();

  BrowserAutofillManagerTestDelegateImpl(
      const BrowserAutofillManagerTestDelegateImpl&) = delete;
  BrowserAutofillManagerTestDelegateImpl& operator=(
      const BrowserAutofillManagerTestDelegateImpl&) = delete;

  ~BrowserAutofillManagerTestDelegateImpl() override;

  // Controls whether back-to-back events of |type|, except for the first one,
  // are ignored. This is useful for cross-iframe forms, where events such as
  // ObservedUiEvents::kFormDataFilled are triggered by each filled renderer
  // form.
  void SetIgnoreBackToBackMessages(ObservedUiEvents type, bool ignore);

  // autofill::BrowserAutofillManagerTestDelegate:
  void DidPreviewFormData() override;
  void DidFillFormData() override;
  void DidShowSuggestions() override;
  void OnTextFieldChanged() override;

  void SetExpectations(std::list<ObservedUiEvents> expected_events,
                       base::TimeDelta timeout = base::Seconds(0));
  bool Wait();

  void SetIsExpectingDynamicRefill(bool expect_refill) {
    is_expecting_dynamic_refill_ = expect_refill;
  }

 private:
  void FireEvent(ObservedUiEvents event);

  bool is_expecting_dynamic_refill_ = false;
  std::unique_ptr<EventWaiter<ObservedUiEvents>> event_waiter_;
  DenseSet<ObservedUiEvents> ignore_back_to_back_event_types_;
  ObservedUiEvents last_event_ = ObservedUiEvents::kNoEvent;
};

class AutofillUiTest : public InProcessBrowserTest,
                       public content::WebContentsObserver {
 public:
  AutofillUiTest(const AutofillUiTest&) = delete;
  AutofillUiTest& operator=(const AutofillUiTest&) = delete;

 protected:
  AutofillUiTest();
  ~AutofillUiTest() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void SendKeyToPage(content::WebContents* web_contents, const ui::DomKey key);
  void SendKeyToPageAndWait(ui::DomKey key,
                            std::list<ObservedUiEvents> expected_events);
  void SendKeyToPageAndWait(ui::DomKey key,
                            ui::DomCode code,
                            ui::KeyboardCode key_code,
                            std::list<ObservedUiEvents> expected_events);
  void DoNothingAndWait(unsigned seconds);
  void SendKeyToPopup(content::RenderFrameHost* render_frame_host,
                      const ui::DomKey key);
  // Send key to the render host view's widget if |widget| is null.
  void SendKeyToPopupAndWait(ui::DomKey key,
                             std::list<ObservedUiEvents> expected_events,
                             content::RenderWidgetHost* widget = nullptr);
  void SendKeyToPopupAndWait(ui::DomKey key,
                             ui::DomCode code,
                             ui::KeyboardCode key_code,
                             std::list<ObservedUiEvents> expected_events,
                             content::RenderWidgetHost* widget);
  void SendKeyToDataListPopup(ui::DomKey key);
  void SendKeyToDataListPopup(ui::DomKey key,
                              ui::DomCode code,
                              ui::KeyboardCode key_code);
  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  content::WebContents* GetWebContents();
  content::RenderViewHost* GetRenderViewHost();
  BrowserAutofillManager* GetBrowserAutofillManager();

  BrowserAutofillManagerTestDelegateImpl* test_delegate() {
    return &test_delegate_;
  }
  content::RenderWidgetHost::KeyPressEventCallback key_press_GetEventSink();

 private:
  // WebContentsObserver override:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  raw_ptr<content::RenderFrameHost> current_main_rfh_ = nullptr;
  BrowserAutofillManagerTestDelegateImpl test_delegate_;

  // KeyPressEventCallback that serves as a sink to ensure that every key press
  // event the tests create and have the WebContents forward is handled by some
  // key press event callback. It is necessary to have this sink because if no
  // key press event callback handles the event (at least on Mac), a DCHECK
  // ends up going off that the |event| doesn't have an |os_event| associated
  // with it.
  content::RenderWidgetHost::KeyPressEventCallback key_press_event_sink_;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> disable_animation_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_UITEST_H_
