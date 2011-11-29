// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/message_box_view.h"

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/message_box_flags.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "views/views_delegate.h"

const int kDefaultMessageWidth = 320;

namespace views {

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, public:

MessageBoxView::MessageBoxView(int dialog_flags,
                               const string16& message,
                               const string16& default_prompt,
                               int message_width)
    : message_label_(new Label(message)),
      prompt_field_(NULL),
      icon_(NULL),
      checkbox_(NULL),
      message_width_(message_width) {
  Init(dialog_flags, default_prompt);
}

MessageBoxView::MessageBoxView(int dialog_flags,
                               const string16& message,
                               const string16& default_prompt)
    : message_label_(new Label(message)),
      prompt_field_(NULL),
      icon_(NULL),
      checkbox_(NULL),
      message_width_(kDefaultMessageWidth) {
  Init(dialog_flags, default_prompt);
}

MessageBoxView::~MessageBoxView() {}

string16 MessageBoxView::GetInputText() {
  return prompt_field_ ? prompt_field_->text() : string16();
}

bool MessageBoxView::IsCheckBoxSelected() {
  return checkbox_ ? checkbox_->checked() : false;
}

void MessageBoxView::SetIcon(const SkBitmap& icon) {
  if (!icon_)
    icon_ = new ImageView();
  icon_->SetImage(icon);
  icon_->SetBounds(0, 0, icon.width(), icon.height());
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxLabel(const string16& label) {
  if (!checkbox_)
    checkbox_ = new Checkbox(label);
  else
    checkbox_->SetText(label);
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxSelected(bool selected) {
  if (!checkbox_)
    return;
  checkbox_->SetChecked(selected);
}

void MessageBoxView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, View overrides:

void MessageBoxView::ViewHierarchyChanged(bool is_add,
                                          View* parent,
                                          View* child) {
  if (child == this && is_add) {
    if (prompt_field_)
      prompt_field_->SelectAll();

    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_ALERT, true);
  }
}

bool MessageBoxView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // We only accepts Ctrl-C.
  DCHECK(accelerator.key_code() == 'C' && accelerator.IsCtrlDown());

  // We must not intercept Ctrl-C when we have a text box and it's focused.
  if (prompt_field_ && prompt_field_->HasFocus())
    return false;

  if (!ViewsDelegate::views_delegate)
    return false;

  ui::Clipboard* clipboard = ViewsDelegate::views_delegate->GetClipboard();
  if (!clipboard)
    return false;

  ui::ScopedClipboardWriter scw(clipboard);
  scw.WriteText(message_label_->GetText());
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, private:

void MessageBoxView::Init(int dialog_flags,
                          const string16& default_prompt) {
  message_label_->SetMultiLine(true);
  message_label_->SetAllowCharacterBreak(true);
  if (dialog_flags & ui::MessageBoxFlags::kAutoDetectAlignment) {
    // Determine the alignment and directionality based on the first character
    // with strong directionality.
    base::i18n::TextDirection direction =
        base::i18n::GetFirstStrongCharacterDirection(
            message_label_->GetText());
    Label::Alignment alignment;
    if (direction == base::i18n::RIGHT_TO_LEFT)
      alignment = Label::ALIGN_RIGHT;
    else
      alignment = Label::ALIGN_LEFT;
    // In addition, we should set the RTL alignment mode as
    // AUTO_DETECT_ALIGNMENT so that the alignment will not be flipped around
    // in RTL locales.
    message_label_->set_rtl_alignment_mode(Label::AUTO_DETECT_ALIGNMENT);
    message_label_->SetHorizontalAlignment(alignment);
  } else {
    message_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  }

  if (dialog_flags & ui::MessageBoxFlags::kFlagHasPromptField) {
    prompt_field_ = new Textfield;
    prompt_field_->SetText(default_prompt);
  }

  ResetLayoutManager();
}

void MessageBoxView::ResetLayoutManager() {
  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  gfx::Size icon_size;
  if (icon_)
    icon_size = icon_->GetPreferredSize();

  // Add the column set for the message displayed at the top of the dialog box.
  // And an icon, if one has been set.
  const int message_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(message_column_view_set_id);
  if (icon_) {
    column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                          GridLayout::FIXED, icon_size.width(),
                          icon_size.height());
    column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  }
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, message_width_, 0);

  // Column set for prompt Textfield, if one has been set.
  const int textfield_column_view_set_id = 1;
  if (prompt_field_) {
    column_set = layout->AddColumnSet(textfield_column_view_set_id);
    if (icon_) {
      column_set->AddPaddingColumn(
          0, icon_size.width() + kUnrelatedControlHorizontalSpacing);
    }
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

  // Column set for checkbox, if one has been set.
  const int checkbox_column_view_set_id = 2;
  if (checkbox_) {
    column_set = layout->AddColumnSet(checkbox_column_view_set_id);
    if (icon_) {
      column_set->AddPaddingColumn(
          0, icon_size.width() + kUnrelatedControlHorizontalSpacing);
    }
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
  }

  layout->StartRow(0, message_column_view_set_id);
  if (icon_)
    layout->AddView(icon_);

  layout->AddView(message_label_);

  if (prompt_field_) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, textfield_column_view_set_id);
    layout->AddView(prompt_field_);
  }

  if (checkbox_) {
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, checkbox_column_view_set_id);
    layout->AddView(checkbox_);
  }

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace views
