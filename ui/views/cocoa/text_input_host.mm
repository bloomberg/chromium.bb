// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/text_input_host.h"

#include "ui/base/ime/text_input_client.h"
#include "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views_bridge_mac/bridged_native_widget_impl.h"

namespace views {

TextInputHost::TextInputHost(BridgedNativeWidgetHostImpl* host_impl)
    : host_impl_(host_impl) {}

TextInputHost::~TextInputHost() {}

ui::TextInputClient* TextInputHost::GetTextInputClient() const {
  return text_input_client_;
}

void TextInputHost::SetTextInputClient(
    ui::TextInputClient* new_text_input_client) {
  if (pending_text_input_client_ == new_text_input_client)
    return;

  // This method may cause the IME window to dismiss, which may cause it to
  // insert text (e.g. to replace marked text with "real" text). That should
  // happen in the old -inputContext (which AppKit stores a reference to).
  // Unfortunately, the only way to invalidate the the old -inputContext is to
  // invoke -[NSApp updateWindows], which also wants a reference to the _new_
  // -inputContext. So put the new inputContext in |pendingTextInputClient_| and
  // only use it for -inputContext.
  ui::TextInputClient* old_text_input_client = text_input_client_;

  // Since dismissing an IME may insert text, a misbehaving IME or a
  // ui::TextInputClient that acts on InsertChar() to change focus a second time
  // may invoke -setTextInputClient: recursively; with [NSApp updateWindows]
  // still on the stack. Calling [NSApp updateWindows] recursively may upset
  // an IME. Since the rest of this method is only to decide whether to call
  // updateWindows, and we're already calling it, just bail out.
  if (text_input_client_ != pending_text_input_client_) {
    pending_text_input_client_ = new_text_input_client;
    return;
  }

  // Start by assuming no need to invoke -updateWindows.
  text_input_client_ = new_text_input_client;
  pending_text_input_client_ = new_text_input_client;

  if (host_impl_->bridge_impl_ &&
      host_impl_->bridge_impl_->NeedsUpdateWindows()) {
    text_input_client_ = old_text_input_client;
    [NSApp updateWindows];
    // Note: |pending_text_input_client_| (and therefore +[NSTextInputContext
    // currentInputContext] may have changed if called recursively.
    text_input_client_ = pending_text_input_client_;
  }
}

void TextInputHost::GetHasInputContext(bool* has_input_context) {
  *has_input_context = false;

  // If the textInputClient_ does not exist, return nil since this view does not
  // conform to NSTextInputClient protocol.
  if (!pending_text_input_client_)
    return;

  // If a menu is active, and -[NSView interpretKeyEvents:] asks for the
  // input context, return nil. This ensures the action message is sent to
  // the view, rather than any NSTextInputClient a subview has installed.
  bool has_menu_controller = false;
  host_impl_->GetHasMenuController(&has_menu_controller);
  if (has_menu_controller)
    return;

  // When not in an editable mode, or while entering passwords
  // (http://crbug.com/23219), we don't want to show IME candidate windows.
  // Returning nil prevents this view from getting messages defined as part of
  // the NSTextInputClient protocol.
  switch (pending_text_input_client_->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return;
    default:
      *has_input_context = true;
  }
}

}  // namespace views
