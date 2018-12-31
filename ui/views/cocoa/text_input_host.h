// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_TEXT_INPUT_HOST_H_
#define UI_VIEWS_COCOA_TEXT_INPUT_HOST_H_

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace views {

class BridgedNativeWidgetHostImpl;

class VIEWS_EXPORT TextInputHost {
 public:
  TextInputHost(BridgedNativeWidgetHostImpl* host_impl);
  ~TextInputHost();

  // Set the current TextInputClient.
  void SetTextInputClient(ui::TextInputClient* new_text_input_client);

  // Return true if -[NSView inputContext] should return a non-nil value.
  void GetHasInputContext(bool* has_input_context);

  // Return a pointer to the host's ui::TextInputClient.
  // TODO(ccameron): Remove the need for this call.
  ui::TextInputClient* GetTextInputClient() const;

 private:
  // Weak. If non-null the TextInputClient of the currently focused views::View
  // in the hierarchy rooted at the root view of |host_impl_|. Owned by the
  // focused views::View.
  ui::TextInputClient* text_input_client_ = nullptr;

  // The TextInputClient about to be set. Requests for a new -inputContext will
  // use this, but while the input is changing the NSView still needs to service
  // IME requests using the old |text_input_client_|.
  ui::TextInputClient* pending_text_input_client_ = nullptr;

  BridgedNativeWidgetHostImpl* const host_impl_;

  DISALLOW_COPY_AND_ASSIGN(TextInputHost);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_TEXT_INPUT_HOST_H_
