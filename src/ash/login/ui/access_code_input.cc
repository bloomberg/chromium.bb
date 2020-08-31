// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/access_code_input.h"

#include "ash/public/cpp/login_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {
// Identifier for focus traversal.
constexpr int kFixedLengthInputGroup = 1;

constexpr int kAccessCodeFlexLengthWidthDp = 192;
constexpr int kAccessCodeFlexUnderlineThicknessDp = 1;
constexpr int kAccessCodeFontSizeDeltaDp = 4;
constexpr int kObscuredGlyphSpacingDp = 6;

constexpr int kAccessCodeInputFieldWidthDp = 24;
constexpr int kAccessCodeBetweenInputFieldsGapDp = 8;

constexpr SkColor kTextColor = SK_ColorWHITE;
}  // namespace

FlexCodeInput::FlexCodeInput(OnInputChange on_input_change,
                             OnEnter on_enter,
                             OnEscape on_escape,
                             bool obscure_pin)
    : on_input_change_(std::move(on_input_change)),
      on_enter_(std::move(on_enter)),
      on_escape_(std::move(on_escape)) {
  DCHECK(on_input_change_);
  DCHECK(on_enter_);
  DCHECK(on_escape_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  code_field_ = AddChildView(std::make_unique<views::Textfield>());
  code_field_->set_controller(this);
  code_field_->SetTextColor(login_constants::kAuthMethodsTextColor);
  code_field_->SetFontList(views::Textfield::GetDefaultFontList().Derive(
      kAccessCodeFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::NORMAL));
  code_field_->SetBorder(views::CreateSolidSidedBorder(
      0, 0, kAccessCodeFlexUnderlineThicknessDp, 0, kTextColor));
  code_field_->SetBackgroundColor(SK_ColorTRANSPARENT);
  code_field_->SetFocusBehavior(FocusBehavior::ALWAYS);
  code_field_->SetPreferredSize(
      gfx::Size(kAccessCodeFlexLengthWidthDp, kAccessCodeInputFieldHeightDp));

  if (obscure_pin) {
    code_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    code_field_->SetObscuredGlyphSpacing(kObscuredGlyphSpacingDp);
  } else {
    code_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
  }
}

FlexCodeInput::~FlexCodeInput() = default;

void FlexCodeInput::InsertDigit(int value) {
  DCHECK_LE(0, value);
  DCHECK_GE(9, value);
  if (code_field_->GetEnabled()) {
    code_field_->SetText(code_field_->GetText() +
                         base::NumberToString16(value));
    on_input_change_.Run(true);
  }
}

void FlexCodeInput::Backspace() {
  // Instead of just adjusting code_field_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  auto* view = static_cast<views::View*>(code_field_);
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  // This triggers ContentsChanged(), which calls |on_input_change_|.
}

base::Optional<std::string> FlexCodeInput::GetCode() const {
  base::string16 code = code_field_->GetText();
  if (!code.length()) {
    return base::nullopt;
  }
  return base::UTF16ToUTF8(code);
}

void FlexCodeInput::SetInputColor(SkColor color) {
  code_field_->SetTextColor(color);
}

void FlexCodeInput::SetInputEnabled(bool input_enabled) {
  code_field_->SetEnabled(input_enabled);
}

void FlexCodeInput::ClearInput() {
  code_field_->SetText(base::string16());
  on_input_change_.Run(false);
}

void FlexCodeInput::RequestFocus() {
  code_field_->RequestFocus();
}

void FlexCodeInput::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  const bool has_content = new_contents.length() > 0;
  on_input_change_.Run(has_content);
}

bool FlexCodeInput::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  // Only handle keys.
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  // Default handling for events with Alt modifier like spoken feedback.
  if (key_event.IsAltDown())
    return false;

  // FlexCodeInput class responds to a limited subset of key press events.
  // All events not handled below are sent to |code_field_|.
  const ui::KeyboardCode key_code = key_event.key_code();

  // Allow using tab for keyboard navigation.
  if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB)
    return false;

  if (key_code == ui::VKEY_RETURN) {
    if (GetCode().has_value()) {
      on_enter_.Run();
    }
    return true;
  }

  if (key_code == ui::VKEY_ESCAPE) {
    on_escape_.Run();
    return true;
  }

  // We only expect digits in the PIN, so we swallow all letters.
  if (key_code >= ui::VKEY_A && key_code <= ui::VKEY_Z)
    return true;

  return false;
}

void AccessibleInputField::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Textfield::GetAccessibleNodeData(node_data);
  // The following property setup is needed to match the custom behavior of
  // pin input. It results in the following a11y vocalizations:
  // * when input field is empty: "Next number, {current field index} of
  // {number of fields}"
  // * when input field is populated: "{value}, {current field index} of
  // {number of fields}"
  node_data->RemoveState(ax::mojom::State::kEditable);
  node_data->role = ax::mojom::Role::kListItem;
  base::string16 description =
      views::Textfield::GetText().empty() ? accessible_description_ : GetText();
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                base::UTF16ToUTF8(description));
}

bool AccessibleInputField::IsGroupFocusTraversable() const {
  return false;
}

views::View* AccessibleInputField::GetSelectedViewForGroup(int group) {
  return parent() ? parent()->GetSelectedViewForGroup(group) : nullptr;
}

void AccessibleInputField::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    RequestFocusWithPointer(event->details().primary_pointer_type());
    return;
  }

  views::Textfield::OnGestureEvent(event);
}

FixedLengthCodeInput::FixedLengthCodeInput(int length,
                                           OnInputChange on_input_change,
                                           OnEnter on_enter,
                                           OnEscape on_escape,
                                           bool obscure_pin)
    : on_input_change_(std::move(on_input_change)),
      on_enter_(std::move(on_enter)),
      on_escape_(std::move(on_escape)) {
  DCHECK_LT(0, length);
  DCHECK(on_input_change_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kAccessCodeBetweenInputFieldsGapDp));
  SetGroup(kFixedLengthInputGroup);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  for (int i = 0; i < length; ++i) {
    auto* field = new AccessibleInputField();
    field->set_controller(this);
    field->SetPreferredSize(
        gfx::Size(kAccessCodeInputFieldWidthDp, kAccessCodeInputFieldHeightDp));
    field->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    field->SetBackgroundColor(SK_ColorTRANSPARENT);
    if (obscure_pin) {
      field->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    } else {
      field->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
    }
    field->SetTextColor(kTextColor);
    field->SetFontList(views::Textfield::GetDefaultFontList().Derive(
        kAccessCodeFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
        gfx::Font::Weight::NORMAL));
    field->SetBorder(views::CreateSolidSidedBorder(
        0, 0, kAccessCodeInputFieldUnderlineThicknessDp, 0, kTextColor));
    field->SetGroup(kFixedLengthInputGroup);
    field->set_accessible_description(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_PIN_REQUEST_NEXT_NUMBER_PROMPT));
    input_fields_.push_back(field);
    AddChildView(field);
  }
}

FixedLengthCodeInput::~FixedLengthCodeInput() = default;

// Inserts |value| into the |active_field_| and moves focus to the next field
// if it exists.
void FixedLengthCodeInput::InsertDigit(int value) {
  DCHECK_LE(0, value);
  DCHECK_GE(9, value);

  ActiveField()->SetText(base::NumberToString16(value));
  bool was_last_field = IsLastFieldActive();

  // Moving focus is delayed by using PostTask to allow for proper
  // a11y announcements. Without that some of them are skipped.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FixedLengthCodeInput::FocusNextField,
                                weak_ptr_factory_.GetWeakPtr()));

  on_input_change_.Run(was_last_field, GetCode().has_value());
}

// Clears input from the |active_field_|. If |active_field| is empty moves
// focus to the previous field (if exists) and clears input there.
void FixedLengthCodeInput::Backspace() {
  if (ActiveInput().empty()) {
    FocusPreviousField();
  }

  ActiveField()->SetText(base::string16());
  on_input_change_.Run(IsLastFieldActive(), false /*complete*/);
}

// Returns access code as string if all fields contain input.
base::Optional<std::string> FixedLengthCodeInput::GetCode() const {
  std::string result;
  size_t length;
  for (auto* field : input_fields_) {
    length = field->GetText().length();
    if (!length)
      return base::nullopt;

    DCHECK_EQ(1u, length);
    base::StrAppend(&result, {base::UTF16ToUTF8(field->GetText())});
  }
  return result;
}

void FixedLengthCodeInput::SetInputColor(SkColor color) {
  for (auto* field : input_fields_) {
    field->SetTextColor(color);
  }
}

bool FixedLengthCodeInput::IsGroupFocusTraversable() const {
  return false;
}

views::View* FixedLengthCodeInput::GetSelectedViewForGroup(int group) {
  return ActiveField();
}

void FixedLengthCodeInput::RequestFocus() {
  ActiveField()->RequestFocus();
}

void FixedLengthCodeInput::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kGroup;
}

bool FixedLengthCodeInput::HandleKeyEvent(views::Textfield* sender,
                                          const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  // Default handling for events with Alt modifier like spoken feedback.
  if (key_event.IsAltDown())
    return false;

  // FixedLengthCodeInput class responds to limited subset of key press
  // events. All key pressed events not handled below are ignored.
  const ui::KeyboardCode key_code = key_event.key_code();
  if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB) {
    // Allow using tab for keyboard navigation.
    return false;
  } else if (key_code >= ui::VKEY_0 && key_code <= ui::VKEY_9) {
    InsertDigit(key_code - ui::VKEY_0);
  } else if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9) {
    InsertDigit(key_code - ui::VKEY_NUMPAD0);
  } else if (key_code == ui::VKEY_LEFT) {
    FocusPreviousField();
  } else if (key_code == ui::VKEY_RIGHT) {
    // Do not allow to leave empty field when moving focus with arrow key.
    if (!ActiveInput().empty())
      FocusNextField();
  } else if (key_code == ui::VKEY_BACK) {
    Backspace();
  } else if (key_code == ui::VKEY_RETURN) {
    if (GetCode().has_value())
      on_enter_.Run();
  } else if (key_code == ui::VKEY_ESCAPE) {
    on_escape_.Run();
  }

  return true;
}

bool FixedLengthCodeInput::HandleMouseEvent(views::Textfield* sender,
                                            const ui::MouseEvent& mouse_event) {
  if (!(mouse_event.IsOnlyLeftMouseButton() ||
        mouse_event.IsOnlyRightMouseButton())) {
    return false;
  }

  // Move focus to the field that was selected with mouse input.
  for (size_t i = 0; i < input_fields_.size(); ++i) {
    if (input_fields_[i] == sender) {
      active_input_index_ = i;
      RequestFocus();
      break;
    }
  }

  return true;
}

bool FixedLengthCodeInput::HandleGestureEvent(
    views::Textfield* sender,
    const ui::GestureEvent& gesture_event) {
  if (gesture_event.details().type() != ui::EventType::ET_GESTURE_TAP)
    return false;

  // Move focus to the field that was selected with gesture.
  for (size_t i = 0; i < input_fields_.size(); ++i) {
    if (input_fields_[i] == sender) {
      active_input_index_ = i;
      RequestFocus();
      break;
    }
  }

  return true;
}

void FixedLengthCodeInput::SetInputEnabled(bool input_enabled) {
  NOTIMPLEMENTED();
}

void FixedLengthCodeInput::ClearInput() {
  NOTIMPLEMENTED();
}

void FixedLengthCodeInput::FocusPreviousField() {
  if (active_input_index_ == 0)
    return;

  --active_input_index_;
  ActiveField()->RequestFocus();
}

void FixedLengthCodeInput::FocusNextField() {
  if (IsLastFieldActive())
    return;

  ++active_input_index_;
  ActiveField()->RequestFocus();
}

bool FixedLengthCodeInput::IsLastFieldActive() const {
  return active_input_index_ == (static_cast<int>(input_fields_.size()) - 1);
}

AccessibleInputField* FixedLengthCodeInput::ActiveField() const {
  return input_fields_[active_input_index_];
}

const base::string16& FixedLengthCodeInput::ActiveInput() const {
  return ActiveField()->GetText();
}

}  // namespace ash