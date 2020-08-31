// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_controller.h"

#include <utility>

#include "build/build_config.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "ui/gfx/text_elider.h"

namespace javascript_dialogs {
namespace {

AppModalDialogObserver* app_modal_dialog_observer = nullptr;

// Control maximum sizes of various texts passed to us from javascript.
#if defined(OS_POSIX) && !defined(OS_MACOSX)
// Two-dimensional eliding.  Reformat the text of the message dialog
// inserting line breaks because otherwise a single long line can overflow
// the message dialog (and crash/hang the GTK, depending on the version).
const int kMessageTextMaxRows = 32;
const int kMessageTextMaxCols = 132;
const int kDefaultPromptMaxRows = 24;
const int kDefaultPromptMaxCols = 132;

base::string16 EnforceMaxTextSize(const base::string16& in_string) {
  base::string16 out_string;
  gfx::ElideRectangleString(in_string, kMessageTextMaxRows, kMessageTextMaxCols,
                            false, &out_string);
  return out_string;
}

base::string16 EnforceMaxPromptSize(const base::string16& in_string) {
  base::string16 out_string;
  gfx::ElideRectangleString(in_string, kDefaultPromptMaxRows,
                            kDefaultPromptMaxCols, false, &out_string);
  return out_string;
}

#else
// One-dimensional eliding.  Trust the window system to break the string
// appropriately, but limit its overall length to something reasonable.
const size_t kMessageTextMaxSize = 2000;
const size_t kDefaultPromptMaxSize = 2000;

base::string16 EnforceMaxTextSize(const base::string16& in_string) {
  base::string16 out_string;
  gfx::ElideString(in_string, kMessageTextMaxSize, &out_string);
  return out_string;
}

base::string16 EnforceMaxPromptSize(const base::string16& in_string) {
  base::string16 out_string;
  gfx::ElideString(in_string, kDefaultPromptMaxSize, &out_string);
  return out_string;
}
#endif

}  // namespace

ChromeJavaScriptDialogExtraData::ChromeJavaScriptDialogExtraData()
    : has_already_shown_a_dialog_(false),
      suppress_javascript_messages_(false) {}

AppModalDialogController::AppModalDialogController(
    content::WebContents* web_contents,
    ExtraDataMap* extra_data_map,
    const base::string16& title,
    content::JavaScriptDialogType javascript_dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    bool display_suppress_checkbox,
    bool is_before_unload_dialog,
    bool is_reload,
    content::JavaScriptDialogManager::DialogClosedCallback callback)
    : title_(title),
      completed_(false),
      valid_(true),
      view_(nullptr),
      web_contents_(web_contents),
      extra_data_map_(extra_data_map),
      javascript_dialog_type_(javascript_dialog_type),
      message_text_(EnforceMaxTextSize(message_text)),
      default_prompt_text_(EnforceMaxPromptSize(default_prompt_text)),
      display_suppress_checkbox_(display_suppress_checkbox),
      is_before_unload_dialog_(is_before_unload_dialog),
      is_reload_(is_reload),
      callback_(std::move(callback)),
      use_override_prompt_text_(false) {}

AppModalDialogController::~AppModalDialogController() {
  CompleteDialog();
}

void AppModalDialogController::ShowModalDialog() {
  view_ = AppModalDialogManager::GetInstance()->view_factory()->Run(this);
  view_->ShowAppModalDialog();
  if (app_modal_dialog_observer)
    app_modal_dialog_observer->Notify(this);
}

void AppModalDialogController::ActivateModalDialog() {
  DCHECK(view_);
  view_->ActivateAppModalDialog();
}

void AppModalDialogController::CloseModalDialog() {
  DCHECK(view_);
  view_->CloseAppModalDialog();
}

void AppModalDialogController::CompleteDialog() {
  if (!completed_) {
    completed_ = true;
    AppModalDialogQueue::GetInstance()->ShowNextDialog();
  }
}

bool AppModalDialogController::IsValid() {
  return valid_;
}

void AppModalDialogController::Invalidate() {
  if (!valid_)
    return;

  valid_ = false;
  CallDialogClosedCallback(false, base::string16());
  if (view_)
    CloseModalDialog();
}

void AppModalDialogController::OnCancel(bool suppress_js_messages) {
  // We need to do this before WM_DESTROY (WindowClosing()) as any parent frame
  // will receive its activation messages before this dialog receives
  // WM_DESTROY. The parent frame would then try to activate any modal dialogs
  // that were still open in the ModalDialogQueue, which would send activation
  // back to this one. The framework should be improved to handle this, so this
  // is a temporary workaround.
  CompleteDialog();

  NotifyDelegate(false, base::string16(), suppress_js_messages);
}

void AppModalDialogController::OnAccept(const base::string16& prompt_text,
                                        bool suppress_js_messages) {
  base::string16 prompt_text_to_use = prompt_text;
  // This is only for testing.
  if (use_override_prompt_text_)
    prompt_text_to_use = override_prompt_text_;

  CompleteDialog();
  NotifyDelegate(true, prompt_text_to_use, suppress_js_messages);
}

void AppModalDialogController::OnClose() {
  NotifyDelegate(false, base::string16(), false);
}

void AppModalDialogController::SetOverridePromptText(
    const base::string16& override_prompt_text) {
  override_prompt_text_ = override_prompt_text;
  use_override_prompt_text_ = true;
}

void AppModalDialogController::NotifyDelegate(bool success,
                                              const base::string16& user_input,
                                              bool suppress_js_messages) {
  if (!valid_)
    return;

  CallDialogClosedCallback(success, user_input);

  // The close callback above may delete web_contents_, thus removing the extra
  // data from the map owned by ::AppModalDialogManager. Make sure
  // to only use the data if still present. http://crbug.com/236476
  auto extra_data = extra_data_map_->find(web_contents_);
  if (extra_data != extra_data_map_->end()) {
    extra_data->second.has_already_shown_a_dialog_ = true;
    extra_data->second.suppress_javascript_messages_ = suppress_js_messages;
  }

  // On Views, we can end up coming through this code path twice :(.
  // See crbug.com/63732.
  valid_ = false;
}

void AppModalDialogController::CallDialogClosedCallback(
    bool success,
    const base::string16& user_input) {
  if (!callback_.is_null())
    std::move(callback_).Run(success, user_input);
}

AppModalDialogObserver::AppModalDialogObserver() {
  DCHECK(!app_modal_dialog_observer);
  app_modal_dialog_observer = this;
}

AppModalDialogObserver::~AppModalDialogObserver() {
  DCHECK(app_modal_dialog_observer);
  app_modal_dialog_observer = nullptr;
}

}  // namespace javascript_dialogs
