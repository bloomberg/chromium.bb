// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_IMPL_H_

#include "ash/public/cpp/select_to_speak_event_handler_delegate.h"

namespace ash {

// SelectToSpeakEventHandlerDelegate receives mouse and key events from Ash's
// event handler and forwards them to the Select-to-Speak extension in Chrome.
class SelectToSpeakEventHandlerDelegateImpl
    : public SelectToSpeakEventHandlerDelegate {
 public:
  SelectToSpeakEventHandlerDelegateImpl();

  SelectToSpeakEventHandlerDelegateImpl(
      const SelectToSpeakEventHandlerDelegateImpl&) = delete;
  SelectToSpeakEventHandlerDelegateImpl& operator=(
      const SelectToSpeakEventHandlerDelegateImpl&) = delete;

  virtual ~SelectToSpeakEventHandlerDelegateImpl();

 private:
  // SelectToSpeakEventHandlerDelegate:
  void DispatchKeyEvent(const ui::KeyEvent& event) override;
  void DispatchMouseEvent(const ui::MouseEvent& event) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_EVENT_HANDLER_DELEGATE_IMPL_H_
