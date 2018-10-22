// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_

#include <memory>
#include <queue>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/macros.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/text_edit_action.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

class PlatformInputHandler;

// This class is responsible for processing all events and gestures for
// ContentElement.
class VR_UI_EXPORT ContentInputDelegate : public PlatformUiInputDelegate {
 public:
  ContentInputDelegate();
  explicit ContentInputDelegate(PlatformInputHandler* content);
  ~ContentInputDelegate() override;

  // Text Input specific.
  void OnFocusChanged(bool focused);
  void OnWebInputEdited(const EditedText& info, bool commit);

  void OnSwapContents(int new_content_id);

  // This should be called in reponse to selection and composition changes.
  // The given callback will may be called asynchronously with the updated text
  // state. This is because we may have to query content for the text after the
  // index change.
  VIRTUAL_FOR_MOCKS void OnWebInputIndicesChanged(
      int selection_start,
      int selection_end,
      int composition_start,
      int compositon_end,
      base::OnceCallback<void(const TextInputInfo&)> callback);

  void OnWebInputTextChangedForTest(const base::string16& text) {
    OnWebInputTextChanged(text);
  }

  void ClearTextInputState();

 protected:
  void SendGestureToTarget(std::unique_ptr<InputEvent> event) override;

 private:
  enum TextRequestState {
    kNoPendingRequest,
    kRequested,
    kResponseReceived,
  };
  bool ContentGestureIsLocked(InputEvent::Type type);
  void OnWebInputTextChanged(const base::string16& text);

  int content_id_ = 0;
  int locked_content_id_ = 0;

  EditedText last_keyboard_edit_;
  TextRequestState pending_text_request_state_ = kNoPendingRequest;
  TextInputInfo pending_text_input_info_;
  std::queue<base::OnceCallback<void(const TextInputInfo&)>>
      update_state_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ContentInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
