// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/native_textfield_views.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "grit/app_locale_settings.h"
#include "grit/ui_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/range/range.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/render_text.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_views_model.h"
#include "ui/views/events/event.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/metrics.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "unicode/uchar.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

namespace {

// Text color for read only.
const SkColor kReadonlyTextColor = SK_ColorDKGRAY;

// Default "system" color for text cursor.
const SkColor kDefaultCursorColor = SK_ColorBLACK;

// Parameters to control cursor blinking.
const int kCursorVisibleTimeMs = 800;
const int kCursorInvisibleTimeMs = 500;

}  // namespace

namespace views {

const char NativeTextfieldViews::kViewClassName[] =
    "views/NativeTextfieldViews";

NativeTextfieldViews::NativeTextfieldViews(Textfield* parent)
    : textfield_(parent),
      ALLOW_THIS_IN_INITIALIZER_LIST(model_(new TextfieldViewsModel(this))),
      text_border_(new FocusableBorder()),
      is_cursor_visible_(false),
      is_drop_cursor_visible_(false),
      skip_input_method_cancel_composition_(false),
      initiating_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(cursor_timer_(this)),
      aggregated_clicks_(0),
      last_click_time_(),
      last_click_location_(),
      ALLOW_THIS_IN_INITIALIZER_LIST(touch_selection_controller_(
          TouchSelectionController::create(this))) {
  set_border(text_border_);

#if defined(OS_CHROMEOS)
  GetRenderText()->SetFontList(gfx::FontList(l10n_util::GetStringUTF8(
      IDS_UI_FONT_FAMILY_CROS)));
#else
  GetRenderText()->SetFontList(gfx::FontList(textfield_->font()));
#endif
  // Set the default text style.
  gfx::StyleRange default_style;
  default_style.foreground = textfield_->text_color();
  GetRenderText()->set_default_style(default_style);
  GetRenderText()->ApplyDefaultStyle();

  set_context_menu_controller(this);
  set_drag_controller(this);
}

NativeTextfieldViews::~NativeTextfieldViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, View overrides:

bool NativeTextfieldViews::OnMousePressed(const MouseEvent& event) {
  OnBeforeUserAction();
  TrackMouseClicks(event);
  // TODO: Remove once NativeTextfield implementations are consolidated to
  // Textfield.
  if (!textfield_->OnMousePressed(event))
    HandleMousePressEvent(event);
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::ExceededDragThresholdFromLastClickLocation(
    const MouseEvent& event) {
  gfx::Point location_delta = event.location().Subtract(last_click_location_);
  return ExceededDragThreshold(location_delta.x(), location_delta.y());
}

bool NativeTextfieldViews::OnMouseDragged(const MouseEvent& event) {
  // Don't adjust the cursor on a potential drag and drop, or if the mouse
  // movement from the last mouse click does not exceed the drag threshold.
  if (initiating_drag_ || !ExceededDragThresholdFromLastClickLocation(event))
    return true;

  OnBeforeUserAction();
  // TODO: Remove once NativeTextfield implementations are consolidated to
  // Textfield.
  if (!textfield_->OnMouseDragged(event)) {
    if (MoveCursorTo(event.location(), true))
      SchedulePaint();
  }
  OnAfterUserAction();
  return true;
}

void NativeTextfieldViews::OnMouseReleased(const MouseEvent& event) {
  OnBeforeUserAction();
  // TODO: Remove once NativeTextfield implementations are consolidated to
  // Textfield.
  textfield_->OnMouseReleased(event);
  // Cancel suspected drag initiations, the user was clicking in the selection.
  if (initiating_drag_ && MoveCursorTo(event.location(), false))
    SchedulePaint();
  initiating_drag_ = false;
  OnAfterUserAction();
}

bool NativeTextfieldViews::OnKeyPressed(const KeyEvent& event) {
  // OnKeyPressed/OnKeyReleased/OnFocus/OnBlur will never be invoked on
  // NativeTextfieldViews as it will never gain focus.
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::OnKeyReleased(const KeyEvent& event) {
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  if (!textfield_->enabled() || textfield_->read_only())
    return false;
  // TODO(msw): Can we support URL, FILENAME, etc.?
  *formats = ui::OSExchangeData::STRING;
  return true;
}

bool NativeTextfieldViews::CanDrop(const OSExchangeData& data) {
  return textfield_->enabled() && !textfield_->read_only() && data.HasString();
}

int NativeTextfieldViews::OnDragUpdated(const DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));
  bool in_selection = GetRenderText()->IsPointInSelection(event.location());
  is_drop_cursor_visible_ = !in_selection;
  // TODO(msw): Pan over text when the user drags to the visible text edge.
  OnCaretBoundsChanged();
  SchedulePaint();

  if (initiating_drag_) {
    if (in_selection)
      return ui::DragDropTypes::DRAG_NONE;
    return event.IsControlDown() ? ui::DragDropTypes::DRAG_COPY :
                                   ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
}

int NativeTextfieldViews::OnPerformDrop(const DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));
  DCHECK(!initiating_drag_ ||
         !GetRenderText()->IsPointInSelection(event.location()));
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;

  gfx::SelectionModel drop_destination_model =
      GetRenderText()->FindCursorPosition(event.location());
  string16 text;
  event.data().GetString(&text);
  text = GetTextForDisplay(text);

  // We'll delete the current selection for a drag and drop within this view.
  bool move = initiating_drag_ && !event.IsControlDown() &&
              event.source_operations() & ui::DragDropTypes::DRAG_MOVE;
  if (move) {
    gfx::SelectionModel selected;
    model_->GetSelectionModel(&selected);
    // Adjust the drop destination if it is on or after the current selection.
    size_t drop_destination = drop_destination_model.caret_pos();
    drop_destination -=
        selected.selection().Intersect(ui::Range(0, drop_destination)).length();
    model_->DeleteSelectionAndInsertTextAt(text, drop_destination);
  } else {
    model_->MoveCursorTo(drop_destination_model);
    // Drop always inserts text even if the textfield is not in insert mode.
    model_->InsertText(text);
  }
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
  return move ? ui::DragDropTypes::DRAG_MOVE : ui::DragDropTypes::DRAG_COPY;
}

void NativeTextfieldViews::OnDragDone() {
  initiating_drag_ = false;
  is_drop_cursor_visible_ = false;
}

void NativeTextfieldViews::OnPaint(gfx::Canvas* canvas) {
  text_border_->set_has_focus(textfield_->HasFocus());
  OnPaintBackground(canvas);
  PaintTextAndCursor(canvas);
  if (textfield_->draw_border())
    OnPaintBorder(canvas);
}

void NativeTextfieldViews::OnFocus() {
  NOTREACHED();
}

void NativeTextfieldViews::OnBlur() {
  NOTREACHED();
}

void NativeTextfieldViews::SelectRect(const gfx::Point& start,
                                      const gfx::Point& end) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  gfx::SelectionModel start_caret = GetRenderText()->FindCursorPosition(start);
  gfx::SelectionModel end_caret = GetRenderText()->FindCursorPosition(end);
  gfx::SelectionModel selection(
      ui::Range(start_caret.caret_pos(), end_caret.caret_pos()),
      end_caret.caret_affinity());

  OnBeforeUserAction();
  model_->SelectSelectionModel(selection);
  OnCaretBoundsChanged();
  SchedulePaint();
  OnAfterUserAction();
}

gfx::NativeCursor NativeTextfieldViews::GetCursor(const MouseEvent& event) {
  bool in_selection = GetRenderText()->IsPointInSelection(event.location());
  bool drag_event = event.type() == ui::ET_MOUSE_DRAGGED;
  bool text_cursor = !initiating_drag_ && (drag_event || !in_selection);
#if defined(USE_AURA)
  return text_cursor ? ui::kCursorIBeam : ui::kCursorNull;
#elif defined(OS_WIN)
  static HCURSOR ibeam = LoadCursor(NULL, IDC_IBEAM);
  static HCURSOR arrow = LoadCursor(NULL, IDC_ARROW);
  return text_cursor ? ibeam : arrow;
#endif
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ContextMenuController overrides:
void NativeTextfieldViews::ShowContextMenuForView(View* source,
                                                  const gfx::Point& point) {
  UpdateContextMenu();
  if (context_menu_runner_->RunMenuAt(GetWidget(), NULL,
          gfx::Rect(point, gfx::Size()), views::MenuItemView::TOPLEFT,
          MenuRunner::HAS_MNEMONICS) == MenuRunner::MENU_DELETED)
    return;
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, views::DragController overrides:
void NativeTextfieldViews::WriteDragDataForView(views::View* sender,
                                                const gfx::Point& press_pt,
                                                OSExchangeData* data) {
  DCHECK_NE(ui::DragDropTypes::DRAG_NONE,
            GetDragOperationsForView(sender, press_pt));
  data->SetString(GetSelectedText());
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnWriteDragData(data);
}

int NativeTextfieldViews::GetDragOperationsForView(views::View* sender,
                                                   const gfx::Point& p) {
  if (!textfield_->enabled() || textfield_->IsObscured() ||
      !GetRenderText()->IsPointInSelection(p))
    return ui::DragDropTypes::DRAG_NONE;
  if (sender == this && !textfield_->read_only())
    return ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY;
  return ui::DragDropTypes::DRAG_COPY;
}

bool NativeTextfieldViews::CanStartDragForView(View* sender,
                                               const gfx::Point& press_pt,
                                               const gfx::Point& p) {
  return GetRenderText()->IsPointInSelection(press_pt);
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, NativeTextifieldWrapper overrides:

string16 NativeTextfieldViews::GetText() const {
  return model_->GetText();
}

void NativeTextfieldViews::UpdateText() {
  model_->SetText(GetTextForDisplay(textfield_->text()));
  OnCaretBoundsChanged();
  SchedulePaint();
  textfield_->GetWidget()->NotifyAccessibilityEvent(
      textfield_, ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
}

void NativeTextfieldViews::AppendText(const string16& text) {
  if (text.empty())
    return;
  model_->Append(GetTextForDisplay(text));
  OnCaretBoundsChanged();
  SchedulePaint();
}

string16 NativeTextfieldViews::GetSelectedText() const {
  return model_->GetSelectedText();
}

void NativeTextfieldViews::SelectAll() {
  OnBeforeUserAction();
  model_->SelectAll();
  OnCaretBoundsChanged();
  SchedulePaint();
  OnAfterUserAction();
}

void NativeTextfieldViews::ClearSelection() {
  OnBeforeUserAction();
  model_->ClearSelection();
  OnCaretBoundsChanged();
  SchedulePaint();
  OnAfterUserAction();
}

void NativeTextfieldViews::UpdateBorder() {
  if (textfield_->draw_border()) {
    gfx::Insets insets = GetInsets();
    textfield_->SetHorizontalMargins(insets.left(), insets.right());
    textfield_->SetVerticalMargins(insets.top(), insets.bottom());
  } else {
    textfield_->SetHorizontalMargins(0, 0);
    textfield_->SetVerticalMargins(0, 0);
  }
}

void NativeTextfieldViews::UpdateTextColor() {
  SchedulePaint();
}

void NativeTextfieldViews::UpdateBackgroundColor() {
  // TODO(oshima): Background has to match the border's shape.
  set_background(
      Background::CreateSolidBackground(textfield_->background_color()));
  SchedulePaint();
}

void NativeTextfieldViews::UpdateCursorColor() {
  SchedulePaint();
}

void NativeTextfieldViews::UpdateReadOnly() {
  // Update the default text style.
  gfx::StyleRange default_style(GetRenderText()->default_style());
  default_style.foreground = textfield_->read_only() ? kReadonlyTextColor :
                                                       textfield_->text_color();
  GetRenderText()->set_default_style(default_style);
  GetRenderText()->ApplyDefaultStyle();

  SchedulePaint();
  OnTextInputTypeChanged();
}

void NativeTextfieldViews::UpdateFont() {
#if defined(OS_CHROMEOS)
  // For ChromeOS, we support a pre-defined font list per locale. UpdateFont()
  // only changes the font size, not the font family names.
  GetRenderText()->SetFontSize(textfield_->font().GetFontSize());
#else
  GetRenderText()->SetFontList(gfx::FontList(textfield_->font()));
#endif
  OnCaretBoundsChanged();
}

void NativeTextfieldViews::UpdateIsObscured() {
  GetRenderText()->SetObscured(textfield_->IsObscured());
  OnCaretBoundsChanged();
  SchedulePaint();
  OnTextInputTypeChanged();
}

void NativeTextfieldViews::UpdateEnabled() {
  SetEnabled(textfield_->enabled());
  SchedulePaint();
  OnTextInputTypeChanged();
}

gfx::Insets NativeTextfieldViews::CalculateInsets() {
  return GetInsets();
}

void NativeTextfieldViews::UpdateHorizontalMargins() {
  int left, right;
  if (!textfield_->GetHorizontalMargins(&left, &right))
    return;
  gfx::Insets inset = GetInsets();

  text_border_->SetInsets(inset.top(), left, inset.bottom(), right);
  OnCaretBoundsChanged();
}

void NativeTextfieldViews::UpdateVerticalMargins() {
  int top, bottom;
  if (!textfield_->GetVerticalMargins(&top, &bottom))
    return;
  gfx::Insets inset = GetInsets();
  text_border_->SetInsets(top, inset.left(), bottom, inset.right());
  OnCaretBoundsChanged();
}

bool NativeTextfieldViews::SetFocus() {
  return false;
}

View* NativeTextfieldViews::GetView() {
  return this;
}

gfx::NativeView NativeTextfieldViews::GetTestingHandle() const {
  NOTREACHED();
  return NULL;
}

bool NativeTextfieldViews::IsIMEComposing() const {
  return model_->HasCompositionText();
}

void NativeTextfieldViews::GetSelectedRange(ui::Range* range) const {
  *range = GetRenderText()->selection();
}

void NativeTextfieldViews::SelectRange(const ui::Range& range) {
  model_->SelectRange(range);
  OnCaretBoundsChanged();
  SchedulePaint();
  textfield_->GetWidget()->NotifyAccessibilityEvent(
      textfield_, ui::AccessibilityTypes::EVENT_SELECTION_CHANGED, true);
}

void NativeTextfieldViews::GetSelectionModel(gfx::SelectionModel* sel) const {
  model_->GetSelectionModel(sel);
}

void NativeTextfieldViews::SelectSelectionModel(
    const gfx::SelectionModel& sel) {
  model_->SelectSelectionModel(sel);
  OnCaretBoundsChanged();
  SchedulePaint();
}

size_t NativeTextfieldViews::GetCursorPosition() const {
  return model_->GetCursorPosition();
}

bool NativeTextfieldViews::HandleKeyPressed(const KeyEvent& e) {
  TextfieldController* controller = textfield_->GetController();
  bool handled = false;
  if (controller)
    handled = controller->HandleKeyEvent(textfield_, e);
  return handled || HandleKeyEvent(e);
}

bool NativeTextfieldViews::HandleKeyReleased(const KeyEvent& e) {
  return false;  // crbug.com/127520
}

void NativeTextfieldViews::HandleFocus() {
  GetRenderText()->set_focused(true);
  is_cursor_visible_ = true;
  SchedulePaint();
  OnCaretBoundsChanged();
  // Start blinking cursor.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NativeTextfieldViews::UpdateCursor,
                 cursor_timer_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCursorVisibleTimeMs));
}

void NativeTextfieldViews::HandleBlur() {
  GetRenderText()->set_focused(false);
  // Stop blinking cursor.
  cursor_timer_.InvalidateWeakPtrs();
  if (is_cursor_visible_) {
    is_cursor_visible_ = false;
    RepaintCursor();
  }

  if (touch_selection_controller_.get())
    touch_selection_controller_->ClientViewLostFocus();
}

ui::TextInputClient* NativeTextfieldViews::GetTextInputClient() {
  return textfield_->read_only() ? NULL : this;
}

void NativeTextfieldViews::ClearEditHistory() {
  model_->ClearEditHistory();
}

int NativeTextfieldViews::GetFontHeight() {
  return GetRenderText()->GetFont().GetHeight();
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ui::SimpleMenuModel::Delegate overrides:

bool NativeTextfieldViews::IsCommandIdChecked(int command_id) const {
  return true;
}

bool NativeTextfieldViews::IsCommandIdEnabled(int command_id) const {
  bool editable = !textfield_->read_only();
  string16 result;
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && model_->HasSelection() && !textfield_->IsObscured();
    case IDS_APP_COPY:
      return model_->HasSelection() && !textfield_->IsObscured();
    case IDS_APP_PASTE:
      ViewsDelegate::views_delegate->GetClipboard()
          ->ReadText(ui::Clipboard::BUFFER_STANDARD, &result);
      return editable && !result.empty();
    case IDS_APP_DELETE:
      return editable && model_->HasSelection();
    case IDS_APP_SELECT_ALL:
      return true;
    default:
      return textfield_->GetController()->IsCommandIdEnabled(command_id);
  }
}

bool NativeTextfieldViews::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void NativeTextfieldViews::ExecuteCommand(int command_id) {
  if (!IsCommandIdEnabled(command_id))
    return;

  bool text_changed = false;
  OnBeforeUserAction();
  switch (command_id) {
    case IDS_APP_CUT:
      text_changed = Cut();
      break;
    case IDS_APP_COPY:
      Copy();
      break;
    case IDS_APP_PASTE:
      text_changed = Paste();
      break;
    case IDS_APP_DELETE:
      text_changed = model_->Delete();
      break;
    case IDS_APP_SELECT_ALL:
      SelectAll();
      break;
    default:
      textfield_->GetController()->ExecuteCommand(command_id);
      break;
  }

  // The cursor must have changed if text changed during cut/paste/delete.
  UpdateAfterChange(text_changed, text_changed);
  OnAfterUserAction();
}

void NativeTextfieldViews::ApplyStyleRange(const gfx::StyleRange& style) {
  GetRenderText()->ApplyStyleRange(style);
  SchedulePaint();
}

void NativeTextfieldViews::ApplyDefaultStyle() {
  GetRenderText()->ApplyDefaultStyle();
  SchedulePaint();
}

void NativeTextfieldViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Set the RenderText display area.
  gfx::Insets insets = GetInsets();
  gfx::Rect display_rect(insets.left(),
                         insets.top(),
                         width() - insets.width(),
                         height() - insets.height());
  GetRenderText()->SetDisplayRect(display_rect);
  OnCaretBoundsChanged();
}

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ui::TextInputClient implementation, private:

void NativeTextfieldViews::SetCompositionText(
    const ui::CompositionText& composition) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->SetCompositionText(composition);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::ConfirmCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->ConfirmCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::ClearCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->CancelCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::InsertText(const string16& text) {
  // TODO(suzhe): Filter invalid characters.
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE || text.empty())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (GetRenderText()->insert_mode())
    model_->InsertText(GetTextForDisplay(text));
  else
    model_->ReplaceText(GetTextForDisplay(text));
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::InsertChar(char16 ch, int flags) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE ||
      !ShouldInsertChar(ch, flags)) {
    return;
  }

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (GetRenderText()->insert_mode())
    model_->InsertChar(ch);
  else
    model_->ReplaceChar(ch);
  skip_input_method_cancel_composition_ = false;

  model_->SetText(GetTextForDisplay(GetText()));

  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

ui::TextInputType NativeTextfieldViews::GetTextInputType() const {
  return textfield_->GetTextInputType();
}

bool NativeTextfieldViews::CanComposeInline() const {
  return true;
}

gfx::Rect NativeTextfieldViews::GetCaretBounds() {
  return GetRenderText()->GetUpdatedCursorBounds();
}

bool NativeTextfieldViews::HasCompositionText() {
  return model_->HasCompositionText();
}

bool NativeTextfieldViews::GetTextRange(ui::Range* range) {
  if (!ImeEditingAllowed())
    return false;

  model_->GetTextRange(range);
  return true;
}

bool NativeTextfieldViews::GetCompositionTextRange(ui::Range* range) {
  if (!ImeEditingAllowed())
    return false;

  model_->GetCompositionTextRange(range);
  return true;
}

bool NativeTextfieldViews::GetSelectionRange(ui::Range* range) {
  if (!ImeEditingAllowed())
    return false;
  GetSelectedRange(range);
  return true;
}

bool NativeTextfieldViews::SetSelectionRange(const ui::Range& range) {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;

  OnBeforeUserAction();
  SelectRange(range);
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::DeleteRange(const ui::Range& range) {
  if (!ImeEditingAllowed() || range.is_empty())
    return false;

  OnBeforeUserAction();
  model_->SelectRange(range);
  if (model_->HasSelection()) {
    model_->DeleteSelection();
    UpdateAfterChange(true, true);
  }
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::GetTextFromRange(
    const ui::Range& range,
    string16* text) {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;

  ui::Range text_range;
  if (!GetTextRange(&text_range) || !text_range.Contains(range))
    return false;

  *text = model_->GetTextFromRange(range);
  return true;
}

void NativeTextfieldViews::OnInputMethodChanged() {
  NOTIMPLEMENTED();
}

bool NativeTextfieldViews::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  NOTIMPLEMENTED();
  return false;
}

void NativeTextfieldViews::OnCompositionTextConfirmedOrCleared() {
  if (skip_input_method_cancel_composition_)
    return;
  DCHECK(textfield_->GetInputMethod());
  textfield_->GetInputMethod()->CancelComposition(textfield_);
}

gfx::RenderText* NativeTextfieldViews::GetRenderText() const {
  return model_->render_text();
}

string16 NativeTextfieldViews::GetTextForDisplay(const string16& text) {
  return textfield_->style() & Textfield::STYLE_LOWERCASE ?
      base::i18n::ToLower(text) : text;
}

void NativeTextfieldViews::UpdateCursor() {
  is_cursor_visible_ = !is_cursor_visible_;
  RepaintCursor();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NativeTextfieldViews::UpdateCursor,
                 cursor_timer_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          is_cursor_visible_ ? kCursorVisibleTimeMs : kCursorInvisibleTimeMs));
}

void NativeTextfieldViews::RepaintCursor() {
  gfx::Rect r(GetCaretBounds());
  r.Inset(-1, -1, -1, -1);
  SchedulePaintInRect(r);
}

void NativeTextfieldViews::PaintTextAndCursor(gfx::Canvas* canvas) {
  TRACE_EVENT0("views", "NativeTextfieldViews::PaintTextAndCursor");
  canvas->Save();
  GetRenderText()->set_background_is_transparent(
      !textfield_->use_default_background_color() &&
      SkColorGetA(textfield_->background_color()) != 0xFF);
  GetRenderText()->set_cursor_visible(is_drop_cursor_visible_ ||
      (is_cursor_visible_ && !model_->HasSelection()));
  GetRenderText()->set_cursor_color(
      textfield_->use_default_cursor_color() ?
          kDefaultCursorColor :
          textfield_->cursor_color());
  // Draw the text, cursor, and selection.
  GetRenderText()->Draw(canvas);

  // Draw placeholder text if needed.
  if (model_->GetText().empty() &&
      !textfield_->placeholder_text().empty()) {
    canvas->DrawStringInt(
        textfield_->placeholder_text(),
        GetRenderText()->GetFont(),
        textfield_->placeholder_text_color(),
        GetRenderText()->display_rect());
  }
  canvas->Restore();
}

bool NativeTextfieldViews::HandleKeyEvent(const KeyEvent& key_event) {
  // TODO(oshima): Refactor and consolidate with ExecuteCommand.
  if (key_event.type() == ui::ET_KEY_PRESSED) {
    ui::KeyboardCode key_code = key_event.key_code();
    // TODO(oshima): shift-tab does not work. Figure out why and fix.
    if (key_code == ui::VKEY_TAB)
      return false;

    OnBeforeUserAction();
    bool editable = !textfield_->read_only();
    bool readable = !textfield_->IsObscured();
    bool selection = key_event.IsShiftDown();
    bool control = key_event.IsControlDown();
    bool text_changed = false;
    bool cursor_changed = false;
    switch (key_code) {
      case ui::VKEY_Z:
        if (control && editable)
          cursor_changed = text_changed = model_->Undo();
        break;
      case ui::VKEY_Y:
        if (control && editable)
          cursor_changed = text_changed = model_->Redo();
        break;
      case ui::VKEY_A:
        if (control) {
          model_->SelectAll();
          cursor_changed = true;
        }
        break;
      case ui::VKEY_X:
        if (control && editable && readable)
          cursor_changed = text_changed = Cut();
        break;
      case ui::VKEY_C:
        if (control && readable)
          Copy();
        break;
      case ui::VKEY_V:
        if (control && editable)
          cursor_changed = text_changed = Paste();
        break;
      case ui::VKEY_RIGHT:
      case ui::VKEY_LEFT:
        model_->MoveCursor(
            control ? gfx::WORD_BREAK : gfx::CHARACTER_BREAK,
            (key_code == ui::VKEY_RIGHT) ? gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT,
            selection);
        cursor_changed = true;
        break;
      case ui::VKEY_END:
      case ui::VKEY_HOME:
        if ((key_code == ui::VKEY_HOME) ==
            (GetRenderText()->GetTextDirection() == base::i18n::RIGHT_TO_LEFT))
          model_->MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, selection);
        else
          model_->MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, selection);
        cursor_changed = true;
        break;
      case ui::VKEY_BACK:
      case ui::VKEY_DELETE:
        if (!editable)
          break;
        if (!model_->HasSelection()) {
          gfx::VisualCursorDirection direction = (key_code == ui::VKEY_DELETE) ?
              gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT;
          if (selection && control) {
            // If both shift and control are pressed, then erase up to the
            // beginning/end of the buffer in ChromeOS. In windows, do nothing.
#if defined(OS_WIN)
            break;
#else
            model_->MoveCursor(gfx::LINE_BREAK, direction, true);
#endif
          } else if (control) {
            // If only control is pressed, then erase the previous/next word.
            model_->MoveCursor(gfx::WORD_BREAK, direction, true);
          }
        }
        if (key_code == ui::VKEY_BACK)
          model_->Backspace();
        else
          model_->Delete();

        // We have to consume the backspace/delete keys here even if the edit
        // did not make effects.  This is to prevent further handling of the key
        // event that might have unintended side-effects.
        text_changed = true;
        break;
      case ui::VKEY_INSERT:
        GetRenderText()->ToggleInsertMode();
        cursor_changed = true;
        break;
      default:
        break;
    }

    // We must have input method in order to support text input.
    DCHECK(textfield_->GetInputMethod());

    UpdateAfterChange(text_changed, cursor_changed);
    OnAfterUserAction();
    return (text_changed || cursor_changed);
  }
  return false;
}

bool NativeTextfieldViews::MoveCursorTo(const gfx::Point& point, bool select) {
  if (!model_->MoveCursorTo(point, select))
    return false;
  OnCaretBoundsChanged();
  return true;
}

void NativeTextfieldViews::PropagateTextChange() {
  textfield_->SyncText();
}

void NativeTextfieldViews::UpdateAfterChange(bool text_changed,
                                             bool cursor_changed) {
  if (text_changed) {
    PropagateTextChange();
    textfield_->GetWidget()->NotifyAccessibilityEvent(
        textfield_, ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
  }
  if (cursor_changed) {
    is_cursor_visible_ = true;
    RepaintCursor();
    if (!text_changed) {
      // TEXT_CHANGED implies SELECTION_CHANGED, so we only need to fire
      // this if only the selection changed.
      textfield_->GetWidget()->NotifyAccessibilityEvent(
          textfield_, ui::AccessibilityTypes::EVENT_SELECTION_CHANGED, true);
    }
  }
  if (text_changed || cursor_changed) {
    OnCaretBoundsChanged();
    SchedulePaint();
  }
}

void NativeTextfieldViews::UpdateContextMenu() {
  if (!context_menu_contents_.get()) {
    context_menu_contents_.reset(new ui::SimpleMenuModel(this));
    context_menu_contents_->AddItemWithStringId(IDS_APP_CUT, IDS_APP_CUT);
    context_menu_contents_->AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
    context_menu_contents_->AddItemWithStringId(IDS_APP_PASTE, IDS_APP_PASTE);
    context_menu_contents_->AddItemWithStringId(IDS_APP_DELETE, IDS_APP_DELETE);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDS_APP_SELECT_ALL,
                                                IDS_APP_SELECT_ALL);
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->UpdateContextMenu(context_menu_contents_.get());

    context_menu_delegate_.reset(
        new views::MenuModelAdapter(context_menu_contents_.get()));
    context_menu_runner_.reset(
        new MenuRunner(new views::MenuItemView(context_menu_delegate_.get())));
  }

  context_menu_delegate_->BuildMenu(context_menu_runner_->GetMenu());
}

void NativeTextfieldViews::OnTextInputTypeChanged() {
  // TODO(suzhe): changed from DCHECK. See http://crbug.com/81320.
  if (textfield_->GetInputMethod())
    textfield_->GetInputMethod()->OnTextInputTypeChanged(textfield_);
}

void NativeTextfieldViews::OnCaretBoundsChanged() {
  // TODO(suzhe): changed from DCHECK. See http://crbug.com/81320.
  if (textfield_->GetInputMethod())
    textfield_->GetInputMethod()->OnCaretBoundsChanged(textfield_);

  // Notify selection controller
  if (!touch_selection_controller_.get())
    return;
  gfx::RenderText* render_text = GetRenderText();
  const gfx::SelectionModel& sel = render_text->selection_model();
  gfx::SelectionModel start_sel =
      render_text->GetSelectionModelForSelectionStart();
  gfx::Rect start_cursor = render_text->GetCursorBounds(start_sel, true);
  gfx::Rect end_cursor = render_text->GetCursorBounds(sel, true);
  gfx::Point start(start_cursor.x(), start_cursor.bottom() - 1);
  gfx::Point end(end_cursor.x(), end_cursor.bottom() - 1);
  touch_selection_controller_->SelectionChanged(start, end);
}

void NativeTextfieldViews::OnBeforeUserAction() {
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnBeforeUserAction(textfield_);
}

void NativeTextfieldViews::OnAfterUserAction() {
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnAfterUserAction(textfield_);
}

bool NativeTextfieldViews::Cut() {
  if (model_->Cut()) {
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->OnAfterCutOrCopy();
    return true;
  }
  return false;
}

bool NativeTextfieldViews::Copy() {
  if (model_->Copy()) {
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->OnAfterCutOrCopy();
    return true;
  }
  return false;
}

bool NativeTextfieldViews::Paste() {
  const string16 original_text = GetText();
  const bool success = model_->Paste();

  if (success) {
    // As Paste is handled in model_->Paste(), the RenderText may contain
    // upper case characters. This is not consistent with other places
    // which keeps RenderText only containing lower case characters.
    string16 new_text = GetTextForDisplay(GetText());
    model_->SetText(new_text);

    // Calls TextfieldController::ContentsChanged() explicitly if the paste
    // action did not change the content at all. See http://crbug.com/79002
    if (new_text == original_text) {
      TextfieldController* controller = textfield_->GetController();
      if (controller)
        controller->ContentsChanged(textfield_, textfield_->text());
    }
  }
  return success;
}

void NativeTextfieldViews::TrackMouseClicks(const MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    base::TimeDelta time_delta = event.time_stamp() - last_click_time_;
    if (time_delta.InMilliseconds() <= GetDoubleClickInterval() &&
        !ExceededDragThresholdFromLastClickLocation(event)) {
      aggregated_clicks_ = (aggregated_clicks_ + 1) % 3;
    } else {
      aggregated_clicks_ = 0;
    }
    last_click_time_ = event.time_stamp();
    last_click_location_ = event.location();
  }
}

void NativeTextfieldViews::HandleMousePressEvent(const MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    textfield_->RequestFocus();

    initiating_drag_ = false;
    bool can_drag = true;

    switch (aggregated_clicks_) {
      case 0:
        if (can_drag && GetRenderText()->IsPointInSelection(event.location()))
          initiating_drag_ = true;
        else
          MoveCursorTo(event.location(), event.IsShiftDown());
        break;
      case 1:
        model_->SelectWord();
        OnCaretBoundsChanged();
        break;
      case 2:
        model_->SelectAll();
        OnCaretBoundsChanged();
        break;
      default:
        NOTREACHED();
    }
    SchedulePaint();
  }
}

bool NativeTextfieldViews::ImeEditingAllowed() const {
  // We don't allow the input method to retrieve or delete content from a
  // password field.
  ui::TextInputType t = GetTextInputType();
  return (t != ui::TEXT_INPUT_TYPE_NONE && t != ui::TEXT_INPUT_TYPE_PASSWORD);
}

// static
bool NativeTextfieldViews::ShouldInsertChar(char16 ch, int flags) {
  // Filter out all control characters, including tab and new line characters,
  // and all characters with Alt modifier. But we need to allow characters with
  // AltGr modifier.
  // On Windows AltGr is represented by Alt+Ctrl, and on Linux it's a different
  // flag that we don't care about.
  return ((ch >= 0x20 && ch < 0x7F) || ch > 0x9F) &&
      (flags & ~(ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN)) != ui::EF_ALT_DOWN;
}

#if defined(USE_AURA)
// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  return new NativeTextfieldViews(field);
}
#endif

}  // namespace views
